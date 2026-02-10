/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTOR_HPP
#define BOOST_REDIS_CONNECTOR_HPP

#include <boost/redis/detail/helper.hpp>
#include <boost/redis/error.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/cancel_after.hpp>
#include <string>
#include <chrono>

namespace boost::redis::detail
{

template <class Connector, class Stream>
struct connect_op {
   Connector* ctor_ = nullptr;
   Stream* stream = nullptr;
   asio::ip::tcp::resolver::results_type const* res_ = nullptr;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , system::error_code const& ec = {}
                  , asio::ip::tcp::endpoint const& ep= {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         asio::async_connect(*stream, *res_,
            [](system::error_code const&, auto const&) { return true; },
            asio::cancel_after(ctor_->timeout_, std::move(self)));

         ctor_->endpoint_ = ep;

         if (ec == asio::error::operation_aborted) {
            self.complete(redis::error::connect_timeout);
         } else {
            self.complete(ec);
         }
      }
   }
};

class connector {
public:
   void set_config(config const& cfg)
      { timeout_ = cfg.connect_timeout; }

   template <class Stream, class CompletionToken>
   auto
   async_connect(
         Stream& stream,
         asio::ip::tcp::resolver::results_type const& res,
         CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(connect_op<connector, Stream>{this, &stream, &res}, token);
   }

   auto const& endpoint() const noexcept { return endpoint_;}

private:
   template <class, class> friend struct connect_op;

   std::chrono::steady_clock::duration timeout_ = std::chrono::seconds{2};
   asio::ip::tcp::endpoint endpoint_;
};

} // boost::redis::detail

#endif // BOOST_REDIS_CONNECTOR_HPP
