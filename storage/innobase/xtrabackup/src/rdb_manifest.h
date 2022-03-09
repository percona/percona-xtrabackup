/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#ifndef XTRABACKUP_RDB_MANIFEST_H
#define XTRABACKUP_RDB_MANIFEST_H
#include <string>
#include <unordered_set>
#include "datasink.h"
#include <rapidjson/fwd.h>

class RdbManifest {
public:
    RdbManifest(int highestSstSeqNo = 0);

    bool serialize(ds_ctxt_t *ds) const;
    bool deserialize(const std::string &dir);

    const std::unordered_set<std::string>& GetSstFiles() const;
    void AddSstFile(const std::string &filename);

    uint64_t GetHighestSstSeqNo() const;


    static uint64_t GetSstFileSeqNo(const std::string &filename);

private:
    bool serialize(rapidjson::StringBuffer &buf) const;
    std::unordered_set<std::string> sstFiles_;
    uint64_t highestSstSeqNo_;
};



#endif  /* XTRABACKUP_RDB_MANIFEST_H */