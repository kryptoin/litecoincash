// Copyright (c) 2014 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
#define STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_

#include "leveldb/env.h"
#include "leveldb/status.h"
#include <string>

namespace leveldb {
Status DumpFile(Env *env, const std::string &fname, WritableFile *dst);

}

#endif
