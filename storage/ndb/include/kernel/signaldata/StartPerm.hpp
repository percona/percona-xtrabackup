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

#ifndef START_PERM_REQ_HPP
#define START_PERM_REQ_HPP

#define JAM_FILE_ID 204


/**
 * This signal is sent by starting DIH to master DIH
 *
 * Used when starting in an already started cluster
 *
 */
class StartPermReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 startType;  
};

class StartPermConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 startingNodeId;
  Uint32 systemFailureNo;  
  Uint32 microGCP;
};

class StartPermRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 2 );
private:
  
  Uint32 startingNodeId;
  Uint32 errorCode;  

  enum ErrorCode
  {
    ZNODE_ALREADY_STARTING_ERROR = 305,
    ZNODE_START_DISALLOWED_ERROR = 309,
    InitialStartRequired = 320
  };
};

#undef JAM_FILE_ID

#endif
