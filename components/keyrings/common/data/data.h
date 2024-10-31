/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DATA_INCLUDED
#define DATA_INCLUDED

#include <cstdint>
#include <functional>
#include <string>
#include "pfs_string.h"

namespace keyring_common::data {

/** Data types */
using Type = pfs_string;

struct Sensitive_data {
  Sensitive_data() {}

  Sensitive_data(pfs_string const &str) : data(str) { encode(); }

  Sensitive_data(const char *str) : data(str) { encode(); }

  Sensitive_data(const char *str, std::size_t len) : data(str, len) {
    encode();
  }

  Sensitive_data(Sensitive_data &&o) : data(o.decode()) { encode(); }

  Sensitive_data(Sensitive_data const &o) : data(o.decode()) { encode(); }

  Sensitive_data &operator=(Sensitive_data &&o) {
    data = o.decode();
    encode();
    return *this;
  }

  Sensitive_data &operator=(Sensitive_data const &o) {
    data = o.decode();
    encode();
    return *this;
  }

  std::size_t size() const { return data.size(); }
  std::size_t length() const { return data.size(); }

  pfs_string decode() const {
    auto ret = data;
    const auto key =
        std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(this));
    for (auto &c : ret) {
      c ^= key;
    }
    return ret;
  }

  friend bool operator==(Sensitive_data const &a, Sensitive_data const &b) {
    return a.data == b.data;
  }

 private:
  void encode() {
    for (auto &c : data) {
      const auto key =
          std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(this));
      c ^= key;
    }
  }

  pfs_string data;
};

/**
  Sensitive data storage
*/

class Data {
 public:
  Data(const Sensitive_data &data, Type type);
  Data();
  explicit Data(Type type);
  Data(const Data &src);
  Data(Data &&src) noexcept;
  Data &operator=(const Data &src);
  Data &operator=(Data &&src) noexcept;

  virtual ~Data();

  virtual Data get_data() const;

  Sensitive_data data() const;

  Type type() const;

  bool valid() const;

  void set_data(const Sensitive_data &data);

  virtual void set_data(const Data &src);

  void set_type(Type type);

  bool operator==(const Data &other) const;

 protected:
  void set_validity();
  /** Sensitive data */
  Sensitive_data data_;
  /** Data type */
  Type type_;
  /** Validity of Data object */
  bool valid_{false};
};

}  // namespace keyring_common::data

#endif  // !DATA_INCLUDED
