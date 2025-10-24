// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <amount.h>
#include <consensus/params.h>
#include <policy/feerate.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <streams.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <utilstrencodings.h>
#include <validationinterface.h>
#include <wallet/crypter.h>
#include <wallet/rpcwallet.h>
#include <wallet/walletdb.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

typedef CWallet *CWalletRef;
extern std::vector<CWalletRef> vpwallets;

extern CFeeRate payTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fWalletRbf;
extern bool fWalletUnlockWithoutTransactions;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;

static const CAmount DEFAULT_TRANSACTION_FEE = 0;

static const CAmount DEFAULT_FALLBACK_FEE = 2000000 / COIN_SCALE / 10;

static const CAmount DEFAULT_DISCARD_FEE = 10000 / COIN_SCALE / 10;

static const CAmount DEFAULT_TRANSACTION_MINFEE = 100000 / COIN_SCALE / 10;

static const CAmount WALLET_INCREMENTAL_RELAY_FEE = 5000 / COIN_SCALE / 10;

static const CAmount MIN_CHANGE = CENT;

static const CAmount MIN_FINAL_CHANGE = MIN_CHANGE / 2;

static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;

static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;

static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 6;

static const bool DEFAULT_WALLET_RBF = false;
static const bool DEFAULT_WALLETBROADCAST = true;
static const bool DEFAULT_DISABLE_WALLET = false;

extern const char *DEFAULT_WALLET_DAT;

static const int64_t TIMESTAMP_MIN = 0;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CScheduler;
class CTxMemPool;
class CBlockPolicyEstimator;
class CWalletTx;
struct FeeCalculation;
enum class FeeEstimateMode;

enum WalletFeature {
  FEATURE_BASE = 10500,

  FEATURE_WALLETCRYPT = 40000,

  FEATURE_COMPRPUBKEY = 60000,

  FEATURE_HD = 130000,

  FEATURE_HD_SPLIT = 139900,

  FEATURE_NO_DEFAULT_KEY = 159900,

  FEATURE_LATEST = FEATURE_COMPRPUBKEY

};

enum OutputType : int {
  OUTPUT_TYPE_NONE,
  OUTPUT_TYPE_LEGACY,
  OUTPUT_TYPE_P2SH_SEGWIT,
  OUTPUT_TYPE_BECH32,

  OUTPUT_TYPE_DEFAULT = OUTPUT_TYPE_P2SH_SEGWIT
};

extern OutputType g_address_type;
extern OutputType g_change_type;

class CKeyPool {
public:
  int64_t nTime;
  CPubKey vchPubKey;
  bool fInternal;

  CKeyPool();
  CKeyPool(const CPubKey &vchPubKeyIn, bool internalIn);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(nVersion);
    READWRITE(nTime);
    READWRITE(vchPubKey);
    if (ser_action.ForRead()) {
      try {
        READWRITE(fInternal);
      } catch (std::ios_base::failure &) {
        fInternal = false;
      }
    } else {
      READWRITE(fInternal);
    }
  }
};

class CAddressBookData {
public:
  std::string name;
  std::string purpose;

  CAddressBookData() : purpose("unknown") {}

  typedef std::map<std::string, std::string> StringMap;
  StringMap destdata;
};

struct CRecipient {
  CScript scriptPubKey;
  CAmount nAmount;
  bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;

static inline void ReadOrderPos(int64_t &nOrderPos, mapValue_t &mapValue) {
  if (!mapValue.count("n")) {
    nOrderPos = -1;

    return;
  }
  nOrderPos = atoi64(mapValue["n"].c_str());
}

static inline void WriteOrderPos(const int64_t &nOrderPos,
                                 mapValue_t &mapValue) {
  if (nOrderPos == -1)
    return;
  mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry {
  CTxDestination destination;
  CAmount amount;
  int vout;
};

class CMerkleTx {
private:
  static const uint256 ABANDON_HASH;

public:
  CTransactionRef tx;
  uint256 hashBlock;

  int nIndex;

  CMerkleTx() {
    SetTx(MakeTransactionRef());
    Init();
  }

  explicit CMerkleTx(CTransactionRef arg) {
    SetTx(std::move(arg));
    Init();
  }

  void Init() {
    hashBlock = uint256();
    nIndex = -1;
  }

  void SetTx(CTransactionRef arg) { tx = std::move(arg); }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    std::vector<uint256> vMerkleBranch;

    READWRITE(tx);
    READWRITE(hashBlock);
    READWRITE(vMerkleBranch);
    READWRITE(nIndex);
  }

  void SetMerkleBranch(const CBlockIndex *pIndex, int posInBlock);

  int GetDepthInMainChain(const CBlockIndex *&pindexRet) const;
  int GetDepthInMainChain() const {
    const CBlockIndex *pindexRet;
    return GetDepthInMainChain(pindexRet);
  }
  bool IsInMainChain() const {
    const CBlockIndex *pindexRet;
    return GetDepthInMainChain(pindexRet) > 0;
  }
  int GetBlocksToMaturity() const;
  bool hashUnset() const {
    return (hashBlock.IsNull() || hashBlock == ABANDON_HASH);
  }
  bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
  void setAbandoned() { hashBlock = ABANDON_HASH; }

  const uint256 &GetHash() const { return tx->GetHash(); }
  bool IsCoinBase() const { return tx->IsCoinBase(); }
  bool IsHiveCoinBase() const { return tx->IsHiveCoinBase(); }

  bool IsBCT(const Consensus::Params &consensusParams, CScript scriptPubKeyBCF,
             CAmount *beeFeePaid = nullptr,
             CScript *scriptPubKeyHoney = nullptr) const {
    return tx->IsBCT(consensusParams, scriptPubKeyBCF, beeFeePaid,
                     scriptPubKeyHoney);
  }
};

class CWalletTx : public CMerkleTx {
private:
  const CWallet *pwallet;

public:
  mapValue_t mapValue;
  std::vector<std::pair<std::string, std::string>> vOrderForm;
  unsigned int fTimeReceivedIsTxTime;
  unsigned int nTimeReceived;

  unsigned int nTimeSmart;

  char fFromMe;
  std::string strFromAccount;
  int64_t nOrderPos;

  mutable bool fDebitCached;
  mutable bool fCreditCached;
  mutable bool fImmatureCreditCached;
  mutable bool fAvailableCreditCached;
  mutable bool fWatchDebitCached;
  mutable bool fWatchCreditCached;
  mutable bool fImmatureWatchCreditCached;
  mutable bool fAvailableWatchCreditCached;
  mutable bool fChangeCached;
  mutable bool fInMempool;
  mutable CAmount nDebitCached;
  mutable CAmount nCreditCached;
  mutable CAmount nImmatureCreditCached;
  mutable CAmount nAvailableCreditCached;
  mutable CAmount nWatchDebitCached;
  mutable CAmount nWatchCreditCached;
  mutable CAmount nImmatureWatchCreditCached;
  mutable CAmount nAvailableWatchCreditCached;
  mutable CAmount nChangeCached;

  CWalletTx() { Init(nullptr); }

  CWalletTx(const CWallet *pwalletIn, CTransactionRef arg)
      : CMerkleTx(std::move(arg)) {
    Init(pwalletIn);
  }

  void Init(const CWallet *pwalletIn) {
    pwallet = pwalletIn;
    mapValue.clear();
    vOrderForm.clear();
    fTimeReceivedIsTxTime = false;
    nTimeReceived = 0;
    nTimeSmart = 0;
    fFromMe = false;
    strFromAccount.clear();
    fDebitCached = false;
    fCreditCached = false;
    fImmatureCreditCached = false;
    fAvailableCreditCached = false;
    fWatchDebitCached = false;
    fWatchCreditCached = false;
    fImmatureWatchCreditCached = false;
    fAvailableWatchCreditCached = false;
    fChangeCached = false;
    fInMempool = false;
    nDebitCached = 0;
    nCreditCached = 0;
    nImmatureCreditCached = 0;
    nAvailableCreditCached = 0;
    nWatchDebitCached = 0;
    nWatchCreditCached = 0;
    nAvailableWatchCreditCached = 0;
    nImmatureWatchCreditCached = 0;
    nChangeCached = 0;
    nOrderPos = -1;
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    if (ser_action.ForRead())
      Init(nullptr);
    char fSpent = false;

    if (!ser_action.ForRead()) {
      mapValue["fromaccount"] = strFromAccount;

      WriteOrderPos(nOrderPos, mapValue);

      if (nTimeSmart)
        mapValue["timesmart"] = strprintf("%u", nTimeSmart);
    }

    READWRITE(*(CMerkleTx *)this);
    std::vector<CMerkleTx> vUnused;

    READWRITE(vUnused);
    READWRITE(mapValue);
    READWRITE(vOrderForm);
    READWRITE(fTimeReceivedIsTxTime);
    READWRITE(nTimeReceived);
    READWRITE(fFromMe);
    READWRITE(fSpent);

    if (ser_action.ForRead()) {
      strFromAccount = mapValue["fromaccount"];

      ReadOrderPos(nOrderPos, mapValue);

      nTimeSmart = mapValue.count("timesmart")
                       ? (unsigned int)atoi64(mapValue["timesmart"])
                       : 0;
    }

    mapValue.erase("fromaccount");
    mapValue.erase("spent");
    mapValue.erase("n");
    mapValue.erase("timesmart");
  }

  void MarkDirty() {
    fCreditCached = false;
    fAvailableCreditCached = false;
    fImmatureCreditCached = false;
    fWatchDebitCached = false;
    fWatchCreditCached = false;
    fAvailableWatchCreditCached = false;
    fImmatureWatchCreditCached = false;
    fDebitCached = false;
    fChangeCached = false;
  }

  void BindWallet(CWallet *pwalletIn) {
    pwallet = pwalletIn;
    MarkDirty();
  }

  CAmount GetDebit(const isminefilter &filter) const;
  CAmount GetCredit(const isminefilter &filter) const;
  CAmount GetImmatureCredit(bool fUseCache = true) const;
  CAmount GetAvailableCredit(bool fUseCache = true) const;
  CAmount GetImmatureWatchOnlyCredit(const bool fUseCache = true) const;
  CAmount GetAvailableWatchOnlyCredit(const bool fUseCache = true) const;
  CAmount GetChange() const;

  void GetAmounts(std::list<COutputEntry> &listReceived,
                  std::list<COutputEntry> &listSent, CAmount &nFee,
                  std::string &strSentAccount,
                  const isminefilter &filter) const;

  bool IsFromMe(const isminefilter &filter) const {
    return (GetDebit(filter) > 0);
  }

  bool IsEquivalentTo(const CWalletTx &tx) const;

  bool InMempool() const;
  bool IsTrusted() const;

  int64_t GetTxTime() const;
  int GetRequestCount() const;

  bool RelayWalletTransaction(CConnman *connman);

  bool AcceptToMemoryPool(const CAmount &nAbsurdFee, CValidationState &state);

  std::set<uint256> GetConflicts() const;
};

class CInputCoin {
public:
  CInputCoin(const CWalletTx *walletTx, unsigned int i) {
    if (!walletTx)
      throw std::invalid_argument("walletTx should not be null");
    if (i >= walletTx->tx->vout.size())
      throw std::out_of_range("The output index is out of range");

    outpoint = COutPoint(walletTx->GetHash(), i);
    txout = walletTx->tx->vout[i];
  }

  COutPoint outpoint;
  CTxOut txout;

  bool operator<(const CInputCoin &rhs) const {
    return outpoint < rhs.outpoint;
  }

  bool operator!=(const CInputCoin &rhs) const {
    return outpoint != rhs.outpoint;
  }

  bool operator==(const CInputCoin &rhs) const {
    return outpoint == rhs.outpoint;
  }
};

class COutput {
public:
  const CWalletTx *tx;
  int i;
  int nDepth;

  bool fSpendable;

  bool fSolvable;

  bool fSafe;

  COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn,
          bool fSolvableIn, bool fSafeIn) {
    tx = txIn;
    i = iIn;
    nDepth = nDepthIn;
    fSpendable = fSpendableIn;
    fSolvable = fSolvableIn;
    fSafe = fSafeIn;
  }

  std::string ToString() const;
};

class CWalletKey {
public:
  CPrivKey vchPrivKey;
  int64_t nTimeCreated;
  int64_t nTimeExpires;
  std::string strComment;

  explicit CWalletKey(int64_t nExpires = 0);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(nVersion);
    READWRITE(vchPrivKey);
    READWRITE(nTimeCreated);
    READWRITE(nTimeExpires);
    READWRITE(LIMITED_STRING(strComment, 65536));
  }
};

class CAccountingEntry {
public:
  std::string strAccount;
  CAmount nCreditDebit;
  int64_t nTime;
  std::string strOtherAccount;
  std::string strComment;
  mapValue_t mapValue;
  int64_t nOrderPos;

  uint64_t nEntryNo;

  CAccountingEntry() { SetNull(); }

  void SetNull() {
    nCreditDebit = 0;
    nTime = 0;
    strAccount.clear();
    strOtherAccount.clear();
    strComment.clear();
    nOrderPos = -1;
    nEntryNo = 0;
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(nVersion);

    READWRITE(nCreditDebit);
    READWRITE(nTime);
    READWRITE(LIMITED_STRING(strOtherAccount, 65536));

    if (!ser_action.ForRead()) {
      WriteOrderPos(nOrderPos, mapValue);

      if (!(mapValue.empty() && _ssExtra.empty())) {
        CDataStream ss(s.GetType(), s.GetVersion());
        ss.insert(ss.begin(), '\0');
        ss << mapValue;
        ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
        strComment.append(ss.str());
      }
    }

    READWRITE(LIMITED_STRING(strComment, 65536));

    size_t nSepPos = strComment.find("\0", 0, 1);
    if (ser_action.ForRead()) {
      mapValue.clear();
      if (std::string::npos != nSepPos) {
        CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1,
                                         strComment.end()),
                       s.GetType(), s.GetVersion());
        ss >> mapValue;
        _ssExtra = std::vector<char>(ss.begin(), ss.end());
      }
      ReadOrderPos(nOrderPos, mapValue);
    }
    if (std::string::npos != nSepPos)
      strComment.erase(nSepPos);

    mapValue.erase("n");
  }

private:
  std::vector<char> _ssExtra;
};

struct CBeeCreationTransactionInfo {
  std::string txid;
  int64_t time;
  int beeCount;
  CAmount beeFeePaid;
  bool communityContrib;
  std::string beeStatus;
  std::string honeyAddress;
  CAmount rewardsPaid;
  CAmount profit;
  int blocksFound;
  int blocksLeft;
};

struct CBeeRange {
  std::string txid;
  std::string honeyAddress;
  bool communityContrib;
  int offset;
  int count;
};

class WalletRescanReserver;

class CWallet final : public CCryptoKeyStore, public CValidationInterface {
private:
  static std::atomic<bool> fFlushScheduled;
  std::atomic<bool> fAbortRescan;
  std::atomic<bool> fScanningWallet;

  std::mutex mutexScanning;
  friend class WalletRescanReserver;

  bool SelectCoins(const std::vector<COutput> &vAvailableCoins,
                   const CAmount &nTargetValue,
                   std::set<CInputCoin> &setCoinsRet, CAmount &nValueRet,
                   const CCoinControl *coinControl = nullptr) const;

  CWalletDB *pwalletdbEncryption;

  int nWalletVersion;

  int nWalletMaxVersion;

  int64_t nNextResend;
  int64_t nLastResend;
  bool fBroadcastTransactions;

  typedef std::multimap<COutPoint, uint256> TxSpends;
  TxSpends mapTxSpends;
  void AddToSpends(const COutPoint &outpoint, const uint256 &wtxid);
  void AddToSpends(const uint256 &wtxid);

  void MarkConflicted(const uint256 &hashBlock, const uint256 &hashTx);

  void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

  void SyncTransaction(const CTransactionRef &tx,
                       const CBlockIndex *pindex = nullptr, int posInBlock = 0);

  CHDChain hdChain;

  void DeriveNewChildKey(CWalletDB &walletdb, CKeyMetadata &metadata,
                         CKey &secret, bool internal = false);

  std::set<int64_t> setInternalKeyPool;
  std::set<int64_t> setExternalKeyPool;
  int64_t m_max_keypool_index;
  std::map<CKeyID, int64_t> m_pool_key_to_index;

  int64_t nTimeFirstKey;

  bool AddWatchOnly(const CScript &dest) override;

  std::unique_ptr<CWalletDBWrapper> dbw;

  const CBlockIndex *m_last_block_processed;

public:
  mutable CCriticalSection cs_wallet;

  CWalletDBWrapper &GetDBHandle() { return *dbw; }

  std::string GetName() const {
    if (dbw) {
      return dbw->GetName();
    } else {
      return "dummy";
    }
  }

  void LoadKeyPool(int64_t nIndex, const CKeyPool &keypool);

  std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

  std::map<CScriptID, CKeyMetadata> m_script_metadata;

  typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
  MasterKeyMap mapMasterKeys;
  unsigned int nMasterKeyMaxID;

  CWallet() : dbw(new CWalletDBWrapper()) { SetNull(); }

  explicit CWallet(std::unique_ptr<CWalletDBWrapper> dbw_in)
      : dbw(std::move(dbw_in)) {
    SetNull();
  }

  ~CWallet() {
    delete pwalletdbEncryption;
    pwalletdbEncryption = nullptr;
  }

  void SetNull() {
    nWalletVersion = FEATURE_BASE;
    nWalletMaxVersion = FEATURE_BASE;
    nMasterKeyMaxID = 0;
    pwalletdbEncryption = nullptr;
    nOrderPosNext = 0;
    nAccountingEntryNumber = 0;
    nNextResend = 0;
    nLastResend = 0;
    m_max_keypool_index = 0;
    nTimeFirstKey = 0;
    fBroadcastTransactions = false;
    nRelockTime = 0;
    fAbortRescan = false;
    fScanningWallet = false;
  }

  std::map<uint256, CWalletTx> mapWallet;
  std::list<CAccountingEntry> laccentries;

  typedef std::pair<CWalletTx *, CAccountingEntry *> TxPair;
  typedef std::multimap<int64_t, TxPair> TxItems;
  TxItems wtxOrdered;

  int64_t nOrderPosNext;
  uint64_t nAccountingEntryNumber;
  std::map<uint256, int> mapRequestCount;

  std::map<CTxDestination, CAddressBookData> mapAddressBook;

  std::set<COutPoint> setLockedCoins;

  const CWalletTx *GetWalletTx(const uint256 &hash) const;

  bool CanSupportFeature(enum WalletFeature wf) const {
    AssertLockHeld(cs_wallet);
    return nWalletMaxVersion >= wf;
  }

  void AvailableCoins(std::vector<COutput> &vCoins, bool fOnlySafe = true,
                      const CCoinControl *coinControl = nullptr,
                      const CAmount &nMinimumAmount = 1,
                      const CAmount &nMaximumAmount = MAX_MONEY,
                      const CAmount &nMinimumSumAmount = MAX_MONEY,
                      const uint64_t nMaximumCount = 0, const int nMinDepth = 0,
                      const int nMaxDepth = 9999999) const;

  std::map<CTxDestination, std::vector<COutput>> ListCoins() const;

  const CTxOut &FindNonChangeParentOutput(const CTransaction &tx,
                                          int output) const;

  bool SelectCoinsMinConf(const CAmount &nTargetValue, int nConfMine,
                          int nConfTheirs, uint64_t nMaxAncestors,
                          std::vector<COutput> vCoins,
                          std::set<CInputCoin> &setCoinsRet,
                          CAmount &nValueRet) const;

  bool IsSpent(const uint256 &hash, unsigned int n) const;

  bool IsLockedCoin(uint256 hash, unsigned int n) const;
  void LockCoin(const COutPoint &output);
  void UnlockCoin(const COutPoint &output);
  void UnlockAllCoins();
  void ListLockedCoins(std::vector<COutPoint> &vOutpts) const;

  void AbortRescan() { fAbortRescan = true; }
  bool IsAbortingRescan() { return fAbortRescan; }
  bool IsScanning() { return fScanningWallet; }

  CPubKey GenerateNewKey(CWalletDB &walletdb, bool internal = false);

  bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) override;
  bool AddKeyPubKeyWithDB(CWalletDB &walletdb, const CKey &key,
                          const CPubKey &pubkey);

  bool LoadKey(const CKey &key, const CPubKey &pubkey) {
    return CCryptoKeyStore::AddKeyPubKey(key, pubkey);
  }

  bool LoadKeyMetadata(const CKeyID &keyID, const CKeyMetadata &metadata);
  bool LoadScriptMetadata(const CScriptID &script_id,
                          const CKeyMetadata &metadata);

  bool LoadMinVersion(int nVersion) {
    AssertLockHeld(cs_wallet);
    nWalletVersion = nVersion;
    nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion);
    return true;
  }
  void UpdateTimeFirstKey(int64_t nCreateTime);

  bool
  AddCryptedKey(const CPubKey &vchPubKey,
                const std::vector<unsigned char> &vchCryptedSecret) override;

  bool LoadCryptedKey(const CPubKey &vchPubKey,
                      const std::vector<unsigned char> &vchCryptedSecret);
  bool AddCScript(const CScript &redeemScript) override;
  bool LoadCScript(const CScript &redeemScript);

  bool AddDestData(const CTxDestination &dest, const std::string &key,
                   const std::string &value);

  bool EraseDestData(const CTxDestination &dest, const std::string &key);

  bool LoadDestData(const CTxDestination &dest, const std::string &key,
                    const std::string &value);

  bool GetDestData(const CTxDestination &dest, const std::string &key,
                   std::string *value) const;

  std::vector<std::string> GetDestValues(const std::string &prefix) const;

  bool AddWatchOnly(const CScript &dest, int64_t nCreateTime);
  bool RemoveWatchOnly(const CScript &dest) override;

  bool LoadWatchOnly(const CScript &dest);

  int64_t nRelockTime;

  bool Unlock(const SecureString &strWalletPassphrase);
  bool ChangeWalletPassphrase(const SecureString &strOldWalletPassphrase,
                              const SecureString &strNewWalletPassphrase);
  bool EncryptWallet(const SecureString &strWalletPassphrase);

  void GetKeyBirthTimes(std::map<CTxDestination, int64_t> &mapKeyBirth) const;
  unsigned int ComputeTimeSmart(const CWalletTx &wtx) const;

  int64_t IncOrderPosNext(CWalletDB *pwalletdb = nullptr);
  DBErrors ReorderTransactions();
  bool AccountMove(std::string strFrom, std::string strTo, CAmount nAmount,
                   std::string strComment = "");
  bool GetAccountDestination(CTxDestination &dest, std::string strAccount,
                             bool bForceNew = false);

  void MarkDirty();
  bool AddToWallet(const CWalletTx &wtxIn, bool fFlushOnClose = true);
  bool LoadToWallet(const CWalletTx &wtxIn);
  void TransactionAddedToMempool(const CTransactionRef &tx) override;
  void
  BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                 const CBlockIndex *pindex,
                 const std::vector<CTransactionRef> &vtxConflicted) override;
  void BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) override;
  bool AddToWalletIfInvolvingMe(const CTransactionRef &tx,
                                const CBlockIndex *pIndex, int posInBlock,
                                bool fUpdate);
  int64_t RescanFromTime(int64_t startTime,
                         const WalletRescanReserver &reserver, bool update);
  CBlockIndex *ScanForWalletTransactions(CBlockIndex *pindexStart,
                                         CBlockIndex *pindexStop,
                                         const WalletRescanReserver &reserver,
                                         bool fUpdate = false);
  void TransactionRemovedFromMempool(const CTransactionRef &ptx) override;
  void ReacceptWalletTransactions();
  void ResendWalletTransactions(int64_t nBestBlockTime,
                                CConnman *connman) override;

  std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime,
                                                      CConnman *connman);
  CAmount GetBalance() const;
  CAmount GetUnconfirmedBalance() const;
  CAmount GetImmatureBalance() const;
  CAmount GetWatchOnlyBalance() const;
  CAmount GetUnconfirmedWatchOnlyBalance() const;
  CAmount GetImmatureWatchOnlyBalance() const;
  CAmount GetLegacyBalance(const isminefilter &filter, int minDepth,
                           const std::string *account) const;
  CAmount GetAvailableBalance(const CCoinControl *coinControl = nullptr) const;

  OutputType TransactionChangeType(OutputType change_type,
                                   const std::vector<CRecipient> &vecSend);

  bool CreateBeeTransaction(int beeCount, CWalletTx &wtxNew,
                            CReserveKey &reservekeyChange,
                            CReserveKey &reservekeyHoney,
                            std::string honeyAddress, std::string changeAddress,
                            bool communityContrib, std::string &strFailReason,
                            const Consensus::Params &consensusParams);

  CBeeCreationTransactionInfo GetBCT(const CWalletTx &wtx, bool includeDead,
                                     bool scanRewards,
                                     const Consensus::Params &consensusParams,
                                     int minHoneyConfirmations);

  std::vector<CBeeCreationTransactionInfo>
  GetBCTs(bool includeDead, bool scanRewards,
          const Consensus::Params &consensusParams,
          int minHoneyConfirmations = 1);

  bool CreateNickRegistrationTransaction(
      std::string nickname, CWalletTx &wtxNew, CReserveKey &reservekeyChange,
      CReserveKey &reservekeyNickAddress, std::string nickAddress,
      std::string changeAddress, std::string &strFailReason,
      const Consensus::Params &consensusParams);

  bool FundTransaction(CMutableTransaction &tx, CAmount &nFeeRet,
                       int &nChangePosInOut, std::string &strFailReason,
                       bool lockUnspents,
                       const std::set<int> &setSubtractFeeFromOutputs,
                       CCoinControl);
  bool SignTransaction(CMutableTransaction &tx);

  bool CreateTransaction(const std::vector<CRecipient> &vecSend,
                         CWalletTx &wtxNew, CReserveKey &reservekey,
                         CAmount &nFeeRet, int &nChangePosInOut,
                         std::string &strFailReason,
                         const CCoinControl &coin_control, bool sign = true);
  bool CommitTransaction(CWalletTx &wtxNew, CReserveKey &reservekey,
                         CConnman *connman, CValidationState &state);

  void ListAccountCreditDebit(const std::string &strAccount,
                              std::list<CAccountingEntry> &entries);
  bool AddAccountingEntry(const CAccountingEntry &);
  bool AddAccountingEntry(const CAccountingEntry &, CWalletDB *pwalletdb);
  template <typename ContainerType>
  bool DummySignTx(CMutableTransaction &txNew,
                   const ContainerType &coins) const;

  static CFeeRate minTxFee;
  static CFeeRate fallbackFee;
  static CFeeRate m_discard_rate;

  bool NewKeyPool();
  size_t KeypoolCountExternalKeys();
  bool TopUpKeyPool(unsigned int kpSize = 0);
  void ReserveKeyFromKeyPool(int64_t &nIndex, CKeyPool &keypool,
                             bool fRequestedInternal);
  void KeepKey(int64_t nIndex);
  void ReturnKey(int64_t nIndex, bool fInternal, const CPubKey &pubkey);
  bool GetKeyFromPool(CPubKey &key, bool internal = false);
  int64_t GetOldestKeyPoolTime();

  void MarkReserveKeysAsUsed(int64_t keypool_id);
  const std::map<CKeyID, int64_t> &GetAllReserveKeys() const {
    return m_pool_key_to_index;
  }

  std::set<std::set<CTxDestination>> GetAddressGroupings();
  std::map<CTxDestination, CAmount> GetAddressBalances();

  std::set<CTxDestination>
  GetAccountAddresses(const std::string &strAccount) const;

  isminetype IsMine(const CTxIn &txin) const;

  CAmount GetDebit(const CTxIn &txin, const isminefilter &filter) const;
  isminetype IsMine(const CTxOut &txout) const;
  CAmount GetCredit(const CTxOut &txout, const isminefilter &filter) const;
  bool IsChange(const CTxOut &txout) const;
  CAmount GetChange(const CTxOut &txout) const;
  bool IsMine(const CTransaction &tx) const;

  bool IsFromMe(const CTransaction &tx) const;
  CAmount GetDebit(const CTransaction &tx, const isminefilter &filter) const;

  bool IsAllFromMe(const CTransaction &tx, const isminefilter &filter) const;
  CAmount GetCredit(const CTransaction &tx, const isminefilter &filter) const;
  CAmount GetChange(const CTransaction &tx) const;
  void SetBestChain(const CBlockLocator &loc) override;

  DBErrors LoadWallet(bool &fFirstRunRet);
  DBErrors ZapWalletTx(std::vector<CWalletTx> &vWtx);
  DBErrors ZapSelectTx(std::vector<uint256> &vHashIn,
                       std::vector<uint256> &vHashOut);

  bool SetAddressBook(const CTxDestination &address, const std::string &strName,
                      const std::string &purpose);

  bool DelAddressBook(const CTxDestination &address);

  const std::string &GetAccountName(const CScript &scriptPubKey) const;

  void Inventory(const uint256 &hash) override {
    {
      LOCK(cs_wallet);
      std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
      if (mi != mapRequestCount.end())
        (*mi).second++;
    }
  }

  void GetScriptForMining(std::shared_ptr<CReserveScript> &script);

  unsigned int GetKeyPoolSize() {
    AssertLockHeld(cs_wallet);

    return setInternalKeyPool.size() + setExternalKeyPool.size();
  }

  bool SetMinVersion(enum WalletFeature, CWalletDB *pwalletdbIn = nullptr,
                     bool fExplicit = false);

  bool SetMaxVersion(int nVersion);

  int GetVersion() {
    LOCK(cs_wallet);
    return nWalletVersion;
  }

  std::set<uint256> GetConflicts(const uint256 &txid) const;

  bool HasWalletSpend(const uint256 &txid) const;

  void Flush(bool shutdown = false);

  boost::signals2::signal<void(CWallet *wallet, const CTxDestination &address,
                               const std::string &label, bool isMine,
                               const std::string &purpose, ChangeType status)>
      NotifyAddressBookChanged;

  boost::signals2::signal<void(CWallet *wallet, const uint256 &hashTx,
                               ChangeType status)>
      NotifyTransactionChanged;

  boost::signals2::signal<void(const std::string &title, int nProgress)>
      ShowProgress;

  boost::signals2::signal<void(bool fHaveWatchOnly)> NotifyWatchonlyChanged;

  bool GetBroadcastTransactions() const { return fBroadcastTransactions; }

  void SetBroadcastTransactions(bool broadcast) {
    fBroadcastTransactions = broadcast;
  }

  bool TransactionCanBeAbandoned(const uint256 &hashTx) const;

  bool AbandonTransaction(const uint256 &hashTx);

  bool MarkReplaced(const uint256 &originalHash, const uint256 &newHash);

  static CWallet *CreateWalletFromFile(const std::string walletFile);

  void postInitProcess(CScheduler &scheduler);

  bool BackupWallet(const std::string &strDest);

  bool SetHDChain(const CHDChain &chain, bool memonly);
  const CHDChain &GetHDChain() const { return hdChain; }

  bool IsHDEnabled() const;

  CPubKey GenerateNewHDMasterKey();

  bool SetHDMasterKey(const CPubKey &key);

  void BlockUntilSyncedToCurrentChain();

  void LearnRelatedScripts(const CPubKey &key, OutputType);

  void LearnAllRelatedScripts(const CPubKey &key);

  CTxDestination AddAndGetDestinationForScript(const CScript &script,
                                               OutputType);
};

class CReserveKey final : public CReserveScript {
protected:
  CWallet *pwallet;
  int64_t nIndex;
  CPubKey vchPubKey;
  bool fInternal;

public:
  explicit CReserveKey(CWallet *pwalletIn) {
    nIndex = -1;
    pwallet = pwalletIn;
    fInternal = false;
  }

  CReserveKey() = default;
  CReserveKey(const CReserveKey &) = delete;
  CReserveKey &operator=(const CReserveKey &) = delete;

  ~CReserveKey() { ReturnKey(); }

  void ReturnKey();
  bool GetReservedKey(CPubKey &pubkey, bool internal = false);
  void KeepKey();
  void KeepScript() override { KeepKey(); }
};

class CAccount {
public:
  CPubKey vchPubKey;

  CAccount() { SetNull(); }

  void SetNull() { vchPubKey = CPubKey(); }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(nVersion);
    READWRITE(vchPubKey);
  }
};

template <typename ContainerType>
bool CWallet::DummySignTx(CMutableTransaction &txNew,
                          const ContainerType &coins) const {
  int nIn = 0;
  for (const auto &coin : coins) {
    const CScript &scriptPubKey = coin.txout.scriptPubKey;
    SignatureData sigdata;

    if (!ProduceSignature(DummySignatureCreator(this), scriptPubKey, sigdata)) {
      return false;
    } else {
      UpdateTransaction(txNew, nIn, sigdata);
    }

    nIn++;
  }
  return true;
}

OutputType ParseOutputType(const std::string &str,
                           OutputType default_type = OUTPUT_TYPE_DEFAULT);
const std::string &FormatOutputType(OutputType type);

CTxDestination GetDestinationForKey(const CPubKey &key, OutputType);

std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey &key);

class WalletRescanReserver {
private:
  CWalletRef m_wallet;
  bool m_could_reserve;

public:
  explicit WalletRescanReserver(CWalletRef w)
      : m_wallet(w), m_could_reserve(false) {}

  bool reserve() {
    assert(!m_could_reserve);
    std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
    if (m_wallet->fScanningWallet) {
      return false;
    }
    m_wallet->fScanningWallet = true;
    m_could_reserve = true;
    return true;
  }

  bool isReserved() const {
    return (m_could_reserve && m_wallet->fScanningWallet);
  }

  ~WalletRescanReserver() {
    std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
    if (m_could_reserve) {
      m_wallet->fScanningWallet = false;
    }
  }
};

#endif
