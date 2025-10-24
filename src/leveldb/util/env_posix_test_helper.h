// Copyright 2017 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_
#define STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_

namespace leveldb {
class EnvPosixTest;

class EnvPosixTestHelper {
private:
  friend class EnvPosixTest;

  static void SetReadOnlyFDLimit(int limit);

  static void SetReadOnlyMMapLimit(int limit);
};

} // namespace leveldb

#endif
