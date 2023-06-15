/******************************************************
Copyright (c) 2014, 2023 Percona LLC and/or its affiliates.

xbcloud utility. Manage backups on cloud storage services.

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

struct file_metadata_t {
  my_off_t last_chunk;
  my_off_t next_chunk;
  std::string name;
};

/** struct to hold state of current per thread in progress files. This avoids
 * multiple chunks from the same file to be downloaded in parallel */
struct thread_state_t {
  std::mutex m;
  /* files with active chunks been downloaded (waiting http client to complete
   * the download) */
  std::set<std::string> in_progress_files;

  /* list of files allocated for this thread */
  std::unordered_map<std::string, file_metadata_t> file_list;

  /**
  Check if there is any file been downloaded.

  @return false if there are files been downloaded, true otherwise. */
  bool in_progress_files_empty() {
    std::lock_guard<std::mutex> g(m);
    return in_progress_files.empty();
  }

  /**
  Check the number of files been downloaded.

  @return Number of files in in_progress_files list. */
  my_off_t in_progress_files_size() {
    std::lock_guard<std::mutex> g(m);
    return in_progress_files.size();
  }

  /**
  Check if there is any file allocated for this thread.

  @return false if there are no allocated files, true otherwise. */
  bool file_list_empty() {
    std::lock_guard<std::mutex> g(m);
    return file_list.empty();
  }

  /**
  Get the next available file to be downloaded. The file is on file_list
  (allocated to this thread) and has no in progress chunks been downloaded.

  @param [in/out]  file  file available to download.

  @return true if a file has been allocated, false otherwise */
  bool next_file(file_metadata_t &file) {
    std::lock_guard<std::mutex> g(m);
    if (!file_list.empty()) {
      for (auto it : file_list) {
        if (in_progress_files.count(it.first) == 0) {
          file = it.second;
          return true;
        }
      }
    }
    return false;
  }

  /**
  Get the next chunk for a given file.

  @param [in]  file  file to get the next chunk.

  @return id of next available chunk */
  my_off_t next_chunk(file_metadata_t &file) {
    std::lock_guard<std::mutex> g(m);
    auto it = file_list.find(file.name);
    assert(it != file_list.end());
    return it->second.next_chunk;
  }

  /**
  Attempt to reserve ownsership of a file with intend to start downloading it.
  This will add the file into both in_progress_files list and file_list list.

  @param [in]  file  file to attempt to reserve.

  @return true if the file is not already in progress, false otherwise. */
  bool start_file(const file_metadata_t &file) {
    std::lock_guard<std::mutex> g(m);
    if (in_progress_files.count(file.name) > 0) {
      return false;
    }
    in_progress_files.insert(file.name);
    file_list.insert({file.name, file});
    return true;
  }

  /**
  Indicates the current chunk of a given file has been completed.
  This will remove the file from in_progress_files list. If this is the last
  chunk for this file, it will remove the file from file_list otherwise advance
  the next_chunk of the file.

  @param [in]  file  filen to complete chunk.
  @param [in]  idx   chunk index that we have completed. */
  void complete_chunk(const file_metadata_t &file, const my_off_t &idx) {
    std::lock_guard<std::mutex> g(m);
    in_progress_files.erase(file.name);
    auto it = file_list.find(file.name);
    assert(it != file_list.end());
    if (file.last_chunk == idx)
      file_list.erase(it);
    else
      it->second.next_chunk++;
  }
};

/* Thread safe global file list */
struct global_list_t {
  std::mutex m;
  /* Global list of files to be downloaded*/
  std::unordered_map<std::string, file_metadata_t> files;

  /**
  Check if the file list is empty.

  @return false if there are files in the list, true otherwise. */
  bool empty() {
    std::lock_guard<std::mutex> g(m);
    return files.empty();
  }

  /**
  Get the next available file to be downloaded from global list. The file
  will be removed from this list. The caller must allocate the file into
  its own thread file list.

  @param [in/out]  file  file available to download.

  @return true if a file has been allocated, false otherwise */
  bool next_file(file_metadata_t &file) {
    std::lock_guard<std::mutex> g(m);
    if (!files.empty()) {
      auto it = files.begin();
      file = it->second;
      files.erase(it);
      return true;
    }
    return false;
  }

  /**
  Adds a new file to global list of files or update the last_chunk if necessary.

  @param [in]  filename   filename of the new file.
  @param [in]  idx        chunk index of the file. */
  void add(std::string filename, size_t idx) {
    std::lock_guard<std::mutex> g(m);
    if (files.find(filename) == files.end()) {
      file_metadata_t file;
      file.name = filename;
      file.last_chunk = idx;
      file.next_chunk = 0;
      files.insert({filename, file});
    } else {
      if (files[filename].last_chunk < idx) files[filename].last_chunk = idx;
    }
  }
};
