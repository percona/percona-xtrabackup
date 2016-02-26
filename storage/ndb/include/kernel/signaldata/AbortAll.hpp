/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ABORT_ALL_REQ_HPP
#define ABORT_ALL_REQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 141


class AbortAllReq {

  /**
   * Reciver(s)
   */
  friend class Dbtc;

  /**
   * Sender
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 2 );
  
public:
  
  Uint32 senderRef;
  Uint32 senderData;
};

class AbortAllConf {
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  
  /**
   * Sender
   */
  friend class Dbtc;

public:
  STATIC_CONST( SignalLength = 1 );
  
public:
  Uint32 senderData;
};

class AbortAllRef {
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  
  /**
   * Sender
   */
  friend class Dbtc;

public:
  STATIC_CONST( SignalLength = 2 );
  
  enum ErrorCode {
    InvalidState = 1,
    AbortAlreadyInProgress = 2,
    FunctionNotImplemented = 3
  };
public:
  Uint32 senderData;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif

