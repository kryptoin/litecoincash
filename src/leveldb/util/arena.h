// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include "port/port.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace leveldb {
class Arena {
public:
  Arena();
  ~Arena();

  char *Allocate(size_t bytes);

  char *AllocateAligned(size_t bytes);

  size_t MemoryUsage() const {
    return reinterpret_cast<uintptr_t>(memory_usage_.NoBarrier_Load());
  }

private:
  char *AllocateFallback(size_t bytes);
  char *AllocateNewBlock(size_t block_bytes);

  char *alloc_ptr_;
  size_t alloc_bytes_remaining_;

  std::vector<char *> blocks_;

  port::AtomicPointer memory_usage_;

  Arena(const Arena &);
  void operator=(const Arena &);
};

inline char *Arena::Allocate(size_t bytes) {
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char *result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}

} // namespace leveldb

#endif
