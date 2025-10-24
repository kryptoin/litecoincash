// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A filter block is stored near the end of a Table file.  It contains
// filters (e.g., bloom filters) for all data blocks in the table combined
// into a single filter block.

#ifndef STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_

#include "leveldb/slice.h"
#include "util/hash.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace leveldb {
class FilterPolicy;

class FilterBlockBuilder {
public:
  explicit FilterBlockBuilder(const FilterPolicy *);

  void StartBlock(uint64_t block_offset);
  void AddKey(const Slice &key);
  Slice Finish();

private:
  void GenerateFilter();

  const FilterPolicy *policy_;
  std::string keys_;

  std::vector<size_t> start_;

  std::string result_;

  std::vector<Slice> tmp_keys_;

  std::vector<uint32_t> filter_offsets_;

  FilterBlockBuilder(const FilterBlockBuilder &);
  void operator=(const FilterBlockBuilder &);
};

class FilterBlockReader {
public:
  FilterBlockReader(const FilterPolicy *policy, const Slice &contents);
  bool KeyMayMatch(uint64_t block_offset, const Slice &key);

private:
  const FilterPolicy *policy_;
  const char *data_;

  const char *offset_;

  size_t num_;

  size_t base_lg_;
};

} // namespace leveldb

#endif
