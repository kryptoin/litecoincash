// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <stdint.h>

namespace leveldb {
class WritableFile;

namespace log {
class Writer {
public:
  explicit Writer(WritableFile *dest);

  Writer(WritableFile *dest, uint64_t dest_length);

  ~Writer();

  Status AddRecord(const Slice &slice);

private:
  WritableFile *dest_;
  int block_offset_;

  uint32_t type_crc_[kMaxRecordType + 1];

  Status EmitPhysicalRecord(RecordType type, const char *ptr, size_t length);

  Writer(const Writer &);
  void operator=(const Writer &);
};

} // namespace log

} // namespace leveldb

#endif
