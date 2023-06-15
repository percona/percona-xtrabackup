/******************************************************
Copyright (c) 2023 Percona LLC and/or its affiliates.

FIFO datasink interface for XtraBackup.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

/* FIFO Datasink implementation

at xtrabackup_init_datasinks we initialize ds_data as DS_TYPE_FIFO, and create a ds object as XB_STREAM_FMT_XBSTREAM (xbstream).
We adjust ds saying it has to pipe its data to DS_TYPE_FIFO and we adjust ds_data to xbstream.
From this point forward we know we need to first convert the raw data we received from copy threads to xbstream and then later pipe the transformed data (xbstream format) to FIFO (same was there before but fixed to STDOUT).

(gdb) p *ds_data
$2 = {
 datasink = 0x55555d670dc0 <datasink_xbstream>,
 root = 0x0,
 ptr = 0x55555e5af190,
 pipe_ctxt = 0x55555e1e43f0,
 fs_support_punch_hole = 105
}
(gdb) p *ds_data->pipe_ctxt
$3 = {
 datasink = 0x55555d670cc0 <datasink_fifo>,
 root = 0x55555e15ae10 "/tmp/stream",
 ptr = 0x55555e15ae50,
 pipe_ctxt = 0x666e632e706d75,
 fs_support_punch_hole = false
}

fifo_init - responsible for creating the stream dir and all the fifo files (thread_XX).
Once created, we open them for writing at the init function despite the fact that we have an open function, this is because with FIFO / STDOUT we do not actually close the FD when a copy thread finishes its works on a specific data file, so ds_open / ds_close operate differently, ds_open just returns a ds_fifo_file_t as file(ds_file_t)->ptr back to xbstream datasink
xbstream_init will create a list of ds_stream_ctxt_t, those stream contexts will later receive a fifo FD assigned to it. this list is called ds_parallel_stream_ctxt_t.
at xtrabackup_copy_datafile (aka copy threads) we call ds_open passing ds_data (xbstream) as a parameter - xbstream_open will pick the first ds_stream_ctxt_t from the ds_parallel_stream_ctxt_t list and rotate it to the end of the list, so the next copy thread gets the next item on the list. Once we have our stream context assigned, we will check if it has a destination file assigned to it, if not we will do it via ds_open this time passing out destination datasink (either FIFO or STDOUT).
FIFO datasink has a list of FIFO files it can assign on an open operation, when xbstream::ds_open calls FIFO::ds_open we assign the first FIFO file in the list and from this point forward that xbstream ds_stream_ctxt_t will be bound to this particular FIFO file. We do not assign the fifo file back to the list of fifo files as xbstream ds_stream_ctxt_t->fifo is a 1:1 mapping
Note: an xbstream_close operation only ensures we write any remaining buffer data to FIFO/STDOUT, we do not close the FD assigned to that stream context. We only remove the binding of stream context to FIFO file at xbstream_deinit
next xtrabackup_copy_datafile thread, will then go again to xbstream_open and this time it will get the next stream context from the list and same workflow will happen until the first stream context is reached again. When it happens, we will get a stream context assigned that already has a stream_ctxt->dest_file assigned and this is where we will start having multiple copy threads writing in parallel to the same stream
In summary, the relationship is:
 * Copy threads to stream context - N:1 - Multiple copy threads are assigned to the same xbstream context (this is done by the LRU pop_from & push_back)
 * xbstream context -> FIFO file - 1:1 - One stream context to one fifo file, once a stream context has a fifo file assigned to it, it will remain bound to this FIFO until deinit
Workflow:
Before:
 * copy_thread->stream_context->stdout
Now:
 * copy_thread->parallel_stream_ctxt->stream_context->fifo

*/
#ifndef DS_FIFO_H
#define DS_FIFO_H

#include "datasink.h"

extern uint ds_fifo_threads;

extern datasink_t datasink_fifo;

#endif
