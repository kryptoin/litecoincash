// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETDB_H
#define BITCOIN_WALLET_WALLETDB_H

#include <amount.h>
#include <key.h>
#include <primitives/transaction.h>
#include <wallet/db.h>

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

static const bool DEFAULT_FLUSHWALLET = true;

class CAccount;
class CAccountingEntry;
struct CBlockLocator;
class CKeyPool;
class CMasterKey;
class CScript;
class CWallet;
class CWalletTx;
class uint160;
class uint256;

enum DBErrors {
  DB_LOAD_OK,
  DB_CORRUPT,
  DB_NONCRITICAL_ERROR,
  DB_TOO_NEW,
  DB_LOAD_FAIL,
  DB_NEED_REWRITE
};

class CHDChain {
public:
  uint32_t nExternalChainCounter;
  uint32_t nInternalChainCounter;
  CKeyID masterKeyID;

  static const int VERSION_HD_BASE = 1;
  static const int VERSION_HD_CHAIN_SPLIT = 2;
  static const int CURRENT_VERSION = VERSION_HD_CHAIN_SPLIT;
  int nVersion;

  CHDChain() { SetNull(); }
  ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(this->nVersion);
    READWRITE(nExternalChainCounter);
    READWRITE(masterKeyID);
    if (this->nVersion >= VERSION_HD_CHAIN_SPLIT)
      READWRITE(nInternalChainCounter);
  }

  void SetNull() {
    nVersion = CHDChain::CURRENT_VERSION;
    nExternalChainCounter = 0;
    nInternalChainCounter = 0;
    masterKeyID.SetNull();
  }
};

class CKeyMetadata {
public:
  static const int VERSION_BASIC = 1;
  static const int VERSION_WITH_HDDATA = 10;
  static const int CURRENT_VERSION = VERSION_WITH_HDDATA;
  int nVersion;
  int64_t nCreateTime;

  std::string hdKeypath;

  CKeyID hdMasterKeyID;

  CKeyMetadata() { SetNull(); }
  explicit CKeyMetadata(int64_t nCreateTime_) {
    SetNull();
    nCreateTime = nCreateTime_;
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(this->nVersion);
    READWRITE(nCreateTime);
    if (this->nVersion >= VERSION_WITH_HDDATA) {
      READWRITE(hdKeypath);
      READWRITE(hdMasterKeyID);
    }
  }

  void SetNull() {
    nVersion = CKeyMetadata::CURRENT_VERSION;
    nCreateTime = 0;
    hdKeypath.clear();
    hdMasterKeyID.SetNull();
  }
};

class CWalletDB {
private:
  template <typename K, typename T>
  bool WriteIC(const K &key, const T &value, bool fOverwrite = true) {
    if (!batch.Write(key, value, fOverwrite)) {
      return false;
    }
    m_dbw.IncrementUpdateCounter();
    return true;
  }

  template <typename K> bool EraseIC(const K &key) {
    if (!batch.Erase(key)) {
      return false;
    }
    m_dbw.IncrementUpdateCounter();
    return true;
  }

public:
  explicit CWalletDB(CWalletDBWrapper &dbw, const char *pszMode = "r+",
                     bool _fFlushOnClose = true)
      : batch(dbw, pszMode, _fFlushOnClose), m_dbw(dbw) {}
  CWalletDB(const CWalletDB &) = delete;
  CWalletDB &operator=(const CWalletDB &) = delete;

  bool WriteName(const std::string &strAddress, const std::string &strName);
  bool EraseName(const std::string &strAddress);

  bool WritePurpose(const std::string &strAddress, const std::string &purpose);
  bool ErasePurpose(const std::string &strAddress);

  bool WriteTx(const CWalletTx &wtx);
  bool EraseTx(uint256 hash);

  bool WriteKey(const CPubKey &vchPubKey, const CPrivKey &vchPrivKey,
                const CKeyMetadata &keyMeta);
  bool WriteCryptedKey(const CPubKey &vchPubKey,
                       const std::vector<unsigned char> &vchCryptedSecret,
                       const CKeyMetadata &keyMeta);
  bool WriteMasterKey(unsigned int nID, const CMasterKey &kMasterKey);

  bool WriteCScript(const uint160 &hash, const CScript &redeemScript);

  bool WriteWatchOnly(const CScript &script, const CKeyMetadata &keymeta);
  bool EraseWatchOnly(const CScript &script);

  bool WriteBestBlock(const CBlockLocator &locator);
  bool ReadBestBlock(CBlockLocator &locator);

  bool WriteOrderPosNext(int64_t nOrderPosNext);

  bool ReadPool(int64_t nPool, CKeyPool &keypool);
  bool WritePool(int64_t nPool, const CKeyPool &keypool);
  bool ErasePool(int64_t nPool);

  bool WriteMinVersion(int nVersion);

  bool WriteAccountingEntry(const uint64_t nAccEntryNum,
                            const CAccountingEntry &acentry);
  bool ReadAccount(const std::string &strAccount, CAccount &account);
  bool WriteAccount(const std::string &strAccount, const CAccount &account);

  bool WriteDestData(const std::string &address, const std::string &key,
                     const std::string &value);

  bool EraseDestData(const std::string &address, const std::string &key);

  CAmount GetAccountCreditDebit(const std::string &strAccount);
  void ListAccountCreditDebit(const std::string &strAccount,
                              std::list<CAccountingEntry> &acentries);

  DBErrors LoadWallet(CWallet *pwallet);
  DBErrors FindWalletTx(std::vector<uint256> &vTxHash,
                        std::vector<CWalletTx> &vWtx);
  DBErrors ZapWalletTx(std::vector<CWalletTx> &vWtx);
  DBErrors ZapSelectTx(std::vector<uint256> &vHashIn,
                       std::vector<uint256> &vHashOut);

  static bool Recover(const std::string &filename, void *callbackDataIn,
                      bool (*recoverKVcallback)(void *callbackData,
                                                CDataStream ssKey,
                                                CDataStream ssValue),
                      std::string &out_backup_filename);

  static bool Recover(const std::string &filename,
                      std::string &out_backup_filename);

  static bool RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey,
                                    CDataStream ssValue);

  static bool IsKeyType(const std::string &strType);

  static bool VerifyEnvironment(const std::string &walletFile,
                                const fs::path &walletDir,
                                std::string &errorStr);

  static bool VerifyDatabaseFile(const std::string &walletFile,
                                 const fs::path &walletDir,
                                 std::string &warningStr,
                                 std::string &errorStr);

  bool WriteHDChain(const CHDChain &chain);

  bool TxnBegin();

  bool TxnCommit();

  bool TxnAbort();

  bool ReadVersion(int &nVersion);

  bool WriteVersion(int nVersion);

private:
  CDB batch;
  CWalletDBWrapper &m_dbw;
};

void MaybeCompactWalletDB();

#endif
