/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RUNNER_HPP
#define BOOST_REDIS_RUNNER_HPP

#include <boost/redis/detail/health_checker.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/detail/connector.hpp>
#include <boost/redis/detail/resolver.hpp>
#include <boost/redis/detail/handshaker.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/prepend.hpp>
#include <string>
#include <memory>
#include <chrono>

namespace boost::redis::detail
{

void push_hello(config const& cfg, request& req);

template <class Runner, class Connection, class Logger>
struct hello_op {
   Runner* runner_ = nullptr;
   Connection* conn_ = nullptr;
   Logger logger_;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         runner_->add_hello();

         BOOST_ASIO_CORO_YIELD
         conn_->async_exec(runner_->hello_req_, runner_->hello_resp_, std::move(self));
         logger_.on_hello(ec, runner_->hello_resp_);

         if (ec || runner_->has_error_in_response() || is_cancelled(self)) {
            logger_.trace("hello-op: error/canceled. Exiting ...");
            conn_->cancel(operation::run);
            self.complete(!!ec ? ec : asio::error::operation_aborted);
            return;
         }

         self.complete({});
      }
   }
};

template <class Runner, class Connection, class Logger>
class runner_op {
private:
   Runner* runner_ = nullptr;
   Connection* conn_ = nullptr;
   Logger logger_;
   asio::coroutine coro_{};

public:
   runner_op(Runner* runner, Connection* conn, Logger l)
   : runner_{runner}
   , conn_{conn}
   , logger_{l}
   {}

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 3> order = {}
                  , system::error_code ec0 = {}
                  , system::error_code ec1 = {}
                  , system::error_code ec2 = {}
                  , std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         BOOST_ASIO_CORO_YIELD
         runner_->resv_.async_resolve(
            asio::prepend(std::move(self), std::array<std::size_t, 3> {}));

         logger_.on_resolve(ec0, runner_->resv_.results());

         if (ec0 || redis::detail::is_cancelled(self)) {
            self.complete(!!ec0 ? ec0 : asio::error::operation_aborted);
            return;
         }

         BOOST_ASIO_CORO_YIELD
         runner_->ctor_.async_connect(
            conn_->next_layer().next_layer(),
            runner_->resv_.results(),
            asio::prepend(std::move(self), std::array<std::size_t, 3> {}));

         logger_.on_connect(ec0, runner_->ctor_.endpoint());

         if (ec0 || redis::detail::is_cancelled(self)) {
            self.complete(!!ec0 ? ec0 : asio::error::operation_aborted);
            return;
         }

         if (conn_->use_ssl()) {
            BOOST_ASIO_CORO_YIELD
            runner_->hsher_.async_handshake(
               conn_->next_layer(),
               asio::prepend(std::move(self),
                  std::array<std::size_t, 3> {}));

            logger_.on_ssl_handshake(ec0);
            if (ec0 || redis::detail::is_cancelled(self)) {
               self.complete(!!ec0 ? ec0 : asio::error::operation_aborted);
               return;
            }
         }

         // Note: Oder is important here because async_run might
         // trigger an async_write before the async_hello thereby
         // causing authentication problems.
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token) { return runner_->async_hello(*conn_, logger_, token); },
            [this](auto token) { return runner_->health_checker_.async_check_health(*conn_, logger_, token); },
            [this](auto token) { return conn_->async_run_lean(runner_->cfg_, logger_, token); }
         ).async_wait(
            asio::experimental::wait_for_one_error(),
            std::move(self));

         logger_.on_runner(ec0, ec1, ec2);

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         if (order[0] == 0 && !!ec0) {
            self.complete(ec0);
            return;
         }

         if (order[0] == 1 && ec1 == error::pong_timeout) {
            self.complete(ec1);
            return;
         }

         self.complete(ec2);
      }
   }
};

template <class Executor>
class runner {
public:
   runner(Executor ex, config cfg)
   : resv_{ex}
   , hsher_{ex}
   , health_checker_{ex}
   , cfg_{cfg}
   { }

   std::size_t cancel(operation op)
   {
      resv_.cancel(op);
      hsher_.cancel(op);
      health_checker_.cancel(op);
      return 0U;
   }

   void set_config(config const& cfg)
   {
      cfg_ = cfg;
      resv_.set_config(cfg);
      ctor_.set_config(cfg);
      hsher_.set_config(cfg);
      health_checker_.set_config(cfg);
   }

   template <class Connection, class Logger, class CompletionToken>
   auto async_run(Connection& conn, Logger l, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(runner_op<runner, Connection, Logger>{this, &conn, l}, token, conn);
   }

   config const& get_config() const noexcept {return cfg_;}

private:
   using resolver_type = resolver<Executor>;
   using handshaker_type = detail::handshaker<Executor>;
   using health_checker_type = health_checker<Executor>;

   template <class, class, class> friend class runner_op;
   template <class, class, class> friend struct hello_op;

   template <class Connection, class Logger, class CompletionToken>
   auto async_hello(Connection& conn, Logger l, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(hello_op<runner, Connection, Logger>{this, &conn, l}, token, conn);
   }

   void add_hello()
   {
      hello_req_.clear();
      if (hello_resp_.has_value())
         hello_resp_.value().clear();
      push_hello(cfg_, hello_req_);
   }

   bool has_error_in_response() const noexcept
   {
      if (!hello_resp_.has_value())
         return true;

      auto f = [](auto const& e)
      {
         switch (e.data_type) {
            case resp3::type::simple_error:
            case resp3::type::blob_error: return true;
            default: return false;
         }
      };

      return std::any_of(std::cbegin(hello_resp_.value()), std::cend(hello_resp_.value()), f);
   }

   resolver_type resv_;
   connector ctor_;
   handshaker_type hsher_;
   health_checker_type health_checker_;
   request hello_req_;
   generic_response hello_resp_;
   config cfg_;
};

} // boost::redis::detail

#endif // BOOST_REDIS_RUNNER_HPP
