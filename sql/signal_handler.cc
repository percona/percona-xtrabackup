/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_global.h"
#include <signal.h>

#include "sys_vars.h"
#include "my_stacktrace.h"
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "mysqld_thd_manager.h"          // Global_THD_manager
#include "sql_class.h"

#ifdef _WIN32
#include <crtdbg.h>
#define SIGNAL_FMT "exception 0x%x"
#else
#define SIGNAL_FMT "signal %d"
#endif

/*
  We are handling signals in this file.
  Any global variables we read should be 'volatile sig_atomic_t'
  to guarantee that we read some consistent value.
 */
static volatile sig_atomic_t segfaulted= 0;
extern volatile sig_atomic_t calling_initgroups;

/**
 * Handler for fatal signals
 *
 * Fatal events (seg.fault, bus error etc.) will trigger
 * this signal handler.  The handler will try to dump relevant
 * debugging information to stderr and dump a core image.
 *
 * Signal handlers can only use a set of 'safe' system calls
 * and library functions.  A list of safe calls in POSIX systems
 * are available at:
 *  http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
 * For MS Windows, guidelines are available at:
 *  http://msdn.microsoft.com/en-us/library/xdkz3x12(v=vs.71).aspx
 *
 * @param sig Signal number
*/
extern "C" void handle_fatal_signal(int sig)
{
  if (segfaulted)
  {
    my_safe_printf_stderr("Fatal " SIGNAL_FMT " while backtracing\n", sig);
    _exit(MYSQLD_FAILURE_EXIT); /* Quit without running destructors */
  }

  segfaulted = 1;

#ifdef _WIN32
  SYSTEMTIME utc_time;
  GetSystemTime(&utc_time);
  const long hrs=  utc_time.wHour;
  const long mins= utc_time.wMinute;
  const long secs= utc_time.wSecond;
#else
  /* Using time() instead of my_time() to avoid looping */
  const time_t curr_time= time(NULL);
  /* Calculate time of day */
  const long tmins = curr_time / 60;
  const long thrs  = tmins / 60;
  const long hrs   = thrs  % 24;
  const long mins  = tmins % 60;
  const long secs  = curr_time % 60;
#endif

  char hrs_buf[3]= "00";
  char mins_buf[3]= "00";
  char secs_buf[3]= "00";
  my_safe_itoa(10, hrs, &hrs_buf[2]);
  my_safe_itoa(10, mins, &mins_buf[2]);
  my_safe_itoa(10, secs, &secs_buf[2]);

  my_safe_printf_stderr("%s:%s:%s UTC - xtrabackup got " SIGNAL_FMT " ;\n",
                        hrs_buf, mins_buf, secs_buf, sig);

  my_safe_printf_stderr("%s",
    "This could be because you hit a bug or data is corrupted.\n"
    "This error can also be caused by malfunctioning hardware.\n");

  my_safe_printf_stderr("%s",
    "Attempting to collect some information that could help diagnose the problem.\n"
    "As this is a crash and something is definitely wrong, the information\n"
    "collection process might fail.\n\n");

#ifdef HAVE_STACKTRACE
  THD *thd= my_thread_get_THR_THD();

  if (!(test_flags & TEST_NO_STACKTRACE))
  {
    my_safe_printf_stderr("Thread pointer: 0x%p\n", thd);
    my_safe_printf_stderr("%s",
      "Attempting backtrace. You can use the following "
      "information to find out\n"
      "where mysqld died. If you see no messages after this, something went\n"
      "terribly wrong...\n");
    my_print_stacktrace(thd ? (uchar*) thd->thread_stack : NULL,
                        my_thread_stack_size);
  }
  my_safe_printf_stderr("%s", "\n"
    "Please report a bug at https://jira.percona.com/projects/PXB\n");

#endif /* HAVE_STACKTRACE */

#ifdef HAVE_INITGROUPS
  if (calling_initgroups)
  {
    my_safe_printf_stderr("%s", "\n"
      "This crash occured while the server was calling initgroups(). This is\n"
      "often due to the use of a mysqld that is statically linked against \n"
      "glibc and configured to use LDAP in /etc/nsswitch.conf.\n"
      "You will need to either upgrade to a version of glibc that does not\n"
      "have this problem (2.3.4 or later when used with nscd),\n"
      "disable LDAP in your nsswitch.conf, or use a "
      "mysqld that is not statically linked.\n");
  }
#endif

  if (locked_in_memory)
  {
    my_safe_printf_stderr("%s", "\n"
      "The \"--memlock\" argument, which was enabled, "
      "uses system calls that are\n"
      "unreliable and unstable on some operating systems and "
      "operating-system versions (notably, some versions of Linux).\n"
      "This crash could be due to use of those buggy OS calls.\n"
      "You should consider whether you really need the "
      "\"--memlock\" parameter and/or consult the OS distributer about "
      "\"mlockall\" bugs.\n");
  }

  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    my_safe_printf_stderr("%s", "Writing a core file\n");
    my_write_core(sig);
  }

#ifndef _WIN32
  /*
     Quit, without running destructors (etc.)
     On Windows, do not terminate, but pass control to exception filter.
  */
  _exit(MYSQLD_FAILURE_EXIT);  // Using _exit(), since exit() is not async signal safe
#endif
}
