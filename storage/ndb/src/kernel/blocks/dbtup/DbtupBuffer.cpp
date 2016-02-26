/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUP_C
#define DBTUP_BUFFER_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TransIdAI.hpp>

#define JAM_FILE_ID 410


void Dbtup::execSEND_PACKED(Signal* signal)
{
  Uint16 hostId;
  Uint32 i;
  Uint32 TpackedListIndex= cpackedListIndex;
  bool present = false;
  jamEntry();
  for (i= 0; i < TpackedListIndex; i++) {
    jam();
    hostId= cpackedList[i];
    ndbrequire((hostId - 1) < (MAX_NODES - 1)); // Also check not zero
    Uint32 TpacketTA= hostBuffer[hostId].noOfPacketsTA;
    if (TpacketTA != 0) {
      jam();

      if (ERROR_INSERTED(4037))
      {
        /* Delay a SEND_PACKED signal for 10 calls to execSEND_PACKED */
        jam();
        if (!present)
        {
          /* First valid packed data in this pass */
          jam();
          present = true;
          cerrorPackedDelay++;
          
          if ((cerrorPackedDelay % 10) != 0)
          {
            /* Skip it */
            jam();
            return;
          }
        }
      }
      BlockReference TBref= numberToRef(API_PACKED, hostId);
      Uint32 TpacketLen= hostBuffer[hostId].packetLenTA;
      MEMCOPY_NO_WORDS(&signal->theData[0],
                       &hostBuffer[hostId].packetBufferTA[0],
                       TpacketLen);
      sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
      hostBuffer[hostId].noOfPacketsTA= 0;
      hostBuffer[hostId].packetLenTA= 0;
    }
    hostBuffer[hostId].inPackedList= false;
  }//for
  cpackedListIndex= 0;
}

void Dbtup::bufferTRANSID_AI(Signal* signal, BlockReference aRef,
                             Uint32 Tlen)
{
  if (Tlen == 3)
    return;
  
  Uint32 hostId= refToNode(aRef);
  Uint32 Theader= ((refToBlock(aRef) << 16)+(Tlen-3));
  
  ndbrequire(hostId < MAX_NODES);
  Uint32 TpacketLen= hostBuffer[hostId].packetLenTA;
  Uint32 TnoOfPackets= hostBuffer[hostId].noOfPacketsTA;
  Uint32 sig0= signal->theData[0];
  Uint32 sig1= signal->theData[1];
  Uint32 sig2= signal->theData[2];

  BlockReference TBref= numberToRef(API_PACKED, hostId);

  if ((Tlen + TpacketLen + 1) <= 25) {
// ----------------------------------------------------------------
// There is still space in the buffer. We will copy it into the
// buffer.
// ----------------------------------------------------------------
    jam();
    updatePackedList(signal, hostId);
  } else if (false && TnoOfPackets == 1) {
// ----------------------------------------------------------------
// The buffer is full and there was only one packet buffered. We
// will send this as a normal signal.
// ----------------------------------------------------------------
    Uint32 TnewRef= numberToRef((hostBuffer[hostId].packetBufferTA[0] >> 16),
                                 hostId);
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferTA[1],
                     TpacketLen - 1);
    sendSignal(TnewRef, GSN_TRANSID_AI, signal, (TpacketLen - 1), JBB);
    TpacketLen= 0;
    TnoOfPackets= 0;
  } else {
// ----------------------------------------------------------------
// The buffer is full but at least two packets. Send those in
// packed form.
// ----------------------------------------------------------------
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferTA[0],
                     TpacketLen);
    sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
    TpacketLen= 0;
    TnoOfPackets= 0;
  }
// ----------------------------------------------------------------
// Copy the signal into the buffer
// ----------------------------------------------------------------
  hostBuffer[hostId].packetBufferTA[TpacketLen + 0]= Theader;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 1]= sig0;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 2]= sig1;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 3]= sig2;
  hostBuffer[hostId].noOfPacketsTA= TnoOfPackets + 1;
  hostBuffer[hostId].packetLenTA= Tlen + TpacketLen + 1;
  MEMCOPY_NO_WORDS(&hostBuffer[hostId].packetBufferTA[TpacketLen + 4],
                   &signal->theData[25],
                   Tlen - 3);
}

void Dbtup::updatePackedList(Signal* signal, Uint16 hostId)
{
  if (hostBuffer[hostId].inPackedList == false) {
    Uint32 TpackedListIndex= cpackedListIndex;
    jam();
    hostBuffer[hostId].inPackedList= true;
    cpackedList[TpackedListIndex]= hostId;
    cpackedListIndex= TpackedListIndex + 1;
  }
}

/* ---------------------------------------------------------------- */
/* ----------------------- SEND READ ATTRINFO --------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::sendReadAttrinfo(Signal* signal,
                             KeyReqStruct *req_struct,
                             Uint32 ToutBufIndex)
{
  if(ToutBufIndex == 0)
    return;
  
  const BlockReference recBlockref= req_struct->rec_blockref;
  const Uint32 nodeId= refToNode(recBlockref);

  bool connectedToNode= getNodeInfo(nodeId).m_connected;
  const Uint32 type= getNodeInfo(nodeId).m_type;
  bool is_api= (type >= NodeInfo::API && type <= NodeInfo::MGM);
  bool old_dest= (getNodeInfo(nodeId).m_version < MAKE_VERSION(6,4,0));
  Uint32 TpacketLen= hostBuffer[nodeId].packetLenTA;
  Uint32 TpacketTA= hostBuffer[nodeId].noOfPacketsTA;

  if (ERROR_INSERTED(4006) && (nodeId != getOwnNodeId())){
    // Use error insert to turn routing on
    jam();
    connectedToNode= false;    
  }

  Uint32 sig0= req_struct->tc_operation_ptr;
  Uint32 sig1= req_struct->trans_id1;
  Uint32 sig2= req_struct->trans_id2;
  
  TransIdAI * transIdAI=  (TransIdAI *)signal->getDataPtrSend();
  transIdAI->connectPtr= sig0;
  transIdAI->transId[0]= sig1;
  transIdAI->transId[1]= sig2;
  
  const Uint32 routeBlockref= req_struct->TC_ref;
  /**
   * If we are not connected to the destination block, we may reach it 
   * indirectly by sending a TRANSID_AI_R signal to routeBlockref. Only
   * TC can handle TRANSID_AI_R signals. The 'ndbrequire' below should
   * check that there is no chance of sending TRANSID_AI_R to a block
   * that cannot handle it.
   */
  ndbrequire(refToMain(routeBlockref) == DBTC || 
             /**
              * routeBlockref will point to SPJ for operations initiated by
              * that block. TRANSID_AI_R should not be sent to SPJ, as
              * SPJ will do its own internal error handling to compensate
              * for the lost TRANSID_AI signal.
              */
             refToMain(routeBlockref) == DBSPJ ||
             /** 
              * A node should always be connected to itself. So we should
              * never need to send TRANSID_AI_R in this case.
              */
             (nodeId == getOwnNodeId() && connectedToNode));

  if (connectedToNode){
    /**
     * Own node -> execute direct
     */
    if(nodeId != getOwnNodeId()){
      jam();
    
      /**
       * Send long sig
       */
      if (ToutBufIndex >= 22 && is_api) {
	jam();
	/**
	 * Flush buffer so that order is maintained
	 */
	if (TpacketTA != 0) {
	  jam();
	  BlockReference TBref = numberToRef(API_PACKED, nodeId);
	  MEMCOPY_NO_WORDS(&signal->theData[0],
			   &hostBuffer[nodeId].packetBufferTA[0],
			   TpacketLen);
	  sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
	  hostBuffer[nodeId].noOfPacketsTA = 0;
	  hostBuffer[nodeId].packetLenTA = 0;
	  transIdAI->connectPtr = sig0;
	  transIdAI->transId[0] = sig1;
	  transIdAI->transId[1] = sig2;
	}//if
	LinearSectionPtr ptr[3];
	ptr[0].p= &signal->theData[25];
	ptr[0].sz= ToutBufIndex;
	sendSignal(recBlockref, GSN_TRANSID_AI, signal, 3, JBB, ptr, 1);
	return;
      }
      
      /**
       * Send long signal to DBUTIL.
       */
      const Uint32 block= refToMain(recBlockref);
      if ((block == DBUTIL || block == DBSPJ) && !old_dest) {
	jam();
	LinearSectionPtr ptr[3];
	ptr[0].p= &signal->theData[25];
	ptr[0].sz= ToutBufIndex;
	sendSignal(recBlockref, GSN_TRANSID_AI, signal, 3, JBB, ptr, 1);
	return;
      }

      /**
       * short sig + api -> buffer
       */
#ifndef NDB_NO_DROPPED_SIGNAL
      if (ToutBufIndex < 22 && is_api){
	jam();
	bufferTRANSID_AI(signal, recBlockref, 3+ToutBufIndex);
	return;
      }
#endif      

      /**
       * rest -> old send sig
       */
      Uint32 * src= signal->theData+25;
      if (ToutBufIndex >= 22){
	do {
	  jam();
	  MEMCOPY_NO_WORDS(&signal->theData[3], src, 22);
	  sendSignal(recBlockref, GSN_TRANSID_AI, signal, 25, JBB);
	  ToutBufIndex -= 22;
	  src += 22;
	} while(ToutBufIndex >= 22);
      }
      
      if (ToutBufIndex > 0){
	jam();
	MEMCOPY_NO_WORDS(&signal->theData[3], src, ToutBufIndex);
	sendSignal(recBlockref, GSN_TRANSID_AI, signal, 3+ToutBufIndex, JBB);
      }
      return;
    }

    /**
     * BACKUP/LQH run in our thread, so we can EXECUTE_DIRECT().
     *
     * The UTIL/TC blocks are in another thread (in multi-threaded ndbd), so
     * must use sendSignal().
     *
     * In MT LQH only LQH and BACKUP are in same thread, and BACKUP only
     * in LCP case since user-backup uses single worker.
     */
    const bool sameInstance = refToInstance(recBlockref) == instance();
    const Uint32 blockNumber= refToMain(recBlockref);
    if (sameInstance &&
        (blockNumber == BACKUP ||
         blockNumber == DBLQH ||
         blockNumber == SUMA))
    {
      EXECUTE_DIRECT(blockNumber, GSN_TRANSID_AI, signal, 3 + ToutBufIndex);
      jamEntry();
    }
    else
    {
      jam();
      LinearSectionPtr ptr[3];
      ptr[0].p= &signal->theData[3];
      ptr[0].sz= ToutBufIndex;
      if (ERROR_INSERTED(4038))
      {
        /* Copy data to Seg-section for delayed send */
        Uint32 sectionIVal = RNIL;
        ndbrequire(appendToSection(sectionIVal, ptr[0].p, ptr[0].sz));
        SectionHandle sh(this, sectionIVal);
        
        sendSignalWithDelay(recBlockref, GSN_TRANSID_AI, signal, 10, 3, &sh);
      }
      else
      {
        /**
         * We are sending to the same node, it is important that we maintain
         * signal order with SCAN_FRAGCONF and other signals. So we make sure
         * that TRANSID_AI is sent at the same priority level as the
         * SCAN_FRAGCONF will be sent at.
         *
         * One case for this is Backups, the receiver is the first LDM thread
         * which could have raised priority of scan executions to Priority A.
         * To ensure that TRANSID_AI arrives there before SCAN_FRAGCONF we
         * send also TRANSID_AI on priority A if the signal is sent on prio A.
         */
        JobBufferLevel prioLevel = req_struct->m_prio_a_flag ? JBA : JBB;
        sendSignal(recBlockref,
                   GSN_TRANSID_AI,
                   signal,
                   3,
                   prioLevel,
                   ptr,
                   1);
      }
    }
    return;
  }

  /** 
   * If this node does not have a direct connection 
   * to the receiving node, we want to send the signals 
   * routed via the node that controls this read
   */
  // TODO is_api && !old_dest){
  if (refToNode(recBlockref) == refToNode(routeBlockref))
  {
    jam();
    /**
     * Signal's only alternative route is direct - cannot be delivered, 
     * drop it. (Expected behavior if recBlockRef is an SPJ block.)
     */
    return;
  }
  // Only TC can handle TRANSID_AI_R signals.
  ndbrequire(refToMain(routeBlockref) == DBTC);
  transIdAI->attrData[0]= recBlockref;
  LinearSectionPtr ptr[3];
  ptr[0].p= &signal->theData[25];
  ptr[0].sz= ToutBufIndex;
  sendSignal(routeBlockref, GSN_TRANSID_AI_R, signal, 4, JBB, ptr, 1);
}
