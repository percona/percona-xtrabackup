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

#ifndef CNTR_MASTERREQ_HPP
#define CNTR_MASTERREQ_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 32


/**
 * This signals is sent by NdbCntr-Master to NdbCntr
 */
class CntrMasterReq {
  /**
   * Sender(s)
   */
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  /**
   * Reciver(s)
   */
  
public:
  STATIC_CONST( SignalLength = 4 + NdbNodeBitmask::Size );
private:
  
  Uint32 userBlockRef;
  Uint32 userNodeId;
  Uint32 typeOfStart;
  Uint32 noRestartNodes;
  Uint32 theNodes[NdbNodeBitmask::Size];
};


#undef JAM_FILE_ID

#endif
