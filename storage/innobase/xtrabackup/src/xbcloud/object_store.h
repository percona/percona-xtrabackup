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

#ifndef __XBCLOUD_OBJECT_STORE__
#define __XBCLOUD_OBJECT_STORE__

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
  virtual bool async_upload_object(const std::string &container,
                                   const std::string &object,
                                   const Http_buffer &contents,
                                   Event_handler *h,
                                   std::function<void(bool)> f = {}) = 0;
  virtual bool async_download_object(
      const std::string &container, const std::string &object, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) = 0;
  virtual bool async_delete_object(const std::string &container,
                                   const std::string &object, Event_handler *h,
                                   std::function<void(bool)> f = {}) = 0;
  virtual bool delete_object(const std::string &container,
                             const std::string &name) = 0;
  virtual Http_buffer download_object(const std::string &container,
                                      const std::string &name,
                                      bool &success) = 0;
  virtual ~Object_store(){};
};

}  // namespace xbcloud

#endif
