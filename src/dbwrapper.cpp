// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dbwrapper.h>

#include <random.h>

#include <algorithm>
#include <leveldb/cache.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <memenv.h>
#include <stdint.h>

class CBitcoinLevelDBLogger : public leveldb::Logger {
public:
  void Logv(const char *format, va_list ap) override {
    if (!LogAcceptCategory(BCLog::LEVELDB)) {
      return;
    }
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char *base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }
      char *p = base;
      char *limit = base + bufsize;

      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);

        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      if (p >= limit) {
        if (iter == 0) {
          continue;

        } else {
          p = limit - 1;
        }
      }

      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      base[std::min(bufsize - 1, (int)(p - base))] = '\0';
      LogPrintf("leveldb: %s", base);
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
};

static leveldb::Options GetOptions(size_t nCacheSize) {
  leveldb::Options options;
  options.block_cache = leveldb::NewLRUCache(nCacheSize / 2);
  options.write_buffer_size = nCacheSize / 4;

  options.filter_policy = leveldb::NewBloomFilterPolicy(10);
  options.compression = leveldb::kNoCompression;
  options.max_open_files = 64;
  options.info_log = new CBitcoinLevelDBLogger();
  if (leveldb::kMajorVersion > 1 ||
      (leveldb::kMajorVersion == 1 && leveldb::kMinorVersion >= 16)) {
    options.paranoid_checks = true;
  }
  return options;
}

CDBWrapper::CDBWrapper(const fs::path &path, size_t nCacheSize, bool fMemory,
                       bool fWipe, bool obfuscate) {
  penv = nullptr;
  readoptions.verify_checksums = true;
  iteroptions.verify_checksums = true;
  iteroptions.fill_cache = false;
  syncoptions.sync = true;
  options = GetOptions(nCacheSize);
  options.create_if_missing = true;
  if (fMemory) {
    penv = leveldb::NewMemEnv(leveldb::Env::Default());
    options.env = penv;
  } else {
    if (fWipe) {
      LogPrintf("Wiping LevelDB in %s\n", path.string());
      leveldb::Status result = leveldb::DestroyDB(path.string(), options);
      dbwrapper_private::HandleError(result);
    }
    TryCreateDirectories(path);
    LogPrintf("Opening LevelDB in %s\n", path.string());
  }
  leveldb::Status status = leveldb::DB::Open(options, path.string(), &pdb);
  dbwrapper_private::HandleError(status);
  LogPrintf("Opened LevelDB successfully\n");

  if (gArgs.GetBoolArg("-forcecompactdb", false)) {
    LogPrintf("Starting database compaction of %s\n", path.string());
    pdb->CompactRange(nullptr, nullptr);
    LogPrintf("Finished database compaction of %s\n", path.string());
  }

  obfuscate_key = std::vector<unsigned char>(OBFUSCATE_KEY_NUM_BYTES, '\000');

  bool key_exists = Read(OBFUSCATE_KEY_KEY, obfuscate_key);

  if (!key_exists && obfuscate && IsEmpty()) {
    std::vector<unsigned char> new_key = CreateObfuscateKey();

    Write(OBFUSCATE_KEY_KEY, new_key);
    obfuscate_key = new_key;

    LogPrintf("Wrote new obfuscate key for %s: %s\n", path.string(),
              HexStr(obfuscate_key));
  }

  LogPrintf("Using obfuscation key for %s: %s\n", path.string(),
            HexStr(obfuscate_key));
}

CDBWrapper::~CDBWrapper() {
  delete pdb;
  pdb = nullptr;
  delete options.filter_policy;
  options.filter_policy = nullptr;
  delete options.info_log;
  options.info_log = nullptr;
  delete options.block_cache;
  options.block_cache = nullptr;
  delete penv;
  options.env = nullptr;
}

bool CDBWrapper::WriteBatch(CDBBatch &batch, bool fSync) {
  leveldb::Status status =
      pdb->Write(fSync ? syncoptions : writeoptions, &batch.batch);
  dbwrapper_private::HandleError(status);
  return true;
}

const std::string CDBWrapper::OBFUSCATE_KEY_KEY("\000obfuscate_key", 14);

const unsigned int CDBWrapper::OBFUSCATE_KEY_NUM_BYTES = 8;

std::vector<unsigned char> CDBWrapper::CreateObfuscateKey() const {
  unsigned char buff[OBFUSCATE_KEY_NUM_BYTES];
  GetRandBytes(buff, OBFUSCATE_KEY_NUM_BYTES);
  return std::vector<unsigned char>(&buff[0], &buff[OBFUSCATE_KEY_NUM_BYTES]);
}

bool CDBWrapper::IsEmpty() {
  std::unique_ptr<CDBIterator> it(NewIterator());
  it->SeekToFirst();
  return !(it->Valid());
}

CDBIterator::~CDBIterator() { delete piter; }
bool CDBIterator::Valid() const { return piter->Valid(); }
void CDBIterator::SeekToFirst() { piter->SeekToFirst(); }
void CDBIterator::Next() { piter->Next(); }

namespace dbwrapper_private {
void HandleError(const leveldb::Status &status) {
  if (status.ok())
    return;
  LogPrintf("%s\n", status.ToString());
  if (status.IsCorruption())
    throw dbwrapper_error("Database corrupted");
  if (status.IsIOError())
    throw dbwrapper_error("Database I/O error");
  if (status.IsNotFound())
    throw dbwrapper_error("Database entry missing");
  throw dbwrapper_error("Unknown database error");
}

const std::vector<unsigned char> &GetObfuscateKey(const CDBWrapper &w) {
  return w.obfuscate_key;
}

} // namespace dbwrapper_private
