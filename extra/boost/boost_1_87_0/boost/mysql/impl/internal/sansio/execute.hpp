//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_EXECUTE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_EXECUTE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_resultset_head.hpp>
#include <boost/mysql/impl/internal/sansio/read_some_rows.hpp>
#include <boost/mysql/impl/internal/sansio/start_execution.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_execute_response_algo
{
    int resume_point_{0};
    read_resultset_head_algo read_head_st_;
    read_some_rows_algo read_some_rows_st_;

public:
    read_execute_response_algo(diagnostics& diag, execution_processor* proc) noexcept
        : read_head_st_(diag, {proc}), read_some_rows_st_(diag, {proc, output_ref()})
    {
    }

    diagnostics& diag() { return read_head_st_.diag(); }
    execution_processor& processor() { return read_head_st_.processor(); }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            while (!processor().is_complete())
            {
                if (processor().is_reading_head())
                {
                    read_head_st_.reset();
                    while (!(act = read_head_st_.resume(st, ec)).is_done())
                        BOOST_MYSQL_YIELD(resume_point_, 1, act)
                    if (act.error())
                        return act;
                }
                else if (processor().is_reading_rows())
                {
                    read_some_rows_st_.reset();
                    while (!(act = read_some_rows_st_.resume(st, ec)).is_done())
                        BOOST_MYSQL_YIELD(resume_point_, 2, act)
                    if (act.error())
                        return act;
                }
            }
        }

        return next_action();
    }
};

class execute_algo
{
    int resume_point_{0};
    start_execution_algo start_execution_st_;
    read_execute_response_algo read_response_st_;

    diagnostics& diag() { return read_response_st_.diag(); }
    execution_processor& processor() { return read_response_st_.processor(); }

public:
    execute_algo(diagnostics& diag, execute_algo_params params) noexcept
        : start_execution_st_(diag, {params.req, params.proc}), read_response_st_(diag, params.proc)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Send request and read the first response
            while (!(act = start_execution_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 1, act)
            if (act.error())
                return act;

            // Read anything else
            while (!(act = read_response_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
