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

#include "meta.h"

namespace keyring_common::meta {

/** Constructor */
Metadata::Metadata(std::string key_id, std::string owner_id)
    : key_id_(std::move(key_id)), owner_id_(std::move(owner_id)) {
  valid_ = !(key_id_.empty() && owner_id_.empty());
  create_hash_key();
}

Metadata::Metadata(const char *key_id, const char *owner_id)
    : Metadata(key_id != nullptr ? std::string{key_id} : std::string{},
               owner_id != nullptr ? std::string{owner_id} : std::string{}) {}

Metadata::Metadata() : Metadata(std::string{}, std::string{}) {}

/** Copy constructor */
Metadata::Metadata(const Metadata &src)
    : Metadata(src.key_id_, src.owner_id_) {}

/** Move constructor */
Metadata::Metadata(Metadata &&src) noexcept {
  std::swap(src.key_id_, key_id_);
  std::swap(src.owner_id_, owner_id_);
  std::swap(src.hash_key_, hash_key_);
  std::swap(src.valid_, valid_);
}

/** Assignment operator */
Metadata &Metadata::operator=(const Metadata &src) = default;

Metadata &Metadata::operator=(Metadata &&src) noexcept {
  std::swap(src.key_id_, key_id_);
  std::swap(src.owner_id_, owner_id_);
  std::swap(src.hash_key_, hash_key_);
  std::swap(src.valid_, valid_);

  return *this;
}

/** Destructor */
Metadata::~Metadata() { valid_ = false; }

/** Get key ID */
std::string Metadata::key_id() const { return key_id_; }

/** Get owner info */
std::string Metadata::owner_id() const { return owner_id_; }

/** Validity of metadata object */
bool Metadata::valid() const { return valid_; }

/** create hash key */
void Metadata::create_hash_key() {
  if (valid_) {
    hash_key_ = key_id_;
    if (!owner_id_.empty()) {
      hash_key_.push_back('\0');
      hash_key_.append(owner_id_);
    }
  }
}

}  // namespace keyring_common::meta
