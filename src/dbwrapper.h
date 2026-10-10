// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DBWRAPPER_H
#define BITCOIN_DBWRAPPER_H

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <streams.h>
#include <util.h>
#include <utilstrencodings.h>
#include <version.h>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

class dbwrapper_error : public std::runtime_error {
public:
  explicit dbwrapper_error(const std::string &msg) : std::runtime_error(msg) {}
};

class CDBWrapper;

namespace dbwrapper_private {
void HandleError(const leveldb::Status &status);

const std::vector<unsigned char> &GetObfuscateKey(const CDBWrapper &w);

}; // namespace dbwrapper_private

class CDBBatch {
  friend class CDBWrapper;

private:
  const CDBWrapper &parent;
  leveldb::WriteBatch batch;

  CDataStream ssKey;
  CDataStream ssValue;

  size_t size_estimate;

public:
  explicit CDBBatch(const CDBWrapper &_parent)
      : parent(_parent), ssKey(SER_DISK, CLIENT_VERSION),
        ssValue(SER_DISK, CLIENT_VERSION), size_estimate(0) {};

  void Clear() {
    batch.Clear();
    size_estimate = 0;
  }

  template <typename K, typename V> void Write(const K &key, const V &value) {
    ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey << key;
    leveldb::Slice slKey(ssKey.data(), ssKey.size());

    ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
    ssValue << value;
    ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
    leveldb::Slice slValue(ssValue.data(), ssValue.size());

    batch.Put(slKey, slValue);

    size_estimate += 3 + (slKey.size() > 127) + slKey.size() +
                     (slValue.size() > 127) + slValue.size();
    ssKey.clear();
    ssValue.clear();
  }

  template <typename K> void Erase(const K &key) {
    ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey << key;
    leveldb::Slice slKey(ssKey.data(), ssKey.size());

    batch.Delete(slKey);

    size_estimate += 2 + (slKey.size() > 127) + slKey.size();
    ssKey.clear();
  }

  size_t SizeEstimate() const { return size_estimate; }
};

class CDBIterator {
private:
  const CDBWrapper &parent;
  leveldb::Iterator *piter;

public:
  CDBIterator(const CDBWrapper &_parent, leveldb::Iterator *_piter)
      : parent(_parent), piter(_piter) {};
  ~CDBIterator();

  bool Valid() const;

  void SeekToFirst();

  template <typename K> void Seek(const K &key) {
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey << key;
    leveldb::Slice slKey(ssKey.data(), ssKey.size());
    piter->Seek(slKey);
  }

  void Next();

  template <typename K> bool GetKey(K &key) {
    leveldb::Slice slKey = piter->key();
    try {
      CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK,
                        CLIENT_VERSION);
      ssKey >> key;
    } catch (const std::exception &) {
      return false;
    }
    return true;
  }

  template <typename V> bool GetValue(V &value) {
    leveldb::Slice slValue = piter->value();
    try {
      CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(),
                          SER_DISK, CLIENT_VERSION);
      ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
      ssValue >> value;
    } catch (const std::exception &) {
      return false;
    }
    return true;
  }

  unsigned int GetValueSize() { return piter->value().size(); }
};

class CDBWrapper {
  friend const std::vector<unsigned char> &
  dbwrapper_private::GetObfuscateKey(const CDBWrapper &w);

private:
  leveldb::Env *penv;

  leveldb::Options options;

  leveldb::ReadOptions readoptions;

  leveldb::ReadOptions iteroptions;

  leveldb::WriteOptions writeoptions;

  leveldb::WriteOptions syncoptions;

  leveldb::DB *pdb;

  std::vector<unsigned char> obfuscate_key;

  static const std::string OBFUSCATE_KEY_KEY;

  static const unsigned int OBFUSCATE_KEY_NUM_BYTES;

  std::vector<unsigned char> CreateObfuscateKey() const;

public:
  CDBWrapper(const fs::path &path, size_t nCacheSize, bool fMemory = false,
             bool fWipe = false, bool obfuscate = false);
  ~CDBWrapper();

  template <typename K, typename V> bool Read(const K &key, V &value) const {
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey << key;
    leveldb::Slice slKey(ssKey.data(), ssKey.size());

    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (!status.ok()) {
      if (status.IsNotFound())
        return false;
      LogPrintf("LevelDB read failure: %s\n", status.ToString());
      dbwrapper_private::HandleError(status);
    }
    try {
      CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(),
                          SER_DISK, CLIENT_VERSION);
      ssValue.Xor(obfuscate_key);
      ssValue >> value;
    } catch (const std::exception &) {
      return false;
    }
    return true;
  }

  template <typename K, typename V>
  bool Write(const K &key, const V &value, bool fSync = false) {
    CDBBatch batch(*this);
    batch.Write(key, value);
    return WriteBatch(batch, fSync);
  }

  template <typename K> bool Exists(const K &key) const {
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey << key;
    leveldb::Slice slKey(ssKey.data(), ssKey.size());

    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (!status.ok()) {
      if (status.IsNotFound())
        return false;
      LogPrintf("LevelDB read failure: %s\n", status.ToString());
      dbwrapper_private::HandleError(status);
    }
    return true;
  }

  template <typename K> bool Erase(const K &key, bool fSync = false) {
    CDBBatch batch(*this);
    batch.Erase(key);
    return WriteBatch(batch, fSync);
  }

  bool WriteBatch(CDBBatch &batch, bool fSync = false);

  bool Flush() { return true; }

  bool Sync() {
    CDBBatch batch(*this);
    return WriteBatch(batch, true);
  }

  CDBIterator *NewIterator() {
    return new CDBIterator(*this, pdb->NewIterator(iteroptions));
  }

  bool IsEmpty();

  template <typename K>
  size_t EstimateSize(const K &key_begin, const K &key_end) const {
    CDataStream ssKey1(SER_DISK, CLIENT_VERSION),
        ssKey2(SER_DISK, CLIENT_VERSION);
    ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey1 << key_begin;
    ssKey2 << key_end;
    leveldb::Slice slKey1(ssKey1.data(), ssKey1.size());
    leveldb::Slice slKey2(ssKey2.data(), ssKey2.size());
    uint64_t size = 0;
    leveldb::Range range(slKey1, slKey2);
    pdb->GetApproximateSizes(&range, 1, &size);
    return size;
  }

  template <typename K>
  void CompactRange(const K &key_begin, const K &key_end) const {
    CDataStream ssKey1(SER_DISK, CLIENT_VERSION),
        ssKey2(SER_DISK, CLIENT_VERSION);
    ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
    ssKey1 << key_begin;
    ssKey2 << key_end;
    leveldb::Slice slKey1(ssKey1.data(), ssKey1.size());
    leveldb::Slice slKey2(ssKey2.data(), ssKey2.size());
    pdb->CompactRange(&slKey1, &slKey2);
  }
};

#endif
