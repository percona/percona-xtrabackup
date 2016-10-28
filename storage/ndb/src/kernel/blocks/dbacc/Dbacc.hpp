/*
   Copyright (c) 2003, 2014 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DBACC_H
#define DBACC_H

#if defined (VM_TRACE) && !defined(ACC_SAFE_QUEUE)
#define ACC_SAFE_QUEUE
#endif

#include <pc.hpp>
#include <DynArr256.hpp>
#include <SimulatedBlock.hpp>
#include <LHLevel.hpp>
#include <IntrusiveList.hpp>
#include "Container.hpp"

#define JAM_FILE_ID 344


#ifdef DBACC_C
// Debug Macros
#define dbgWord32(ptr, ind, val) 

/*
#define dbgWord32(ptr, ind, val) \
if(debug_jan){ \
tmp_val = val; \
switch(ind){ \
case 1: strcpy(tmp_string, "ZPOS_PAGE_TYPE   "); \
break; \
case 2: strcpy(tmp_string, "ZPOS_NO_ELEM_IN_PAGE"); \
break; \
case 3: strcpy(tmp_string, "ZPOS_CHECKSUM    "); \
break; \
case 5: strcpy(tmp_string, "ZPOS_FREE_AREA_IN_PAGE"); \
break; \
case 6: strcpy(tmp_string, "ZPOS_LAST_INDEX   "); \
break; \
case 7: strcpy(tmp_string, "ZPOS_INSERT_INDEX  "); \
break; \
case 8: strcpy(tmp_string, "ZPOS_ARRAY_POS    "); \
break; \
case 9: strcpy(tmp_string, "ZPOS_NEXT_FREE_INDEX"); \
break; \
case 10: strcpy(tmp_string, "ZPOS_NEXT_PAGE   "); \
break; \
case 11: strcpy(tmp_string, "ZPOS_PREV_PAGE   "); \
break; \
default: sprintf(tmp_string, "%-20d", ind);\
} \
ndbout << "Ptr: " << ptr.p->word32 << " \tIndex: " << tmp_string << " \tValue: " << tmp_val << " \tLINE: " << __LINE__ << endl; \
}\
*/

// Constants
/** ------------------------------------------------------------------------ 
 *   THESE ARE CONSTANTS THAT ARE USED FOR DEFINING THE SIZE OF BUFFERS, THE
 *   SIZE OF PAGE HEADERS, THE NUMBER OF BUFFERS IN A PAGE AND A NUMBER OF 
 *   OTHER CONSTANTS WHICH ARE CHANGED WHEN THE BUFFER SIZE IS CHANGED. 
 * ----------------------------------------------------------------------- */
#define ZHEAD_SIZE 32
#define ZBUF_SIZE 28
#define ZSHIFT_PLUS 5
#define ZSHIFT_MINUS 2
#define ZFREE_LIMIT 65
#define ZNO_CONTAINERS 64
#define ZELEM_HEAD_SIZE 1
/* ------------------------------------------------------------------------- */
/*  THESE CONSTANTS DEFINE THE USE OF THE PAGE HEADER IN THE INDEX PAGES.    */
/* ------------------------------------------------------------------------- */
#define ZPOS_PAGE_ID Page8::PAGE_ID
#define ZPOS_PAGE_TYPE 1
#define ZPOS_PAGE_TYPE_BIT 14
#define ZPOS_EMPTY_LIST Page8::EMPTY_LIST
#define ZPOS_ALLOC_CONTAINERS Page8::ALLOC_CONTAINERS
#define ZPOS_CHECKSUM Page8::CHECKSUM
#define ZPOS_NO_ELEM_IN_PAGE 2
#define ZPOS_FREE_AREA_IN_PAGE Page8::FREE_AREA_IN_PAGE
#define ZPOS_LAST_INDEX Page8::LAST_INDEX
#define ZPOS_INSERT_INDEX Page8::INSERT_INDEX
#define ZPOS_ARRAY_POS Page8::ARRAY_POS
#define ZPOS_NEXT_FREE_INDEX Page8::NEXT_FREE_INDEX
#define ZPOS_NEXT_PAGE Page8::NEXT_PAGE
#define ZPOS_PREV_PAGE Page8::PREV_PAGE
#define ZNORMAL_PAGE_TYPE 0
#define ZOVERFLOW_PAGE_TYPE 1
#define ZDEFAULT_LIST 3
#define ZWORDS_IN_PAGE 2048
#define ZADDFRAG 0
//#define ZEMPTY_FRAGMENT 0
#define ZFRAGMENTSIZE 64
#define ZFIRSTTIME 1
#define ZFS_CONNECTSIZE 300
#define ZFS_OPSIZE 100
#define ZKEYINKEYREQ 4
#define ZLEFT 1
#define ZLOCALLOGFILE 2
#define ZLOCKED 0
#define ZMAXSCANSIGNALLEN 20
#define ZMAINKEYLEN 8
#define ZNO_OF_DISK_VERSION 3
#define ZNO_OF_OP_PER_SIGNAL 20
//#define ZNOT_EMPTY_FRAGMENT 1
#define ZOP_HEAD_INFO_LN 3
#define ZOPRECSIZE 740
#define ZPAGE8_BASE_ADD 1
#define ZPAGESIZE 128
#define ZPARALLEL_QUEUE 1
#define ZPDIRECTORY 1
#define ZSCAN_MAX_LOCK 4
#define ZSERIAL_QUEUE 2
#define ZSPH1 1
#define ZSPH2 2
#define ZSPH3 3
#define ZSPH6 6
#define ZREADLOCK 0
#define ZRIGHT 2
#define ZROOTFRAGMENTSIZE 32
#define ZSCAN_LOCK_ALL 3
/**
 * Check kernel_types for other operation types
 */
#define ZSCAN_OP 8
#define ZSCAN_REC_SIZE 256
#define ZSTAND_BY 2
#define ZTABLESIZE 16
#define ZTABMAXINDEX 3
#define ZUNDEFINED_OP 6
#define ZUNLOCKED 1

/* --------------------------------------------------------------------------------- */
/* CONTINUEB CODES                                                                   */
/* --------------------------------------------------------------------------------- */
#define ZINITIALISE_RECORDS 1
#define ZREL_ROOT_FRAG 5
#define ZREL_FRAG 6
#define ZREL_DIR 7

/* ------------------------------------------------------------------------- */
/* ERROR CODES                                                               */
/* ------------------------------------------------------------------------- */
#define ZLIMIT_OF_ERROR 600 // Limit check for error codes
#define ZCHECKROOT_ERROR 601 // Delete fragment error code
#define ZCONNECT_SIZE_ERROR 602 // ACC_SEIZEREF
#define ZDIR_RANGE_ERROR 603 // Add fragment error code
#define ZFULL_FRAGRECORD_ERROR 604 // Add fragment error code
#define ZFULL_ROOTFRAGRECORD_ERROR 605 // Add fragment error code
#define ZROOTFRAG_STATE_ERROR 606 // Add fragment
#define ZOVERTAB_REC_ERROR 607 // Add fragment

#define ZSCAN_REFACC_CONNECT_ERROR 608 // ACC_SCANREF
#define ZFOUR_ACTIVE_SCAN_ERROR 609 // ACC_SCANREF
#define ZNULL_SCAN_REC_ERROR 610 // ACC_SCANREF

#define ZDIRSIZE_ERROR 623
#define ZOVER_REC_ERROR 624 // Insufficient Space
#define ZPAGESIZE_ERROR 625
#define ZTUPLE_DELETED_ERROR 626
#define ZREAD_ERROR 626
#define ZWRITE_ERROR 630
#define ZTO_OP_STATE_ERROR 631
#define ZTOO_EARLY_ACCESS_ERROR 632
#define ZDIR_RANGE_FULL_ERROR 633 // on fragment

#if ZBUF_SIZE != ((1 << ZSHIFT_PLUS) - (1 << ZSHIFT_MINUS))
#error ZBUF_SIZE != ((1 << ZSHIFT_PLUS) - (1 << ZSHIFT_MINUS))
#endif

static
inline
Uint32 mul_ZBUF_SIZE(Uint32 i)
{
  return (i << ZSHIFT_PLUS) - (i << ZSHIFT_MINUS);
}

#endif

class ElementHeader {
  /**
   * 
   * l = Locked    -- If true contains operation else scan bits + hash value
   * s = Scan bits
   * h = Reduced hash value. The lower bits used for address is shifted away
   * o = Operation ptr I
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * lssssssssssss   hhhhhhhhhhhhhhhh
   *  ooooooooooooooooooooooooooooooo
   */
public:
  static bool getLocked(Uint32 data);
  static bool getUnlocked(Uint32 data);
  static Uint32 getScanBits(Uint32 data);
  static Uint32 getOpPtrI(Uint32 data);
  static LHBits16 getReducedHashValue(Uint32 data);

  static Uint32 setLocked(Uint32 opPtrI);
  static Uint32 setUnlocked(Uint32 scanBits, LHBits16 const& reducedHashValue);
  static Uint32 setScanBit(Uint32 header, Uint32 scanBit);
  static Uint32 setReducedHashValue(Uint32 header, LHBits16 const& reducedHashValue);
  static Uint32 clearScanBit(Uint32 header, Uint32 scanBit);
};

inline 
bool
ElementHeader::getLocked(Uint32 data){
  return (data & 1) == 0;
}

inline 
bool
ElementHeader::getUnlocked(Uint32 data){
  return (data & 1) == 1;
}

inline 
Uint32 
ElementHeader::getScanBits(Uint32 data){
  assert(getUnlocked(data));
  return (data >> 1) & ((1 << MAX_PARALLEL_SCANS_PER_FRAG) - 1);
}

inline
LHBits16
ElementHeader::getReducedHashValue(Uint32 data){
  assert(getUnlocked(data));
  return LHBits16::unpack(data >> 16);
}

inline
Uint32 
ElementHeader::getOpPtrI(Uint32 data){
  assert(getLocked(data));
  return data >> 1;
}

inline 
Uint32 
ElementHeader::setLocked(Uint32 opPtrI){
  assert(opPtrI < 0x8000000);
  return (opPtrI << 1) + 0;
}
inline
Uint32 
ElementHeader::setUnlocked(Uint32 scanBits, LHBits16 const& reducedHashValue)
{
  assert(scanBits < (1 << MAX_PARALLEL_SCANS_PER_FRAG));
  return (Uint32(reducedHashValue.pack()) << 16) | (scanBits << 1) | 1;
}

inline
Uint32 
ElementHeader::setScanBit(Uint32 header, Uint32 scanBit){
  assert(getUnlocked(header));
  return header | (scanBit << 1);
}

inline
Uint32 
ElementHeader::clearScanBit(Uint32 header, Uint32 scanBit){
  assert(getUnlocked(header));
  return header & (~(scanBit << 1));
}

inline
Uint32
ElementHeader::setReducedHashValue(Uint32 header, LHBits16 const& reducedHashValue)
{
  assert(getUnlocked(header));
  return (Uint32(reducedHashValue.pack()) << 16) | (header & 0xffff);
}

typedef Container::Header ContainerHeader;

class Dbacc: public SimulatedBlock {
  friend class DbaccProxy;

public:
// State values
enum State {
  FREEFRAG = 0,
  ACTIVEFRAG = 1,
  //SEND_QUE_OP = 2,
  WAIT_NOTHING = 10,
  WAIT_ONE_CONF = 26,
  FREE_OP = 30,
  WAIT_EXE_OP = 32,
  WAIT_IN_QUEUE = 34,
  EXE_OP = 35,
  SCAN_ACTIVE = 36,
  SCAN_WAIT_IN_QUEUE = 37,
  IDLE = 39,
  ACTIVE = 40,
  WAIT_COMMIT_ABORT = 41,
  ABORT = 42,
  ABORTADDFRAG = 43,
  REFUSEADDFRAG = 44,
  DELETEFRAG = 45,
  DELETETABLE = 46,
  UNDEFINEDROOT = 47,
  ADDFIRSTFRAG = 48,
  ADDSECONDFRAG = 49,
  DELETEFIRSTFRAG = 50,
  DELETESECONDFRAG = 51,
  ACTIVEROOT = 52
};

// Records

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
struct Page8 {
  Uint32 word32[2048];
  enum Page_variables {
    PAGE_ID = 0,
    EMPTY_LIST = 1,
    ALLOC_CONTAINERS = 2,
    CHECKSUM = 3,
    FREE_AREA_IN_PAGE = 5,
    LAST_INDEX = 6,
    INSERT_INDEX = 7,
    ARRAY_POS = 8,
    NEXT_FREE_INDEX = 9,
    NEXT_PAGE = 10,
    PREV_PAGE = 11
  };
}; /* p2c: size = 8192 bytes */

  typedef Ptr<Page8> Page8Ptr;

struct Page8SLinkMethods
{
  static Uint32 getNext(Page8 const& item) { return item.word32[Page8::NEXT_PAGE]; }
  static void setNext(Page8& item, Uint32 next) { item.word32[Page8::NEXT_PAGE] = next; }
  static void setPrev(Page8& /* item */, Uint32 /* prev */) { /* no op for single linked list */ }
};

struct ContainerPageLinkMethods
{
  static Uint32 getNext(Page8 const& item) { return item.word32[Page8::NEXT_PAGE]; }
  static void setNext(Page8& item, Uint32 next) { item.word32[Page8::NEXT_PAGE] = next; }
  static Uint32 getPrev(Page8 const& item) { return item.word32[Page8::PREV_PAGE]; }
  static void setPrev(Page8& item, Uint32 prev) { item.word32[Page8::PREV_PAGE] = prev; }
};

typedef SLCFifoListImpl<Dbacc,Page8,Page8,Page8SLinkMethods> Page8List;
typedef LocalSLCFifoListImpl<Dbacc,Page8,Page8,Page8SLinkMethods> LocalPage8List;
typedef DLCFifoListImpl<Dbacc,Page8,Page8,ContainerPageLinkMethods> ContainerPageList;
typedef LocalDLCFifoListImpl<Dbacc,Page8,Page8,ContainerPageLinkMethods> LocalContainerPageList;

/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
struct Fragmentrec {
  Uint32 scan[MAX_PARALLEL_SCANS_PER_FRAG];
  union {
    Uint32 mytabptr;
    Uint32 myTableId;
  };
  union {
    Uint32 fragmentid;
    Uint32 myfid;
  };
  Uint32 tupFragptr;
  Uint32 roothashcheck;
  Uint32 noOfElements;
  Uint32 m_commit_count;
  State rootState;
  
//-----------------------------------------------------------------------------
// These variables keep track of allocated pages, the number of them and the
// start file page of them. Used during local checkpoints.
//-----------------------------------------------------------------------------
  Uint32 datapages[8];
  Uint32 activeDataPage;

//-----------------------------------------------------------------------------
// Temporary variables used during shrink and expand process.
//-----------------------------------------------------------------------------
  Uint32 expReceivePageptr;
  Uint32 expReceiveIndex;
  Uint32 expReceiveForward;
  Uint32 expSenderDirIndex;
  Uint32 expSenderIndex;
  Uint32 expSenderPageptr;

//-----------------------------------------------------------------------------
// List of lock owners and list of lock waiters to support LCP handling
//-----------------------------------------------------------------------------
  Uint32 lockOwnersList;

//-----------------------------------------------------------------------------
// References to Directory Ranges (which in turn references directories, which
// in its turn references the pages) for the bucket pages and the overflow
// bucket pages.
//-----------------------------------------------------------------------------
  DynArr256::Head directory;

//-----------------------------------------------------------------------------
// We have a list of overflow pages with free areas. We have a special record,
// the overflow record representing these pages. The reason is that the
// same record is also used to represent pages in the directory array that have
// been released since they were empty (there were however higher indexes with
// data in them). These are put in the firstFreeDirIndexRec-list.
// An overflow record representing a page can only be in one of these lists.
//-----------------------------------------------------------------------------
  ContainerPageList::Head fullpages; // For pages where only containers on page are allowed to overflow (word32[ZPOS_ALLOC_CONTAINERS] > ZFREE_LIMIT)
  ContainerPageList::Head sparsepages; // For pages that other pages are still allowed to overflow into (0 < word32[ZPOS_ALLOC_CONTAINERS] <= ZFREE_LIMIT)

//-----------------------------------------------------------------------------
// Counter keeping track of how many times we have expanded. We need to ensure
// that we do not shrink so many times that this variable becomes negative.
//-----------------------------------------------------------------------------
  Uint32 expandCounter;

//-----------------------------------------------------------------------------
// These variables are important for the linear hashing algorithm.
// localkeylen is the size of the local key (1 and 2 is currently supported)
// maxloadfactor is the factor specifying when to expand
// minloadfactor is the factor specifying when to shrink (hysteresis model)
// maxp and p
// maxp and p is the variables most central to linear hashing. p + maxp + 1 is the
// current number of buckets. maxp is the largest value of the type 2**n - 1
// which is smaller than the number of buckets. These values are used to find
// correct bucket with the aid of the hash value.
//
// slack is the variable keeping track of whether we have inserted more than
// the current size is suitable for or less. Slack together with the boundaries
// set by maxloadfactor and minloadfactor decides when to expand/shrink
// slackCheck When slack goes over this value it is time to expand.
// slackCheck = (maxp + p + 1)*(maxloadfactor - minloadfactor) or 
// bucketSize * hysteresis
// Since at most RNIL 8KiB-pages can be used for a fragment, the extreme values
// for slack will be within -2^43 and +2^43 words.
//-----------------------------------------------------------------------------
  LHLevelRH level;
  Uint32 localkeylen;
  Uint32 maxloadfactor;
  Uint32 minloadfactor;
  Int64 slack;
  Int64 slackCheck;

//-----------------------------------------------------------------------------
// nextfreefrag is the next free fragment if linked into a free list
//-----------------------------------------------------------------------------
  Uint32 nextfreefrag;

//-----------------------------------------------------------------------------
// This variable is used during restore to keep track of page id of read pages.
// During read of bucket pages this is used to calculate the page id and also
// to verify that the page id of the read page is correct. During read of over-
// flow pages it is only used to keep track of the number of pages read.
//-----------------------------------------------------------------------------
  Uint32 nextAllocPage;

//-----------------------------------------------------------------------------
// Number of pages read from file during restore
//-----------------------------------------------------------------------------
  Uint32 noOfExpectedPages;

//-----------------------------------------------------------------------------
// Fragment State, mostly applicable during LCP and restore
//-----------------------------------------------------------------------------
  State fragState;

//-----------------------------------------------------------------------------
// elementLength: Length of element in bucket and overflow pages
// keyLength: Length of key
//-----------------------------------------------------------------------------
  Uint8 elementLength;
  Uint16 keyLength;

//-----------------------------------------------------------------------------
// Only allow one expand or shrink signal in queue at the time.
//-----------------------------------------------------------------------------
  bool expandOrShrinkQueued;

//-----------------------------------------------------------------------------
// hashcheckbit is the bit to check whether to send element to split bucket or not
// k (== 6) is the number of buckets per page
//-----------------------------------------------------------------------------
  STATIC_CONST( k = 6 );
  STATIC_CONST( MIN_HASH_COMPARE_BITS = 7 );
  STATIC_CONST( MAX_HASH_VALUE_BITS = 31 );

//-----------------------------------------------------------------------------
// nodetype can only be STORED in this release. Is currently only set, never read
//-----------------------------------------------------------------------------
  Uint8 nodetype;

//-----------------------------------------------------------------------------
// flag to avoid accessing table record if no char attributes
//-----------------------------------------------------------------------------
  Uint8 hasCharAttr;

//-----------------------------------------------------------------------------
// flag to mark that execEXPANDCHECK2 has failed due to DirRange full
//-----------------------------------------------------------------------------
  Uint8 dirRangeFull;

  // Number of Page8 pages allocated for the hash index.
  Int32 m_noOfAllocatedPages;

public:
  Uint32 getPageNumber(Uint32 bucket_number) const;
  Uint32 getPageIndex(Uint32 bucket_number) const;
  bool enough_valid_bits(LHBits16 const& reduced_hash_value) const;
};

  typedef Ptr<Fragmentrec> FragmentrecPtr;
  void set_tup_fragptr(Uint32 fragptr, Uint32 tup_fragptr);

/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
struct Operationrec {
  Uint32 m_op_bits;
  Uint32 localdata[2];
  Uint32 elementIsforward;
  Uint32 elementPage;
  Uint32 elementPointer;
  Uint32 fid;
  Uint32 fragptr;
  LHBits32 hashValue;
  Uint32 nextLockOwnerOp;
  Uint32 nextOp;
  Uint32 nextParallelQue;
  union {
    Uint32 nextSerialQue;      
    Uint32 m_lock_owner_ptr_i; // if nextParallelQue = RNIL, else undefined
  };
  Uint32 prevOp;
  Uint32 prevLockOwnerOp;
  union {
    Uint32 prevParallelQue;
    Uint32 m_lo_last_parallel_op_ptr_i;
  };
  union {
    Uint32 prevSerialQue;
    Uint32 m_lo_last_serial_op_ptr_i;
  };
  Uint32 scanRecPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 userptr;
  Uint16 elementContainer;
  Uint16 tupkeylen;
  Uint32 xfrmtupkeylen;
  Uint32 userblockref;
  Uint16 scanBits;
  LHBits16 reducedHashValue;

  enum OpBits {
    OP_MASK                 = 0x0000F // 4 bits for operation type
    ,OP_LOCK_MODE           = 0x00010 // 0 - shared lock, 1 = exclusive lock
    ,OP_ACC_LOCK_MODE       = 0x00020 // Or:de lock mode of all operation
                                      // before me
    ,OP_LOCK_OWNER          = 0x00040
    ,OP_RUN_QUEUE           = 0x00080 // In parallell queue of lock owner
    ,OP_DIRTY_READ          = 0x00100
    ,OP_LOCK_REQ            = 0x00200 // isAccLockReq
    ,OP_COMMIT_DELETE_CHECK = 0x00400
    ,OP_INSERT_IS_DONE      = 0x00800
    ,OP_ELEMENT_DISAPPEARED = 0x01000
    
    ,OP_STATE_MASK          = 0xF0000
    ,OP_STATE_IDLE          = 0xF0000
    ,OP_STATE_WAITING       = 0x00000
    ,OP_STATE_RUNNING       = 0x10000
    ,OP_STATE_EXECUTED      = 0x30000
    
    ,OP_EXECUTED_DIRTY_READ = 0x3050F
    ,OP_INITIAL             = ~(Uint32)0
  };
  
  Operationrec() {}
  bool is_same_trans(const Operationrec* op) const {
    return 
      transId1 == op->transId1 && transId2 == op->transId2;
  }
  
}; /* p2c: size = 168 bytes */

  typedef Ptr<Operationrec> OperationrecPtr;

/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
struct ScanRec {
  enum ScanState {
    WAIT_NEXT,  
    SCAN_DISCONNECT
  };
  enum ScanBucketState {
    FIRST_LAP,
    SECOND_LAP,
    SCAN_COMPLETED
  };
  Uint32 activeLocalFrag;
  Uint32 nextBucketIndex;
  Uint32 scanNextfreerec;
  Uint32 scanFirstActiveOp;
  Uint32 scanFirstLockedOp;
  Uint32 scanLastLockedOp;
  Uint32 scanFirstQueuedOp;
  Uint32 scanLastQueuedOp;
  Uint32 scanUserptr;
  Uint32 scanTrid1;
  Uint32 scanTrid2;
  Uint32 startNoOfBuckets;
  Uint32 minBucketIndexToRescan;
  Uint32 maxBucketIndexToRescan;
  Uint32 scanOpsAllocated;
  ScanBucketState scanBucketState;
  ScanState scanState;
  Uint16 scanLockHeld;
  Uint32 scanUserblockref;
  Uint32 scanMask;
  Uint8 scanLockMode;
  Uint8 scanReadCommittedFlag;
}; 

  typedef Ptr<ScanRec> ScanRecPtr;


/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
struct Tabrec {
  Uint32 fragholder[MAX_FRAG_PER_LQH];
  Uint32 fragptrholder[MAX_FRAG_PER_LQH];
  Uint32 tabUserPtr;
  BlockReference tabUserRef;
  Uint32 tabUserGsn;
};
  typedef Ptr<Tabrec> TabrecPtr;

public:
  Dbacc(Block_context&, Uint32 instanceNumber = 0);
  virtual ~Dbacc();

  // pointer to TUP instance in this thread
  class Dbtup* c_tup;
  class Dblqh* c_lqh;

  void execACCMINUPDATE(Signal* signal);
  void execREAD_PSEUDO_REQ(Signal* signal);
  // Get the size of the logical to physical page map, in bytes.
  Uint32 getL2PMapAllocBytes(Uint32 fragId) const;
  void removerow(Uint32 op, const Local_key*);

  // Get the size of the linear hash map in bytes.
  Uint64 getLinHashByteSize(Uint32 fragId) const;

private:
  BLOCK_DEFINES(Dbacc);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execEXPANDCHECK2(Signal* signal);
  void execSHRINKCHECK2(Signal* signal);
  void execACC_OVER_REC(Signal* signal);
  void execNEXTOPERATION(Signal* signal);

  // Received signals
  void execSTTOR(Signal* signal);
  void execACCKEYREQ(Signal* signal);
  void execACCSEIZEREQ(Signal* signal);
  void execACCFRAGREQ(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_ABORTREQ(Signal* signal);
  void execACC_SCANREQ(Signal* signal);
  void execACC_COMMITREQ(Signal* signal);
  void execACC_TO_REQ(Signal* signal);
  void execACC_LOCKREQ(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);

  void execDROP_FRAG_REQ(Signal*);

  void execDBINFO_SCANREQ(Signal *signal);
  void execNODE_STATE_REP(Signal*);

  // Statement blocks
  void ACCKEY_error(Uint32 fromWhere);

  void commitDeleteCheck();
  void report_dealloc(Signal* signal, const Operationrec* opPtrP);
  
  typedef void * RootfragmentrecPtr;
  void initRootFragPageZero(FragmentrecPtr, Page8Ptr);
  void initFragAdd(Signal*, FragmentrecPtr);
  void initFragPageZero(FragmentrecPtr, Page8Ptr);
  void initFragGeneral(FragmentrecPtr);
  void verifyFragCorrect(FragmentrecPtr regFragPtr);
  void releaseFragResources(Signal* signal, Uint32 fragIndex);
  void releaseRootFragRecord(Signal* signal, RootfragmentrecPtr rootPtr);
  void releaseRootFragResources(Signal* signal, Uint32 tableId);
  void releaseDirResources(Signal* signal);
  void releaseDirectoryResources(Signal* signal,
                                 Uint32 fragIndex,
                                 Uint32 dirIndex,
                                 Uint32 startIndex,
                                 Uint32 directoryIndex);
  void releaseFragRecord(Signal* signal, FragmentrecPtr regFragPtr);
  void initScanFragmentPart(Signal* signal);
  Uint32 checkScanExpand(Signal* signal, Uint32 splitBucket);
  Uint32 checkScanShrink(Signal* signal, Uint32 sourceBucket, Uint32 destBucket);
  void initialiseFragRec(Signal* signal);
  void initialiseFsConnectionRec(Signal* signal);
  void initialiseFsOpRec(Signal* signal);
  void initialiseOperationRec(Signal* signal);
  void initialisePageRec(Signal* signal);
  void initialiseRootfragRec(Signal* signal);
  void initialiseScanRec(Signal* signal);
  void initialiseTableRec(Signal* signal);
  bool addfragtotab(Signal* signal, Uint32 rootIndex, Uint32 fragId);
  void initOpRec(Signal* signal);
  void sendAcckeyconf(Signal* signal);
  Uint32 getNoParallelTransaction(const Operationrec*);

#ifdef VM_TRACE
  Uint32 getNoParallelTransactionFull(const Operationrec*);
#endif
#ifdef ACC_SAFE_QUEUE
  bool validate_lock_queue(OperationrecPtr opPtr);
  Uint32 get_parallel_head(OperationrecPtr opPtr);
  void dump_lock_queue(OperationrecPtr loPtr);
#else
  bool validate_lock_queue(OperationrecPtr) { return true;}
#endif
  /**
    Return true if the sum of per fragment pages counts matches the total
    page count (cnoOfAllocatedPages). Used for consistency checks. 
   */
  bool validatePageCount() const;
public:  
  void execACCKEY_ORD(Signal* signal, Uint32 opPtrI);
  void startNext(Signal* signal, OperationrecPtr lastOp);
  
private:
  Uint32 placeReadInLockQueue(OperationrecPtr lockOwnerPtr);
  Uint32 placeWriteInLockQueue(OperationrecPtr lockOwnerPtr);
  void placeSerialQueue(OperationrecPtr lockOwner, OperationrecPtr op);
  void abortSerieQueueOperation(Signal* signal, OperationrecPtr op);  
  void abortParallelQueueOperation(Signal* signal, OperationrecPtr op);  
  
  void expandcontainer(Signal* signal);
  void shrinkcontainer(Signal* signal);
  void nextcontainerinfoExp(Signal* signal, ContainerHeader const containerhead);
  void releaseAndCommitActiveOps(Signal* signal);
  void releaseAndCommitQueuedOps(Signal* signal);
  void releaseAndAbortLockedOps(Signal* signal);
  void containerinfo(Signal* signal, ContainerHeader& containerhead);
  bool getScanElement(Signal* signal);
  void initScanOpRec(Signal* signal);
  void nextcontainerinfo(Signal* signal, ContainerHeader const containerhead);
  void putActiveScanOp(Signal* signal);
  void putOpScanLockQue();
  void putReadyScanQueue(Signal* signal, Uint32 scanRecIndex);
  void releaseScanBucket(Signal* signal);
  void releaseScanContainer(Signal* signal);
  void releaseScanRec(Signal* signal);
  bool searchScanContainer(Signal* signal);
  void sendNextScanConf(Signal* signal);
  void setlock(Signal* signal);
  void takeOutActiveScanOp(Signal* signal);
  void takeOutScanLockQueue(Uint32 scanRecIndex);
  void takeOutReadyScanQueue(Signal* signal);
  void insertElement(Signal* signal);
  void insertContainer(Signal* signal, ContainerHeader& containerhead);
  void addnewcontainer(Signal* signal);
  void getfreelist(Signal* signal);
  void increaselistcont(Signal* signal);
  void seizeLeftlist(Signal* signal);
  void seizeRightlist(Signal* signal);
  Uint32 readTablePk(Uint32 lkey1, Uint32 lkey2, Uint32 eh, OperationrecPtr);
  Uint32 getElement(Signal* signal, OperationrecPtr& lockOwner);
  LHBits32 getElementHash(OperationrecPtr& oprec);
  LHBits32 getElementHash(Uint32 const* element, Int32 forward);
  LHBits32 getElementHash(Uint32 const* element, Int32 forward, OperationrecPtr& oprec);
  void shrink_adjust_reduced_hash_value(Uint32 bucket_number);
  Uint32 getPagePtr(DynArr256::Head&, Uint32);
  bool setPagePtr(DynArr256::Head& directory, Uint32 index, Uint32 ptri);
  Uint32 unsetPagePtr(DynArr256::Head& directory, Uint32 index);
  void getdirindex(Signal* signal);
  void commitdelete(Signal* signal);
  void deleteElement(Signal* signal);
  void getLastAndRemove(Signal* signal, ContainerHeader& containerhead);
  void releaseLeftlist(Signal* signal);
  void releaseRightlist(Signal* signal);
  void checkoverfreelist(Signal* signal);
  void abortOperation(Signal* signal);
  void commitOperation(Signal* signal);
  void copyOpInfo(OperationrecPtr dst, OperationrecPtr src);
  Uint32 executeNextOperation(Signal* signal);
  void releaselock(Signal* signal);
  void release_lockowner(Signal* signal, OperationrecPtr, bool commit);
  void startNew(Signal* signal, OperationrecPtr newOwner);
  void abortWaitingOperation(Signal*, OperationrecPtr);
  void abortExecutedOperation(Signal*, OperationrecPtr);
  
  void takeOutFragWaitQue(Signal* signal);
  void check_lock_upgrade(Signal* signal, OperationrecPtr release_op, bool lo);
  void check_lock_upgrade(Signal* signal, OperationrecPtr lock_owner,
			  OperationrecPtr release_op);
  void allocOverflowPage(Signal* signal);
  bool getfragmentrec(Signal* signal, FragmentrecPtr&, Uint32 fragId);
  void insertLockOwnersList(Signal* signal, const OperationrecPtr&);
  void takeOutLockOwnersList(Signal* signal, const OperationrecPtr&);

  void initFsOpRec(Signal* signal);
  void initOverpage(Signal* signal);
  void initPage(Signal* signal);
  void initRootfragrec(Signal* signal);
  void putOpInFragWaitQue(Signal* signal);
  void releaseFsConnRec(Signal* signal);
  void releaseFsOpRec(Signal* signal);
  void releaseOpRec(Signal* signal);
  void releaseOverpage(Signal* signal);
  void releasePage(Signal* signal);
  void seizeDirectory(Signal* signal);
  void seizeFragrec(Signal* signal);
  void seizeFsConnectRec(Signal* signal);
  void seizeFsOpRec(Signal* signal);
  void seizeOpRec(Signal* signal);
  void seizePage(Signal* signal);
  void seizeRootfragrec(Signal* signal);
  void seizeScanRec(Signal* signal);
  void sendSystemerror(Signal* signal, int line);

  void addFragRefuse(Signal* signal, Uint32 errorCode);
  void ndbsttorryLab(Signal* signal);
  void acckeyref1Lab(Signal* signal, Uint32 result_code);
  void insertelementLab(Signal* signal);
  void checkNextFragmentLab(Signal* signal);
  void endofexpLab(Signal* signal);
  void endofshrinkbucketLab(Signal* signal);
  void senddatapagesLab(Signal* signal);
  void sttorrysignalLab(Signal* signal);
  void sendholdconfsignalLab(Signal* signal);
  void accIsLockedLab(Signal* signal, OperationrecPtr lockOwnerPtr);
  void insertExistElemLab(Signal* signal, OperationrecPtr lockOwnerPtr);
  void refaccConnectLab(Signal* signal);
  void releaseScanLab(Signal* signal);
  void ndbrestart1Lab(Signal* signal);
  void initialiseRecordsLab(Signal* signal, Uint32 ref, Uint32 data);
  void checkNextBucketLab(Signal* signal);
  void storeDataPageInDirectoryLab(Signal* signal);

  void zpagesize_error(const char* where);

  // charsets
  void xfrmKeyData(Signal* signal);

  // Initialisation
  void initData();
  void initRecords();

#ifdef VM_TRACE
  void debug_lh_vars(const char* where);
#else
  void debug_lh_vars(const char* where) {}
#endif

public:
  void getPtr(Ptr<Page8>& page) const;
private:
  // Variables
/* --------------------------------------------------------------------------------- */
/* DIRECTORY                                                                         */
/* --------------------------------------------------------------------------------- */
  DynArr256Pool   directoryPool;
/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
  Fragmentrec *fragmentrec;
  FragmentrecPtr fragrecptr;
  Uint32 cfirstfreefrag;
  Uint32 cfragmentsize;
  RSS_OP_COUNTER(cnoOfFreeFragrec);
  RSS_OP_SNAPSHOT(cnoOfFreeFragrec);


/* --------------------------------------------------------------------------------- */
/* FS_CONNECTREC                                                                     */
/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
  Operationrec *operationrec;
  OperationrecPtr operationRecPtr;
  OperationrecPtr idrOperationRecPtr;
  OperationrecPtr mlpqOperPtr;
  OperationrecPtr queOperPtr;
  OperationrecPtr readWriteOpPtr;
  Uint32 cfreeopRec;
  Uint32 coprecsize;

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
  Page8 *page8;
  /* 8 KB PAGE                       */
  Page8Ptr ancPageptr;
  Page8Ptr colPageptr;
  Page8Ptr ccoPageptr;
  Page8Ptr datapageptr;
  Page8Ptr delPageptr;
  Page8Ptr excPageptr;
  Page8Ptr expPageptr;
  Page8Ptr gdiPageptr;
  Page8Ptr gePageptr;
  Page8Ptr gflPageptr;
  Page8Ptr idrPageptr;
  Page8Ptr ilcPageptr;
  Page8Ptr inpPageptr;
  Page8Ptr iopPageptr;
  Page8Ptr lastPageptr;
  Page8Ptr lastPrevpageptr;
  Page8Ptr lcnPageptr;
  Page8Ptr lcnCopyPageptr;
  Page8Ptr lupPageptr;
  Page8Ptr ciPageidptr;
  Page8Ptr gsePageidptr;
  Page8Ptr isoPageptr;
  Page8Ptr nciPageidptr;
  Page8Ptr rsbPageidptr;
  Page8Ptr rscPageidptr;
  Page8Ptr slPageidptr;
  Page8Ptr sscPageidptr;
  Page8Ptr rlPageptr;
  Page8Ptr rlpPageptr;
  Page8Ptr ropPageptr;
  Page8Ptr rpPageptr;
  Page8Ptr slPageptr;
  Page8Ptr spPageptr;
  Page8List::Head cfreepages;
  Uint32 cpagesize;
  Uint32 cpageCount;
  Uint32 cnoOfAllocatedPages;
  Uint32 cnoOfAllocatedPagesMax;
  Uint32 m_maxAllocPages; // == cpagesize * (100 - m_free_pct) / 100
  Uint32 m_free_pct;
  bool m_oom; // if cnoOfAllocatedPages > m_maxAllocPages

/* --------------------------------------------------------------------------------- */
/* ROOTFRAGMENTREC                                                                   */
/*          DURING EXPAND FRAGMENT PROCESS, EACH FRAGMEND WILL BE EXPAND INTO TWO    */
/*          NEW FRAGMENTS.TO MAKE THIS PROCESS EASIER, DURING ADD FRAGMENT PROCESS   */
/*          NEXT FRAGMENT IDENTIIES WILL BE CALCULATED, AND TWO FRAGMENTS WILL BE    */
/*          ADDED IN (NDBACC). THEREBY EXPAND OF FRAGMENT CAN BE PERFORMED QUICK AND */
/*          EASY.THE NEW FRAGMENT ID SENDS TO TUP MANAGER FOR ALL OPERATION PROCESS. */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
  ScanRec *scanRec;
  ScanRecPtr scanPtr;
  Uint32 cscanRecSize;
  Uint32 cfirstFreeScanRec;
/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
  Tabrec *tabrec;
  TabrecPtr tabptr;
  Uint32 ctablesize;
  Uint32 tgseElementptr;
  Uint32 tgseContainerptr;
  Uint32 trlNextused;
  Uint32 trlPrevused;
  Uint32 tlcnChecksum;
  Uint32 tlupElemIndex;
  Uint32 tlupIndex;
  Uint32 tlupForward;
  Uint32 tancNext;
  Uint32 tancBufType;
  Uint32 tancContainerptr;
  Uint32 tancPageindex;
  Uint32 tancPagei;
  Uint32 tidrResult;
  Uint32 tidrElemhead;
  Uint32 tidrForward;
  Uint32 tidrPageindex;
  Uint32 tidrContainerptr;
  Uint32 tlastForward;
  Uint32 tlastPageindex;
  Uint32 tlastContainerlen;
  Uint32 tlastElementptr;
  Uint32 tlastContainerptr;
  Uint32 trlPageindex;
  Uint32 tdelContainerptr;
  Uint32 tdelElementptr;
  Uint32 tdelForward;
  Uint32 tipPageId;
  Uint32 tgeContainerptr;
  Uint32 tgeElementptr;
  Uint32 tgeForward;
  Uint32 texpDirInd;
  Uint32 texpDirRangeIndex;
  Uint32 texpDirPageIndex;
  Uint32 tdata0;
  Uint32 tcheckpointid;
  Uint32 tciContainerptr;
  Uint32 tnciContainerptr;
  Uint32 tisoContainerptr;
  Uint32 trscContainerptr;
  Uint32 tsscContainerptr;
  Uint32 tciContainerlen;
  Uint32 trscContainerlen;
  Uint32 tsscContainerlen;
  Uint32 tslElementptr;
  Uint32 tisoElementptr;
  Uint32 tsscElementptr;
  Uint32 tfid;
  Uint32 tscanFlag;
  Uint32 tgflBufType;
  Uint32 tgseIsforward;
  Uint32 tsscIsforward;
  Uint32 trscIsforward;
  Uint32 tciIsforward;
  Uint32 tnciIsforward;
  Uint32 tisoIsforward;
  Uint32 tgseIsLocked;
  Uint32 tsscIsLocked;
  Uint32 tkeylen;
  Uint32 tmp;
  Uint32 tmpP;
  Uint32 tmpP2;
  Uint32 tmp2;
  Uint32 tgflPageindex;
  Uint32 tmpindex;
  Uint32 tslNextfree;
  Uint32 tslPageindex;
  Uint32 tgsePageindex;
  Uint32 tnciNextSamePage;
  Uint32 tslPrevfree;
  Uint32 tciPageindex;
  Uint32 trsbPageindex;
  Uint32 tnciPageindex;
  Uint32 tlastPrevconptr;
  Uint32 tresult;
  Uint32 tuserptr;
  BlockReference tuserblockref;
  Uint32 tlqhPointer;
  Uint32 tholdSentOp;
  Uint32 tholdMore;
  Uint32 tgdiPageindex;
  Uint32 tiopIndex;
  Uint32 tullIndex;
  Uint32 turlIndex;
  Uint32 tlfrTmp1;
  Uint32 tlfrTmp2;
  Uint32 tscanTrid1;
  Uint32 tscanTrid2;

  Uint32 ctest;
  Uint32 clqhPtr;
  BlockReference clqhBlockRef;
  Uint32 cminusOne;
  NodeId cmynodeid;
  BlockReference cownBlockref;
  BlockReference cndbcntrRef;
  Uint16 csignalkey;
  Uint32 czero;
  Uint32 cexcForward;
  Uint32 cexcPageindex;
  Uint32 cexcContainerptr;
  Uint32 cexcContainerlen;
  Uint32 cexcElementptr;
  Uint32 cexcPrevconptr;
  Uint32 cexcMovedLen;
  Uint32 cexcPrevpageptr;
  Uint32 cexcPrevpageindex;
  Uint32 cexcPrevforward;
  Uint32 clocalkey[32];
  union {
  Uint32 ckeys[2048 * MAX_XFRM_MULTIPLY];
  Uint64 ckeys_align;
  };
  
  Uint32 c_errorInsert3000_TableId;
  Uint32 c_memusage_report_frequency;
};

inline Uint32 Dbacc::Fragmentrec::getPageNumber(Uint32 bucket_number) const
{
  assert(bucket_number < RNIL);
  return bucket_number >> k;
}

inline Uint32 Dbacc::Fragmentrec::getPageIndex(Uint32 bucket_number) const
{
  assert(bucket_number < RNIL);
  return bucket_number & ((1 << k) - 1);
}

inline bool Dbacc::Fragmentrec::enough_valid_bits(LHBits16 const& reduced_hash_value) const
{
  // Forte C 5.0 needs use of intermediate constant
  int const bits = MIN_HASH_COMPARE_BITS;
  return level.getNeededValidBits(bits) <= reduced_hash_value.valid_bits();
}

inline void Dbacc::getPtr(Ptr<Page8>& page) const
{
  ptrCheckGuard(page, cpagesize, page8);
}


#undef JAM_FILE_ID

#endif
