//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_SOME_ROWS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_SOME_ROWS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

class read_some_rows_algo
{
    diagnostics* diag_;
    execution_processor* proc_;
    output_ref output_;

    struct state_t
    {
        int resume_point{0};
        std::size_t rows_read{0};
    } state_;

    BOOST_ATTRIBUTE_NODISCARD static std::pair<error_code, std::size_t> process_some_rows(
        connection_state_data& st,
        execution_processor& proc,
        output_ref output,
        diagnostics& diag
    )
    {
        // Process all read messages until they run out, an error happens
        // or an EOF is received
        std::size_t read_rows = 0;
        error_code err;
        proc.on_row_batch_start();
        while (true)
        {
            // Check for errors (like seqnum mismatches)
            if (st.reader.error())
                return {st.reader.error(), read_rows};

            // Get the row message
            auto buff = st.reader.message();

            // Deserialize it
            auto res = deserialize_row_message(buff, st.flavor, diag);
            if (res.type == row_message::type_t::error)
            {
                err = res.data.err;
            }
            else if (res.type == row_message::type_t::row)
            {
                output.set_offset(read_rows);
                err = proc.on_row(res.data.row, output, st.shared_fields);
                if (!err)
                    ++read_rows;
            }
            else
            {
                st.backslash_escapes = res.data.ok_pack.backslash_escapes();
                err = proc.on_row_ok_packet(res.data.ok_pack);
            }

            if (err)
                return {err, read_rows};

            // TODO: can we make this better?
            if (!proc.is_reading_rows() || read_rows >= output.max_size())
                break;

            // Attempt to parse the next message
            st.reader.prepare_read(proc.sequence_number());
            if (!st.reader.done())
                break;
        }
        proc.on_row_batch_finish();
        return {error_code(), read_rows};
    }

public:
    read_some_rows_algo(diagnostics& diag, read_some_rows_algo_params params) noexcept
        : diag_(&diag), proc_(params.proc), output_(params.output)
    {
    }

    void reset() { state_ = state_t{}; }

    const execution_processor& processor() const { return *proc_; }
    execution_processor& processor() { return *proc_; }

    next_action resume(connection_state_data& st, error_code ec)
    {
        if (ec)
            return ec;

        switch (state_.resume_point)
        {
        case 0:

            // Clear diagnostics
            diag_->clear();

            // Clear any previous use of shared fields.
            // Required for the dynamic version to work.
            st.shared_fields.clear();

            // If we are not reading rows, return
            if (!processor().is_reading_rows())
                return next_action();

            // Read at least one message. Keep parsing state, in case a previous message
            // was parsed partially
            BOOST_MYSQL_YIELD(state_.resume_point, 1, st.read(proc_->sequence_number(), true))

            // Process messages
            std::tie(ec, state_.rows_read) = process_some_rows(st, *proc_, output_, *diag_);
            return ec;
        }

        return next_action();
    }

    std::size_t result(const connection_state_data&) const { return state_.rows_read; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
