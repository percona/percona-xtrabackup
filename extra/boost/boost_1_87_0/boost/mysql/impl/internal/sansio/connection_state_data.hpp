//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

enum class ssl_state
{
    unsupported,
    inactive,
    active,
    torn_down,
};

struct connection_state_data
{
    // Is the connection actually connected? Set by handshake
    bool is_connected{false};

    // Are we talking to MySQL or MariaDB?
    db_flavor flavor{db_flavor::mysql};

    // What are the connection's capabilities?
    capabilities current_capabilities;

    // Used by async ops without output diagnostics params, to avoid allocations
    diagnostics shared_diag;

    // Temporary field storage, re-used by several ops
    std::vector<field_view> shared_fields;

    // Temporary pipeline stage storage, re-used by several ops
    std::array<pipeline_request_stage, 2> shared_pipeline_stages;

    // Do we want to retain metadata strings or not? Used to save allocations
    metadata_mode meta_mode{metadata_mode::minimal};

    // Is SSL supported/enabled for the current connection?
    ssl_state ssl;

    // Do backslashes represent escape sequences? By default they do, but they can
    // be disabled using a variable. OK packets include a flag with this info.
    bool backslash_escapes{true};

    // The current character set, or a default-constructed character set (will all nullptrs) if unknown
    character_set current_charset{};

    // The write buffer
    std::vector<std::uint8_t> write_buffer;

    // Reader
    message_reader reader;

    std::size_t max_buffer_size() const { return reader.max_buffer_size(); }
    bool ssl_active() const { return ssl == ssl_state::active; }
    bool supports_ssl() const { return ssl != ssl_state::unsupported; }

    connection_state_data(
        std::size_t read_buffer_size,
        std::size_t max_buff_size = static_cast<std::size_t>(-1),
        bool transport_supports_ssl = false
    )
        : ssl(transport_supports_ssl ? ssl_state::inactive : ssl_state::unsupported),
          reader(read_buffer_size, max_buff_size)
    {
    }

    void reset()
    {
        is_connected = false;
        flavor = db_flavor::mysql;
        current_capabilities = capabilities();
        // Metadata mode does not get reset on handshake
        reader.reset();
        // Writer does not need reset, since every write clears previous state
        if (supports_ssl())
            ssl = ssl_state::inactive;
        backslash_escapes = true;
        current_charset = character_set{};
    }

    // Reads an OK packet from the reader. This operation is repeated in several places.
    error_code deserialize_ok(diagnostics& diag)
    {
        return deserialize_ok_response(reader.message(), flavor, diag, backslash_escapes);
    }

    // Helpers for sans-io algorithms
    next_action read(std::uint8_t& seqnum, bool keep_parsing_state = false)
    {
        // buffer is attached by top_level_algo
        reader.prepare_read(seqnum, keep_parsing_state);
        return next_action::read({});
    }

    template <class Serializable>
    next_action write(const Serializable& msg, std::uint8_t& seqnum)
    {
        // use_ssl is attached by top_level_algo
        write_buffer.clear();
        auto res = serialize_top_level(msg, write_buffer, seqnum, max_buffer_size());
        if (res.err)
            return res.err;
        seqnum = res.seqnum;
        return next_action::write({write_buffer, false});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
