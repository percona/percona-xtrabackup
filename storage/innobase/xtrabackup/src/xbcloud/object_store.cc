/******************************************************
Copyright (c) 2026 Percona LLC and/or its affiliates.

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

#include "xbcloud/object_store.h"

namespace xbcloud {

/**
 * Default implementation that lists objects and treats all as files.
 *
 * @param container Container/bucket name.
 * @param directory Directory prefix to list.
 * @param files Output list of file objects.
 * @param dirs Output list of directory objects (unused here).
 * @return true on success, false on error.
 */
bool Object_store::list_objects_files_and_dirs(const std::string &container,
                                               const std::string &directory,
                                               std::vector<std::string> &files,
                                               std::vector<std::string> &dirs) {
  return list_objects_in_directory(container, directory, files);
}

}  // namespace xbcloud
