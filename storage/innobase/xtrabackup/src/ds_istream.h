/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

Helper class to work with two buffers as single stream.

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

#ifndef DS_ISTREAM_H
#define DS_ISTREAM_H

#include <my_base.h>
#include <my_byteorder.h>
#include <my_sys.h>
#include <mysql/service_mysql_alloc.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>

class Datasink_istream {
 public:
  class Buffer {
   public:
    Buffer() : buf(nullptr), len(0), pos(0) {}

    Buffer(const char *buf, size_t len) : buf(buf), len(len), pos(0) {}

    Buffer(const Buffer &&other)
        : buf(other.buf), len(other.len), pos(other.pos) {}

    Buffer &operator=(const Buffer &&other) {
      buf = other.buf;
      len = other.len;
      pos = other.pos;
      return *this;
    }

    const char *ptr() const { return buf + pos; }

    size_t size() const { return len; }

    size_t length() const { return size() - pos; }

    size_t move_pos(size_t n) {
      n = std::min(length(), n);
      pos += n;
      return n;
    }

    size_t get_pos() const { return pos; }

    void set_pos(size_t n) { pos = n; }

    const char *get_buf() const { return buf; }

    void set_buf(const char *buf) { this->buf = buf; }

   private:
    const char *buf;
    size_t len;
    size_t pos;
  };

  Datasink_istream()
      : buffers(),
        cur(0),
        top(0),
        saved_cur(0),
        saved_pos(0),
        alloc_buf(nullptr),
        alloc_buf_size(0),
        remain_buf(nullptr),
        remain_buf_size(0),
        remain_buf_len(0) {}

  ~Datasink_istream() {
    my_free(alloc_buf);
    my_free(remain_buf);
  }

  void reset() {
    const auto len = length();
    if (len > 0) {
      if (remain_buf_size < len) {
        bool fix_first_buf = (buffers[0].get_buf() == remain_buf);
        remain_buf_size = len + buffers[0].size() - buffers[0].length();
        remain_buf = static_cast<char *>(
            my_realloc(PSI_NOT_INSTRUMENTED, remain_buf, remain_buf_size,
                       MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
        if (fix_first_buf) {
          buffers[0].set_buf(remain_buf);
        }
      }
      read_bytes(remain_buf, len);
      buffers[0] = {remain_buf, len};
      top = 1;
      cur = 0;
    } else {
      top = 0;
      cur = 0;
    }
    remain_buf_len = len;
  }

  void add_buffer(const char *buf, size_t len) { buffers[top++] = {buf, len}; }

  bool read_u8(uint8_t *r) {
    if (contiguous_length() < 1) return false;
    *r = *ptr();
    move_ptr(1);
    return true;
  }

  bool read_u32_le(uint32_t *r) {
    if (contiguous_length() >= 4) {
      *r = uint4korr(ptr());
      move_ptr(4);
      return true;
    }
    if (length() < 4) return false;
    char tmp[4];
    read_bytes(tmp, 4);
    *r = uint4korr(tmp);
    return true;
  }

  bool read_u64_le(uint64_t *r) {
    if (contiguous_length() >= 8) {
      *r = uint8korr(ptr());
      move_ptr(8);
      return true;
    }
    if (length() < 8) return false;
    char tmp[8];
    read_bytes(tmp, 8);
    *r = uint8korr(tmp);
    return true;
  }

  size_t read_bytes(char *dest, size_t n) {
    size_t r = 0;
    while (n > 0 && !empty()) {
      size_t k = std::min(n, contiguous_length());
      r += k;
      n -= k;
      memmove(dest, ptr(), k);
      dest += k;
      buffers[cur].move_pos(k);
      if (buffers[cur].length() == 0) {
        ++cur;
      }
    }
    return r;
  }

  size_t move_ptr(size_t n) {
    size_t r = 0;
    while (n > 0 && !empty()) {
      size_t k = std::min(n, contiguous_length());
      r += k;
      n -= k;
      buffers[cur].move_pos(k);
      if (buffers[cur].length() == 0) {
        ++cur;
      }
    }
    return r;
  }

  const char *ptr(size_t n) {
    if (n <= contiguous_length()) {
      const auto r = ptr();
      move_ptr(n);
      return r;
    }
    if (n <= length()) {
      if (alloc_buf_size < n) {
        alloc_buf_size = n;
        alloc_buf =
            static_cast<char *>(my_realloc(PSI_NOT_INSTRUMENTED, alloc_buf, n,
                                           MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
      }
      read_bytes(alloc_buf, n);
      return alloc_buf;
    }
    return nullptr;
  }

  bool empty() const { return (contiguous_length() == 0); }

  size_t length() const {
    size_t len = contiguous_length();
    for (size_t i = cur + 1; i < top; ++i) {
      len += buffers[i].size();
    }
    return len;
  }

  size_t contiguous_length() const { return buffers[cur].length(); }

  void save_pos() {
    saved_pos = buffers[cur].get_pos();
    saved_cur = cur;
  }

  void restore_pos() {
    cur = saved_cur;
    buffers[cur].set_pos(saved_pos);
    for (size_t i = cur + 1; i < top; ++i) {
      buffers[i].set_pos(0);
    }
  }

  const char *ptr() const { return buffers[cur].ptr(); }

 private:
  Buffer buffers[3];
  size_t cur;
  size_t top;
  size_t saved_cur;
  size_t saved_pos;
  char *alloc_buf;
  size_t alloc_buf_size;
  char *remain_buf;
  size_t remain_buf_size;
  size_t remain_buf_len;
};

#endif
