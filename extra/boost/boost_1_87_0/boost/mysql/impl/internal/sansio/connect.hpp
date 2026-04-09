//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/handshake.hpp>

namespace boost {
namespace mysql {
namespace detail {

class connect_algo
{
    int resume_point_{0};
    handshake_algo handshake_;
    error_code stored_ec_;

public:
    connect_algo(diagnostics& diag, connect_algo_params params) noexcept
        : handshake_(diag, {params.hparams, params.secure_channel})
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            handshake_.diag().clear();

            // Physical connect
            BOOST_MYSQL_YIELD(resume_point_, 1, next_action::connect())
            if (ec)
                return ec;

            // Handshake
            while (!(act = handshake_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)

            // If handshake failed, close the stream ignoring the result
            // and return handshake's error code
            if (act.error())
            {
                stored_ec_ = act.error();
                BOOST_MYSQL_YIELD(resume_point_, 3, next_action::close())
                return stored_ec_;
            }

            // Done
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
