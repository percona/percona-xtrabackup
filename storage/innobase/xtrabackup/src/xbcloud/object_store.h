/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

Object Store interface.

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

#ifndef XBCLOUD_OBJECT_STORE
#define XBCLOUD_OBJECT_STORE

#include <functional>
#include <string>
#include <vector>

#include "http.h"

namespace xbcloud {

class Event_handler;

class Object_store {
 public:
  virtual bool create_container(const std::string &name) = 0;
  virtual bool container_exists(const std::string &name, bool &exists) = 0;
  virtual bool list_objects_in_directory(const std::string &container,
                                         const std::string &directory,
                                         std::vector<std::string> &objects) = 0;
  virtual bool upload_object(const std::string &container,
                             const std::string &object,
                             const Http_buffer &contents) = 0;
  virtual bool async_upload_object(
      const std::string &container, const std::string &object,
      const Http_buffer &contents, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) = 0;
  virtual bool async_download_object(
      const std::string &container, const std::string &object, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) = 0;
  virtual bool async_delete_object(const std::string &container,
                                   const std::string &object, Event_handler *h,
                                   std::function<void(bool)> f = {}) = 0;
  /**
   * Delete a single object.
   *
   * @param container    Container/bucket name.
   * @param name         Object name.
   * @param best_effort  When true, an object we cannot delete because it is
   *                     not there, or because we have no permission on it,
   *                     is a success and is not logged.
   * @return true on success, false on error.
   */
  virtual bool delete_object(const std::string &container,
                             const std::string &name, bool best_effort) = 0;
  virtual Http_buffer download_object(const std::string &container,
                                      const std::string &name,
                                      bool &success) = 0;
  /**
   * List objects under a directory prefix and split them into files and dirs.
   *
   * Default implementation treats all returned objects as files.
   *
   * @param container Container/bucket name.
   * @param directory Directory prefix to list.
   * @param files Output list of file objects.
   * @param dirs Output list of directory objects (may be empty).
   * @return true on success, false on error.
   */
  virtual bool list_objects_files_and_dirs(const std::string &container,
                                           const std::string &directory,
                                           std::vector<std::string> &files,
                                           std::vector<std::string> &dirs);
  /**
   * Normalize a backup name according to storage-specific rules.
   *
   * Default implementation returns the name unchanged.
   *
   * @param name Backup name to normalize.
   * @return Normalized name.
   */
  virtual std::string normalize_name(const std::string &name) { return name; }
  virtual ~Object_store() {}
};

}  // namespace xbcloud

#endif  // XBCLOUD_OBJECT_STORE
