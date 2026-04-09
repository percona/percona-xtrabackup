//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RESET_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RESET_CONNECTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_reset_connection_response_algo
{
    int resume_point_{0};
    diagnostics* diag_;
    std::uint8_t seqnum_{0};

public:
    read_reset_connection_response_algo(diagnostics& diag, std::uint8_t seqnum) noexcept
        : diag_(&diag), seqnum_(seqnum)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        switch (resume_point_)
        {
        case 0:

            // Read the reset response
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(seqnum_))
            if (ec)
                return ec;

            // Verify it's what we expected
            ec = st.deserialize_ok(*diag_);
            if (!ec)
            {
                // Reset was successful. Resetting changes the connection's character set
                // to the server's default, which is an unknown value that doesn't have to match
                // what was specified in handshake. As a safety measure, clear the current charset
                st.current_charset = character_set{};
            }

            // Done
        }

        return ec;
    }
};

inline run_pipeline_algo_params setup_reset_connection_pipeline(connection_state_data& st)
{
    // reset_connection request is fixed size and small, so we don't enforce any buffer limit
    st.write_buffer.clear();
    st.shared_pipeline_stages[0] = {
        pipeline_stage_kind::reset_connection,
        serialize_top_level_checked(reset_connection_command{}, st.write_buffer),
        {}
    };
    return {
        st.write_buffer,
        {st.shared_pipeline_stages.data(), 1},
        nullptr
    };
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
