// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config_for_unittests.h"
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <stdio.h>
#include <stdlib.h>

// Correctly capturing backtrace from signal handler is most
// brittle. A number of configurations on Linux work, but not
// all. Same applies to BSDs. But lets somewhat broadly ask those
// setups to be tested. In general, if right backtraces are needed for
// CPU profiler, this test should pass as well.
#if __linux__ || (__FreeBSD__ && (__x86_64__ || __i386__)) || __NetBSD__
#include <signal.h>
#include <sys/time.h>
#define TEST_UCONTEXT_BITS 1
#endif

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#undef TEST_UCONTEXT_BITS
#  endif
#endif

#include <vector>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include <gperftools/stacktrace.h>
#include "tests/testutil.h"

// Obtain a backtrace, verify that the expected callers are present in the
// backtrace, and maybe print the backtrace to stdout.

// The sequence of functions whose return addresses we expect to see in the
// backtrace.
const int BACKTRACE_STEPS = 6;

struct AddressRange {
  const void *start, *end;
};

// Expected function [start,end] range.
AddressRange expected_range[BACKTRACE_STEPS];

bool skipping_ucontext;

#if __GNUC__
// Using GCC extension: address of a label can be taken with '&&label'.
// Start should be a label somewhere before recursive call, end somewhere
// after it.
#define INIT_ADDRESS_RANGE(fn, start_label, end_label, prange)           \
  do {                                                                   \
    (prange)->start = &&start_label;                                     \
    (prange)->end = &&end_label;                                         \
    CHECK_LT((prange)->start, (prange)->end);                            \
  } while (0)
// This macro expands into "unmovable" code (opaque to GCC), and that
// prevents GCC from moving a_label up or down in the code.
// Without it, there is no code following the 'end' label, and GCC
// (4.3.1, 4.4.0) thinks it safe to assign &&end an address that is before
// the recursive call.
#define DECLARE_ADDRESS_LABEL(a_label)                                   \
  a_label: do { __asm__ __volatile__(""); } while (0)
// Gcc 4.4.0 may split function into multiple chunks, and the chunk
// performing recursive call may end up later in the code then the return
// instruction (this actually happens with FDO).
// Adjust function range from __builtin_return_address.
#define ADJUST_ADDRESS_RANGE_FROM_RA(prange)                             \
  do {                                                                   \
    void *ra = __builtin_return_address(0);                              \
    CHECK_LT((prange)->start, ra);                                       \
    if (ra > (prange)->end) {                                            \
      printf("Adjusting range from %p..%p to %p..%p\n",                  \
             (prange)->start, (prange)->end,                             \
             (prange)->start, ra);                                       \
      (prange)->end = ra;                                                \
    }                                                                    \
  } while (0)
#else
// Assume the Check* functions below are not longer than 256 bytes.
#define INIT_ADDRESS_RANGE(fn, start_label, end_label, prange)           \
  do {                                                                   \
    (prange)->start = reinterpret_cast<const void *>(&fn);               \
    (prange)->end = reinterpret_cast<const char *>(&fn) + 256;           \
  } while (0)
#define DECLARE_ADDRESS_LABEL(a_label) do { } while (0)
#define ADJUST_ADDRESS_RANGE_FROM_RA(prange) do { } while (0)
#endif  // __GNUC__


//-----------------------------------------------------------------------//

void CheckRetAddrIsInFunction(void *ret_addr, const AddressRange &range)
{
  CHECK_GE(ret_addr, range.start);
  CHECK_LE(ret_addr, range.end);
}

//-----------------------------------------------------------------------//

extern "C" {

#if TEST_UCONTEXT_BITS

struct get_stack_trace_args {
  volatile bool ready;
  volatile bool captured;

  int *size_ptr;
  void **result;
  int max_depth;
} gst_args;

static
void SignalHandler(int dummy, siginfo_t *si, void* ucv) {
  if (!gst_args.ready || gst_args.captured) {
    return;
  }

  gst_args.captured = true;

  auto uc = static_cast<ucontext_t*>(ucv);

  *gst_args.size_ptr = GetStackTraceWithContext(gst_args.result,
                                                gst_args.max_depth,
                                                2,
                                                uc);
}

int ATTRIBUTE_NOINLINE CaptureLeafUContext(void **stack, int stack_len) {
  INIT_ADDRESS_RANGE(CheckStackTraceLeaf, start, end, &expected_range[0]);
  DECLARE_ADDRESS_LABEL(start);

  int size;

  printf("Capturing stack trace from signal's ucontext\n");
  struct sigaction sa;
  struct sigaction old_sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
  int rv = sigaction(SIGPROF, &sa, &old_sa);
  CHECK(rv == 0);

  gst_args.size_ptr = &size;
  gst_args.result = stack;
  gst_args.max_depth = stack_len;
  gst_args.captured = false;
  gst_args.ready = false;

  struct itimerval it;
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1;

  rv = setitimer(ITIMER_PROF, &it, nullptr);
  CHECK(rv == 0);

  // SignalHandler will run somewhere here, making sure we capture
  // backtrace from signal handler
  gst_args.ready = true;
  while (!gst_args.captured) {
    // do nothing
  }

  rv = sigaction(SIGPROF, &old_sa, nullptr);
  CHECK(rv == 0);

  printf("Obtained %d stack frames.\n", size);
  CHECK_GE(size, 1);
  CHECK_LE(size, stack_len);

  DECLARE_ADDRESS_LABEL(end);

  return size;
}

#endif  // TEST_UCONTEXT_BITS

int ATTRIBUTE_NOINLINE CaptureLeafPlain(void **stack, int stack_len) {
  INIT_ADDRESS_RANGE(CheckStackTraceLeaf, start, end, &expected_range[0]);
  DECLARE_ADDRESS_LABEL(start);

  int size = GetStackTrace(stack, stack_len, 0);

  printf("Obtained %d stack frames.\n", size);
  CHECK_GE(size, 1);
  CHECK_LE(size, stack_len);

  DECLARE_ADDRESS_LABEL(end);

  return size;
}

int ATTRIBUTE_NOINLINE CaptureLeafPlainEmptyUCP(void **stack, int stack_len) {
  INIT_ADDRESS_RANGE(CheckStackTraceLeaf, start, end, &expected_range[0]);
  DECLARE_ADDRESS_LABEL(start);

  int size = GetStackTraceWithContext(stack, stack_len, 0, nullptr);

  printf("Obtained %d stack frames.\n", size);
  CHECK_GE(size, 1);
  CHECK_LE(size, stack_len);

  DECLARE_ADDRESS_LABEL(end);

  return size;
}

int ATTRIBUTE_NOINLINE CaptureLeafWSkip(void **stack, int stack_len) {
  INIT_ADDRESS_RANGE(CheckStackTraceLeaf, start, end, &expected_range[0]);
  DECLARE_ADDRESS_LABEL(start);

  auto trampoline = [] (void **stack, int stack_len) ATTRIBUTE_NOINLINE {
    int rv = GetStackTrace(stack, stack_len, 1);
    (void)*(void * volatile *)(stack); // prevent tail-calling GetStackTrace
    return rv;
  };

  int size = trampoline(stack, stack_len);

  printf("Obtained %d stack frames.\n", size);
  CHECK_GE(size, 1);
  CHECK_LE(size, stack_len);

  DECLARE_ADDRESS_LABEL(end);

  return size;
}

void ATTRIBUTE_NOINLINE CheckStackTrace(int);

int (*leaf_capture_fn)(void**, int) = CaptureLeafPlain;
int leaf_capture_len = 20;

void ATTRIBUTE_NOINLINE CheckStackTraceLeaf(int i) {
  std::vector<void*> stack(leaf_capture_len + 1);
  stack[leaf_capture_len] = (void*)0x42;  // we will check that this value is not overwritten
  int size = 0;

  ADJUST_ADDRESS_RANGE_FROM_RA(&expected_range[1]);

  size = leaf_capture_fn(stack.data(), leaf_capture_len);
  CHECK_EQ(stack[leaf_capture_len], (void*)0x42);

#ifdef HAVE_EXECINFO_H
  {
    char **strings = backtrace_symbols(stack.data(), size);
    for (int i = 0; i < size; i++)
      printf("%s %p\n", strings[i], stack[i]);
    printf("CheckStackTrace() addr: %p\n", &CheckStackTrace);
    free(strings);
  }
#endif

  for (int i = 0, j = 0; i < BACKTRACE_STEPS && j < size; i++, j++) {
    if (i == 1 && j == 1) {
      // this is expected to be our function for which we don't
      // establish bounds. So skip.
      i--;
      continue;
    }
    printf("Backtrace %d: expected: %p..%p  actual: %p ... ",
           i, expected_range[i].start, expected_range[i].end, stack[j]);
    fflush(stdout);
    CheckRetAddrIsInFunction(stack[j], expected_range[i]);
    printf("OK\n");
  }
}

//-----------------------------------------------------------------------//

/* Dummy functions to make the backtrace more interesting. */
void ATTRIBUTE_NOINLINE CheckStackTrace4(int i) {
  ADJUST_ADDRESS_RANGE_FROM_RA(&expected_range[2]);
  INIT_ADDRESS_RANGE(CheckStackTrace4, start, end, &expected_range[1]);
  DECLARE_ADDRESS_LABEL(start);
  for (int j = i; j >= 0; j--)
    CheckStackTraceLeaf(j);
  DECLARE_ADDRESS_LABEL(end);
}
void ATTRIBUTE_NOINLINE CheckStackTrace3(int i) {
  ADJUST_ADDRESS_RANGE_FROM_RA(&expected_range[3]);
  INIT_ADDRESS_RANGE(CheckStackTrace3, start, end, &expected_range[2]);
  DECLARE_ADDRESS_LABEL(start);
  for (int j = i; j >= 0; j--)
    CheckStackTrace4(j);
  DECLARE_ADDRESS_LABEL(end);
}
void ATTRIBUTE_NOINLINE CheckStackTrace2(int i) {
  ADJUST_ADDRESS_RANGE_FROM_RA(&expected_range[4]);
  INIT_ADDRESS_RANGE(CheckStackTrace2, start, end, &expected_range[3]);
  DECLARE_ADDRESS_LABEL(start);
  for (int j = i; j >= 0; j--)
    CheckStackTrace3(j);
  DECLARE_ADDRESS_LABEL(end);
}
void ATTRIBUTE_NOINLINE CheckStackTrace1(int i) {
  ADJUST_ADDRESS_RANGE_FROM_RA(&expected_range[5]);
  INIT_ADDRESS_RANGE(CheckStackTrace1, start, end, &expected_range[4]);
  DECLARE_ADDRESS_LABEL(start);
  for (int j = i; j >= 0; j--)
    CheckStackTrace2(j);
  DECLARE_ADDRESS_LABEL(end);
}
void ATTRIBUTE_NOINLINE CheckStackTrace(int i) {
  INIT_ADDRESS_RANGE(CheckStackTrace, start, end, &expected_range[5]);
  DECLARE_ADDRESS_LABEL(start);
  for (int j = i; j >= 0; j--) {
    CheckStackTrace1(j);
  }
  DECLARE_ADDRESS_LABEL(end);
}

}  // extern "C"

//-----------------------------------------------------------------------//

void RunTest() {
  CheckStackTrace(0);
  printf("PASS\n");

  printf("Will test capturing stack trace with nullptr ucontext\n");
  leaf_capture_fn = CaptureLeafPlainEmptyUCP;
  CheckStackTrace(0);
  printf("PASS\n");

  printf("Will test capturing stack trace with skipped frames\n");
  leaf_capture_fn = CaptureLeafWSkip;
  CheckStackTrace(0);
  printf("PASS\n");

#if TEST_UCONTEXT_BITS
  if (!skipping_ucontext) {
    leaf_capture_fn = CaptureLeafUContext;
    CheckStackTrace(0);
    printf("PASS\n");
  }
#endif  // TEST_UCONTEXT_BITS
}

extern "C" {
const char* TEST_bump_stacktrace_implementation(const char*);
}

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--skip-ucontext") == 0) {
    argc--;
    argv--;
    skipping_ucontext = true;
  }

  for (;;) {
    // first arg if given is stacktrace implementation we want to test
    const char* name = TEST_bump_stacktrace_implementation((argc > 1) ? argv[1] : nullptr);
    if (!name) {
      break;
    }
    printf("Testing stacktrace implementation: %s\n", name);

    leaf_capture_len = 20;
    RunTest();

    printf("\nSet max capture length to 3:\n");
    leaf_capture_len = 3;  // less than stack depth
    RunTest();
  }

  return 0;
}
