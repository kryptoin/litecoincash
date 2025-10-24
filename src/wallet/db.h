// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_DB_H
#define BITCOIN_WALLET_DB_H

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <streams.h>
#include <sync.h>
#include <version.h>

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include <db_cxx.h>

static const unsigned int DEFAULT_WALLET_DBLOGSIZE = 100;
static const bool DEFAULT_WALLET_PRIVDB = true;

class CDBEnv {
private:
  bool fDbEnvInit;
  bool fMockDb;

  std::string strPath;

  void EnvShutdown();

public:
  mutable CCriticalSection cs_db;
  std::unique_ptr<DbEnv> dbenv;
  std::map<std::string, int> mapFileUseCount;
  std::map<std::string, Db *> mapDb;

  CDBEnv();
  ~CDBEnv();
  void Reset();

  void MakeMock();
  bool IsMock() const { return fMockDb; }

  enum VerifyResult { VERIFY_OK, RECOVER_OK, RECOVER_FAIL };
  typedef bool (*recoverFunc_type)(const std::string &strFile,
                                   std::string &out_backup_filename);
  VerifyResult Verify(const std::string &strFile, recoverFunc_type recoverFunc,
                      std::string &out_backup_filename);

  typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char>>
      KeyValPair;
  bool Salvage(const std::string &strFile, bool fAggressive,
               std::vector<KeyValPair> &vResult);

  bool Open(const fs::path &path, bool retry = 0);
  void Close();
  void Flush(bool fShutdown);
  void CheckpointLSN(const std::string &strFile);

  void CloseDb(const std::string &strFile);

  DbTxn *TxnBegin(int flags = DB_TXN_WRITE_NOSYNC) {
    DbTxn *ptxn = nullptr;
    int ret = dbenv->txn_begin(nullptr, &ptxn, flags);
    if (!ptxn || ret != 0)
      return nullptr;
    return ptxn;
  }
};

extern CDBEnv bitdb;

class CWalletDBWrapper {
  friend class CDB;

public:
  CWalletDBWrapper()
      : nUpdateCounter(0), nLastSeen(0), nLastFlushed(0), nLastWalletUpdate(0),
        env(nullptr) {}

  CWalletDBWrapper(CDBEnv *env_in, const std::string &strFile_in)
      : nUpdateCounter(0), nLastSeen(0), nLastFlushed(0), nLastWalletUpdate(0),
        env(env_in), strFile(strFile_in) {}

  bool Rewrite(const char *pszSkip = nullptr);

  bool Backup(const std::string &strDest);

  std::string GetName() const { return strFile; }

  void Flush(bool shutdown);

  void IncrementUpdateCounter();

  std::atomic<unsigned int> nUpdateCounter;
  unsigned int nLastSeen;
  unsigned int nLastFlushed;
  int64_t nLastWalletUpdate;

private:
  CDBEnv *env;
  std::string strFile;

  bool IsDummy() { return env == nullptr; }
};

class CDB {
protected:
  Db *pdb;
  std::string strFile;
  DbTxn *activeTxn;
  bool fReadOnly;
  bool fFlushOnClose;
  CDBEnv *env;

public:
  explicit CDB(CWalletDBWrapper &dbw, const char *pszMode = "r+",
               bool fFlushOnCloseIn = true);
  ~CDB() { Close(); }

  CDB(const CDB &) = delete;
  CDB &operator=(const CDB &) = delete;

  void Flush();
  void Close();
  static bool Recover(const std::string &filename, void *callbackDataIn,
                      bool (*recoverKVcallback)(void *callbackData,
                                                CDataStream ssKey,
                                                CDataStream ssValue),
                      std::string &out_backup_filename);

  static bool PeriodicFlush(CWalletDBWrapper &dbw);

  static bool VerifyEnvironment(const std::string &walletFile,
                                const fs::path &walletDir,
                                std::string &errorStr);

  static bool VerifyDatabaseFile(const std::string &walletFile,
                                 const fs::path &walletDir,
                                 std::string &warningStr, std::string &errorStr,
                                 CDBEnv::recoverFunc_type recoverFunc);

public:
  template <typename K, typename T> bool Read(const K &key, T &value) {
    if (!pdb)
      return false;

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    Dbt datKey(ssKey.data(), ssKey.size());

    Dbt datValue;
    datValue.set_flags(DB_DBT_MALLOC);
    int ret = pdb->get(activeTxn, &datKey, &datValue, 0);
    memory_cleanse(datKey.get_data(), datKey.get_size());
    bool success = false;
    if (datValue.get_data() != nullptr) {
      try {
        CDataStream ssValue((char *)datValue.get_data(),
                            (char *)datValue.get_data() + datValue.get_size(),
                            SER_DISK, CLIENT_VERSION);
        ssValue >> value;
        success = true;
      } catch (const std::exception &) {
      }

      memory_cleanse(datValue.get_data(), datValue.get_size());
      free(datValue.get_data());
    }
    return ret == 0 && success;
  }

  template <typename K, typename T>
  bool Write(const K &key, const T &value, bool fOverwrite = true) {
    if (!pdb)
      return true;
    if (fReadOnly)
      assert(!"Write called on database in read-only mode");

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    Dbt datKey(ssKey.data(), ssKey.size());

    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(10000);
    ssValue << value;
    Dbt datValue(ssValue.data(), ssValue.size());

    int ret = pdb->put(activeTxn, &datKey, &datValue,
                       (fOverwrite ? 0 : DB_NOOVERWRITE));

    memory_cleanse(datKey.get_data(), datKey.get_size());
    memory_cleanse(datValue.get_data(), datValue.get_size());
    return (ret == 0);
  }

  template <typename K> bool Erase(const K &key) {
    if (!pdb)
      return false;
    if (fReadOnly)
      assert(!"Erase called on database in read-only mode");

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    Dbt datKey(ssKey.data(), ssKey.size());

    int ret = pdb->del(activeTxn, &datKey, 0);

    memory_cleanse(datKey.get_data(), datKey.get_size());
    return (ret == 0 || ret == DB_NOTFOUND);
  }

  template <typename K> bool Exists(const K &key) {
    if (!pdb)
      return false;

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    Dbt datKey(ssKey.data(), ssKey.size());

    int ret = pdb->exists(activeTxn, &datKey, 0);

    memory_cleanse(datKey.get_data(), datKey.get_size());
    return (ret == 0);
  }

  Dbc *GetCursor() {
    if (!pdb)
      return nullptr;
    Dbc *pcursor = nullptr;
    int ret = pdb->cursor(nullptr, &pcursor, 0);
    if (ret != 0)
      return nullptr;
    return pcursor;
  }

  int ReadAtCursor(Dbc *pcursor, CDataStream &ssKey, CDataStream &ssValue,
                   bool setRange = false) {
    Dbt datKey;
    unsigned int fFlags = DB_NEXT;
    if (setRange) {
      datKey.set_data(ssKey.data());
      datKey.set_size(ssKey.size());
      fFlags = DB_SET_RANGE;
    }
    Dbt datValue;
    datKey.set_flags(DB_DBT_MALLOC);
    datValue.set_flags(DB_DBT_MALLOC);
    int ret = pcursor->get(&datKey, &datValue, fFlags);
    if (ret != 0)
      return ret;
    else if (datKey.get_data() == nullptr || datValue.get_data() == nullptr)
      return 99999;

    ssKey.SetType(SER_DISK);
    ssKey.clear();
    ssKey.write((char *)datKey.get_data(), datKey.get_size());
    ssValue.SetType(SER_DISK);
    ssValue.clear();
    ssValue.write((char *)datValue.get_data(), datValue.get_size());

    memory_cleanse(datKey.get_data(), datKey.get_size());
    memory_cleanse(datValue.get_data(), datValue.get_size());
    free(datKey.get_data());
    free(datValue.get_data());
    return 0;
  }

public:
  bool TxnBegin() {
    if (!pdb || activeTxn)
      return false;
    DbTxn *ptxn = bitdb.TxnBegin();
    if (!ptxn)
      return false;
    activeTxn = ptxn;
    return true;
  }

  bool TxnCommit() {
    if (!pdb || !activeTxn)
      return false;
    int ret = activeTxn->commit(0);
    activeTxn = nullptr;
    return (ret == 0);
  }

  bool TxnAbort() {
    if (!pdb || !activeTxn)
      return false;
    int ret = activeTxn->abort();
    activeTxn = nullptr;
    return (ret == 0);
  }

  bool ReadVersion(int &nVersion) {
    nVersion = 0;
    return Read(std::string("version"), nVersion);
  }

  bool WriteVersion(int nVersion) {
    return Write(std::string("version"), nVersion);
  }

  bool static Rewrite(CWalletDBWrapper &dbw, const char *pszSkip = nullptr);
};

#endif
