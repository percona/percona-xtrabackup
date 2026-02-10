/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESOLVER_HPP
#define BOOST_REDIS_RESOLVER_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/error.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/cancel_after.hpp>
#include <string>
#include <chrono>

namespace boost::redis::detail
{

template <class Resolver>
struct resolve_op {
   Resolver* resv_ = nullptr;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , asio::ip::tcp::resolver::results_type res = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         resv_->resv_.async_resolve(
            resv_->addr_.host,
            resv_->addr_.port,
            asio::cancel_after(resv_->timeout_, std::move(self)));

         resv_->results_ = res;

         if (ec == asio::error::operation_aborted) {
            self.complete(error::resolve_timeout);
         } else {
            self.complete(ec);
         }
      }
   }
};

template <class Executor>
class resolver {
public:
   resolver(Executor ex) : resv_{ex} {}

   template <class CompletionToken>
   auto async_resolve(CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(resolve_op<resolver>{this}, token, resv_);
   }

   std::size_t cancel(operation op)
   {
      switch (op) {
         case operation::resolve:
         case operation::all:
            resv_.cancel();
            break;
         default: /* ignore */;
      }

      return 0;
   }

   auto const& results() const noexcept
      { return results_;}

   void set_config(config const& cfg)
   {
      addr_ = cfg.addr;
      timeout_ = cfg.resolve_timeout;
   }

private:
   using resolver_type = asio::ip::basic_resolver<asio::ip::tcp, Executor>;
   template <class> friend struct resolve_op;

   resolver_type resv_;
   address addr_;
   std::chrono::steady_clock::duration timeout_;
   asio::ip::tcp::resolver::results_type results_;
};

} // boost::redis::detail

#endif // BOOST_REDIS_RESOLVER_HPP
