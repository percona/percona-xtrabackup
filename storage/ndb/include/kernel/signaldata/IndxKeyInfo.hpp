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

#ifndef INDX_KEY_INFO_HPP
#define INDX_KEY_INFO_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 203


class IndxKeyInfo {
  /**
   * Sender(s)
   */
  friend class NdbIndexOperation;

  /**
   * Reciver(s)
   */
  friend class Dbtc;
  
  friend bool printINDXKEYINFO(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( HeaderLength = 3 );
  STATIC_CONST( DataLength = 20 );
  STATIC_CONST( MaxSignalLength = HeaderLength + DataLength );

  // Public methods
public:
 Uint32* getData() const;

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 keyData[DataLength];
};

inline
Uint32* IndxKeyInfo::getData() const
{
  return (Uint32*)&keyData[0];
}


#undef JAM_FILE_ID

#endif
