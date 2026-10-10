// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_H_

#include "leveldb/iterator.h"
#include <stddef.h>
#include <stdint.h>

namespace leveldb {
struct BlockContents;
class Comparator;

class Block {
public:
  explicit Block(const BlockContents &contents);

  ~Block();

  size_t size() const { return size_; }
  Iterator *NewIterator(const Comparator *comparator);

private:
  uint32_t NumRestarts() const;

  const char *data_;
  size_t size_;
  uint32_t restart_offset_;

  bool owned_;

  Block(const Block &);
  void operator=(const Block &);

  class Iter;
};

} // namespace leveldb

#endif
