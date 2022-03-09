#include <fstream>
#include "rdb_manifest.h"
#include <my_systime.h>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <srv0srv.h>
#include "xb0xb.h"

const char *XBRDBMANIFEST_FILE_NAME = "xtrabackup_rdb_files";
const int XBRDBMANIFEST_FILE_VERSION = 1;

RdbManifest::RdbManifest(int highestSstSeqNo)
  : highestSstSeqNo_(highestSstSeqNo)
  {}

/** Serialize talespace list into a buffer.
@param[out]  buf output buffer */
bool RdbManifest::serialize(rapidjson::StringBuffer &buf) const {
  using Writer = rapidjson::Writer<rapidjson::StringBuffer>;

  Writer writer(buf);

  writer.StartObject();
  writer.String("version");
  writer.Int(XBRDBMANIFEST_FILE_VERSION);

  writer.String("sst_files");
  writer.StartArray();

  for (auto const &file : sstFiles_) {
    writer.String(file.c_str(), file.length());
  }
  writer.EndArray();

  writer.String("highest_sst_seqno");
  writer.Int(highestSstSeqNo_);
  writer.EndObject();

  return (true);
}

bool RdbManifest::serialize(ds_ctxt_t *ds) const {
  using rapidjson::StringBuffer;

  StringBuffer buf;

  bool rc = serialize(buf);
  if (!rc) {
    return (rc);
  }

  MY_STAT mystat;
  mystat.st_size = buf.GetSize();
  mystat.st_mtime = my_time(0);

  const char *path = XBRDBMANIFEST_FILE_NAME;
  ds_file_t *stream = ds_open(ds, path, &mystat);
  if (stream == NULL) {
    xb::error() << "cannot open output stream for " << path;
    return (false);
  }

  rc = true;

  if (ds_write(stream, buf.GetString(), buf.GetSize())) {
    rc = false;
  }

  if (ds_close(stream)) {
    rc = false;
  }

  return (rc);
}

bool RdbManifest::deserialize(const std::string &dir) {
  using rapidjson::Document;
  using rapidjson::IStreamWrapper;

  const std::string path = dir + "/" + XBRDBMANIFEST_FILE_NAME;
  std::ifstream f(path);

  if (f.fail()) {
    xb::error() << "cannot open file " << SQUOTE(path.c_str());
    return (false);
  }

  IStreamWrapper wrapper(f);

  Document doc;
  doc.ParseStream(wrapper);
  if (doc.HasParseError()) {
    xb::error() << "JSON parse error in file " << SQUOTE(path.c_str());
    return (false);
  }

  auto root = doc.GetObject();

  int version = root["version"].GetInt();
  if (version > XBRDBMANIFEST_FILE_VERSION) {
    xb::error() << "wrong " << SQUOTE(XBRDBMANIFEST_FILE_NAME) << " file version "
                << version << ", maximum version supported is "
                << XBRDBMANIFEST_FILE_VERSION;
    return (false);
  }

  auto list = root["sst_files"].GetArray();

  for (auto &entry : list) {
    const auto *sstFile = entry.GetString();
    AddSstFile(sstFile);
  }

  // Sanity check. Calculated should be the same as recorded.
  auto highestSstSeqNo = root["highest_sst_seqno"].GetUint64();
  return highestSstSeqNo == highestSstSeqNo_;
}

const std::unordered_set<std::string>& RdbManifest::GetSstFiles() const {
    return sstFiles_;
}

void RdbManifest::AddSstFile(const std::string &filename) {
    sstFiles_.insert(filename);

    auto seqno = GetSstFileSeqNo(filename);
    highestSstSeqNo_ = seqno > highestSstSeqNo_ ? seqno : highestSstSeqNo_;
}

uint64_t RdbManifest::GetHighestSstSeqNo() const {
    return highestSstSeqNo_;
}

uint64_t RdbManifest::GetSstFileSeqNo(const std::string &filename) {
    try {
      return std::stoull(filename.substr(0, filename.length()-strlen(".sst")));
    } catch(...){
      return -1;
    }
}