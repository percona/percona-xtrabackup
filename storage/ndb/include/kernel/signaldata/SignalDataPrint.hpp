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

#ifndef SIGNAL_DATA_PRINT_H
#define SIGNAL_DATA_PRINT_H

#include <ndb_global.h>
#include <kernel_types.h>

#define JAM_FILE_ID 41


/**
 * Typedef for a Signal Data Print Function
 */
typedef bool (* SignalDataPrintFunction)(FILE * output, const Uint32 * theData, Uint32 len, BlockNumber receiverBlockNo);

struct NameFunctionPair {
  GlobalSignalNumber gsn;
  SignalDataPrintFunction function;
};

extern const NameFunctionPair SignalDataPrintFunctions[];
extern const unsigned short   NO_OF_PRINT_FUNCTIONS;


#undef JAM_FILE_ID

#endif
