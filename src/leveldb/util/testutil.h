// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_TESTUTIL_H_
#define STORAGE_LEVELDB_UTIL_TESTUTIL_H_

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "util/random.h"

namespace leveldb {
namespace test {
extern Slice RandomString(Random *rnd, int len, std::string *dst);

extern std::string RandomKey(Random *rnd, int len);

extern Slice CompressibleString(Random *rnd, double compressed_fraction,
                                size_t len, std::string *dst);

class ErrorEnv : public EnvWrapper {
public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  ErrorEnv()
      : EnvWrapper(Env::Default()), writable_file_error_(false),
        num_writable_file_errors_(0) {}

  virtual Status NewWritableFile(const std::string &fname,
                                 WritableFile **result) {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = NULL;
      return Status::IOError(fname, "fake error");
    }
    return target()->NewWritableFile(fname, result);
  }

  virtual Status NewAppendableFile(const std::string &fname,
                                   WritableFile **result) {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = NULL;
      return Status::IOError(fname, "fake error");
    }
    return target()->NewAppendableFile(fname, result);
  }
};

} // namespace test

} // namespace leveldb

#endif
