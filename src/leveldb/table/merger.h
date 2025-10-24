// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_MERGER_H_
#define STORAGE_LEVELDB_TABLE_MERGER_H_

namespace leveldb {
class Comparator;
class Iterator;

extern Iterator *NewMergingIterator(const Comparator *comparator,
                                    Iterator **children, int n);

} // namespace leveldb

#endif
