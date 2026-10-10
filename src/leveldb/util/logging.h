// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Must not be included from any .h files to avoid polluting the namespace
// with macros.

#ifndef STORAGE_LEVELDB_UTIL_LOGGING_H_
#define STORAGE_LEVELDB_UTIL_LOGGING_H_

#include "port/port.h"
#include <stdint.h>
#include <stdio.h>
#include <string>

namespace leveldb {
class Slice;
class WritableFile;

extern void AppendNumberTo(std::string *str, uint64_t num);

extern void AppendEscapedStringTo(std::string *str, const Slice &value);

extern std::string NumberToString(uint64_t num);

extern std::string EscapeString(const Slice &value);

extern bool ConsumeDecimalNumber(Slice *in, uint64_t *val);

} // namespace leveldb

#endif
