/* Copyright (c) 2015, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BOOTSTRAP_IMPL_H
#define BOOTSTRAP_IMPL_H 1
#include <string>
#include "sql_bootstrap.h"       // MAX_BOOTSTRAP_QUERY_SIZE


/** abstract interface to reading bootstrap commands */
class Command_iterator
{
public:
  /** start processing the iterator */
  virtual void begin(void) {}

  /**
    Get the next query string.

    @arg query output parameter to return the query
    @return one of the READ_BOOTSTRAP
  */
  virtual int next(std::string &query, int *read_error)= 0;

  /** end processing the iterator */
  virtual void end(void) {}

  /** The current bootstrap command reader */
  static Command_iterator *current_iterator;

protected:
  /** needed because of the virtual functions */
  virtual ~Command_iterator() {}
};

/** File bootstrap command reader */
class File_command_iterator : public Command_iterator
{
public:
  File_command_iterator(fgets_input_t input, fgets_fn_t fgets_fn)
    : m_input(input), m_fgets_fn(fgets_fn), is_allocated(false)
  {}
  File_command_iterator(const char *file_name);
  virtual ~File_command_iterator();

  int next(std::string &query, int *read_error);
  void end(void);
  bool has_file()
  {
    return m_input != 0;
  }
protected:
  fgets_input_t m_input;
  fgets_fn_t m_fgets_fn;
  bool is_allocated;
};


#endif /* BOOTSTRAP_IMPL_H */
