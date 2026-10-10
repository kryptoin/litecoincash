// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_ITER_H_
#define STORAGE_LEVELDB_DB_DB_ITER_H_

#include "db/dbformat.h"
#include "leveldb/db.h"
#include <stdint.h>

namespace leveldb {
class DBImpl;

extern Iterator *NewDBIterator(DBImpl *db,
                               const Comparator *user_key_comparator,
                               Iterator *internal_iter, SequenceNumber sequence,
                               uint32_t seed);

} // namespace leveldb

#endif
