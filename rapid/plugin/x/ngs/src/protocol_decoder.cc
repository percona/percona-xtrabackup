// Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; version 2 of the
// License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
// 02110-1301  USA

#include "ngs/protocol_decoder.h"
#include "ngs/log.h"
#include "ngs/ngs_error.h"
#include "ngs_common/protocol_protobuf.h"


using namespace ngs;

Message *Message_decoder::alloc_message(int8_t type, Error_code &ret_error, bool &ret_shared)
{
  try
  {
    Message *msg = NULL;
    ret_shared = true;
    switch ((Mysqlx::ClientMessages::Type)type)
    {
      case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
        msg = new Mysqlx::Connection::CapabilitiesGet();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
        msg = new Mysqlx::Connection::CapabilitiesSet();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::CON_CLOSE:
        msg = new Mysqlx::Connection::Close();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_CLOSE:
        msg = new Mysqlx::Session::Close();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_RESET:
        msg = new Mysqlx::Session::Reset();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
        msg = new Mysqlx::Session::AuthenticateStart();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE:
        msg = new Mysqlx::Session::AuthenticateContinue();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
        msg = &m_stmt_execute;
        break;
      case Mysqlx::ClientMessages::CRUD_FIND:
        msg = &m_crud_find;
        break;
      case Mysqlx::ClientMessages::CRUD_INSERT:
        msg = &m_crud_insert;
        break;
      case Mysqlx::ClientMessages::CRUD_UPDATE:
        msg = &m_crud_update;
        break;
      case Mysqlx::ClientMessages::CRUD_DELETE:
        msg = &m_crud_delete;
        break;
      case Mysqlx::ClientMessages::EXPECT_OPEN:
        msg = &m_expect_open;
        break;
      case Mysqlx::ClientMessages::EXPECT_CLOSE:
        msg = &m_expect_close;
        break;

      default:
        log_debug("Cannot decode message of unknown type %i", type);
        ret_error = Error_code(ER_X_BAD_MESSAGE, "Invalid message type");
        break;
    }
    return msg;
  }
  catch (std::bad_alloc&)
  {
    ret_error = Error_code(ER_OUTOFMEMORY, "Out of memory");
  }
  return NULL;
}


Error_code Message_decoder::parse(Request &request)
{
  bool msg_is_shared;
  Error_code ret_error;
  Message *message = alloc_message(request.get_type(), ret_error, msg_is_shared);
  if (message)
  {
    std::string &buffer(request.buffer());
    // feed the data to the command (up to the specified boundary)
    google::protobuf::io::CodedInputStream stream(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<int>(buffer.length()));
    // variable 'mysqlx_max_allowed_packet' has been checked when buffer was filling by data
    stream.SetTotalBytesLimit(static_cast<int>(buffer.length()), -1 /*no warnings*/);
    message->ParseFromCodedStream(&stream);

    if (!message->IsInitialized())
    {
      log_debug("Error parsing message of type %i: %s",
                request.get_type(), message->InitializationErrorString().c_str());
      if (!msg_is_shared)
        delete message;
      message = NULL;
      return Error_code(ER_X_BAD_MESSAGE, "Parse error unserializing protobuf message");
    }
    else
      request.set_parsed_message(message, !msg_is_shared);
  }
  return Success();
}
