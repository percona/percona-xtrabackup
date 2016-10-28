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


#include <kernel_types.h>
#include <BlockNumbers.h>
#include <signaldata/CloseComReqConf.hpp>

bool
printCLOSECOMREQCONF(FILE * output, 
		     const Uint32 * theData, 
		     Uint32 len, 
		     Uint16 receiverBlockNo){

  CloseComReqConf * cc = (CloseComReqConf*)theData;

  if (len == 1)
  {
    fprintf(output, " xxxBlockRef = (%d, %d)\n",
            refToBlock(cc->xxxBlockRef),
            refToNode(cc->xxxBlockRef));
  }
  else
  {
    fprintf(output, " xxxBlockRef = (%d, %d) failNo = %d noOfNodes = %d\n",
            refToBlock(cc->xxxBlockRef), refToNode(cc->xxxBlockRef),
            cc->failNo, cc->noOfNodes);

    int hits = 0;
    fprintf(output, " Nodes: ");
    for(int i = 0; i<MAX_NODES; i++){
      if(NodeBitmask::get(cc->theNodes, i)){
        hits++;
        fprintf(output, " %d", i);
      }
      if(hits == 16){
        fprintf(output, "\n Nodes: ");
        hits = 0;
      }
    }
    if(hits != 0)
      fprintf(output, "\n");
  }

  return true;
}


