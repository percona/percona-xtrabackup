#include <fstream>
#include "rdb_manifest.h"

const char *XBRDBMANIFEST_FILE_NAME = "xtrabackup_rdb_files";

bool RdbManifest::serialize(ds_ctxt_t *ds) const {
  const char *path = XBRDBMANIFEST_FILE_NAME;
  MY_STAT mystat;
//  mystat.st_size = buf.GetSize();
//  mystat.st_mtime = my_time(0);

  ds_file_t *stream = ds_open(ds, path, &mystat);
  if (stream == NULL) {
    return (false);
  }

  bool rc = true;

  for(auto file : sstFiles_) {
    auto file_ = file + "\n";
    if (ds_write(stream, file_.c_str(), file_.length())) {
        rc = false;
        break;
    }
  }

  if (ds_close(stream)) {
    rc = false;
  }

  return (rc);
}

bool RdbManifest::deserialize(const std::string &dir) {
  const std::string path = dir + "/" + XBRDBMANIFEST_FILE_NAME;
  std::ifstream f(path);

  std::string line;
  while (std::getline(f, line)) {
      sstFiles_.insert(line);
  }

  return true;
}

const std::unordered_set<std::string>& RdbManifest::GetSstFiles() const {
    return sstFiles_;
}

void RdbManifest::AddSstFile(const std::string &filename) {
    sstFiles_.insert(filename);
}
