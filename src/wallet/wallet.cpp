// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <base58.h>
#include <chain.h>
#include <checkpoints.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <fs.h>
#include <key.h>
#include <keystore.h>
#include <net.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rialto.h>
#include <scheduler.h>
#include <script/script.h>
#include <timedata.h>
#include <txmempool.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/init.h>

#include <assert.h>
#include <future>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

#include <script/ismine.h>

std::vector<CWalletRef> vpwallets;

CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
unsigned int nTxConfirmTarget = DEFAULT_TX_CONFIRM_TARGET;
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;
bool fWalletRbf = DEFAULT_WALLET_RBF;
OutputType g_address_type = OUTPUT_TYPE_NONE;
OutputType g_change_type = OUTPUT_TYPE_NONE;

const char *DEFAULT_WALLET_DAT = "wallet.dat";
const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

CFeeRate CWallet::minTxFee = CFeeRate(DEFAULT_TRANSACTION_MINFEE);

CFeeRate CWallet::fallbackFee = CFeeRate(DEFAULT_FALLBACK_FEE);

CFeeRate CWallet::m_discard_rate = CFeeRate(DEFAULT_DISCARD_FEE);

const uint256 CMerkleTx::ABANDON_HASH(uint256S(
    "0000000000000000000000000000000000000000000000000000000000000001"));

struct CompareValueOnly {
  bool operator()(const CInputCoin &t1, const CInputCoin &t2) const {
    return t1.txout.nValue < t2.txout.nValue;
  }
};

std::string COutput::ToString() const {
  return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i,
                   nDepth, FormatMoney(tx->tx->vout[i].nValue));
}

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
  const CKeyStore &keystore;
  std::vector<CKeyID> &vKeys;

public:
  CAffectedKeysVisitor(const CKeyStore &keystoreIn,
                       std::vector<CKeyID> &vKeysIn)
      : keystore(keystoreIn), vKeys(vKeysIn) {}

  void Process(const CScript &script) {
    txnouttype type;
    std::vector<CTxDestination> vDest;
    int nRequired;
    if (ExtractDestinations(script, type, vDest, nRequired)) {
      for (const CTxDestination &dest : vDest)
        boost::apply_visitor(*this, dest);
    }
  }

  void operator()(const CKeyID &keyId) {
    if (keystore.HaveKey(keyId))
      vKeys.push_back(keyId);
  }

  void operator()(const CScriptID &scriptId) {
    CScript script;
    if (keystore.GetCScript(scriptId, script))
      Process(script);
  }

  void operator()(const WitnessV0ScriptHash &scriptID) {
    CScriptID id;
    CRIPEMD160().Write(scriptID.begin(), 32).Finalize(id.begin());
    CScript script;
    if (keystore.GetCScript(id, script)) {
      Process(script);
    }
  }

  void operator()(const WitnessV0KeyHash &keyid) {
    CKeyID id(keyid);
    if (keystore.HaveKey(id)) {
      vKeys.push_back(id);
    }
  }

  template <typename X> void operator()(const X &none) {}
};

const CWalletTx *CWallet::GetWalletTx(const uint256 &hash) const {
  LOCK(cs_wallet);
  std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
  if (it == mapWallet.end())
    return nullptr;
  return &(it->second);
}

CPubKey CWallet::GenerateNewKey(CWalletDB &walletdb, bool internal) {
  AssertLockHeld(cs_wallet);

  bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY);

  CKey secret;

  int64_t nCreationTime = GetTime();
  CKeyMetadata metadata(nCreationTime);

  if (IsHDEnabled()) {
    DeriveNewChildKey(walletdb, metadata, secret,
                      (CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
  } else {
    secret.MakeNewKey(fCompressed);
  }

  if (fCompressed) {
    SetMinVersion(FEATURE_COMPRPUBKEY);
  }

  CPubKey pubkey = secret.GetPubKey();
  assert(secret.VerifyPubKey(pubkey));

  mapKeyMetadata[pubkey.GetID()] = metadata;
  UpdateTimeFirstKey(nCreationTime);

  if (!AddKeyPubKeyWithDB(walletdb, secret, pubkey)) {
    throw std::runtime_error(std::string(__func__) + ": AddKey failed");
  }
  return pubkey;
}

void CWallet::DeriveNewChildKey(CWalletDB &walletdb, CKeyMetadata &metadata,
                                CKey &secret, bool internal) {
  CKey key;

  CExtKey masterKey;

  CExtKey accountKey;

  CExtKey chainChildKey;

  CExtKey childKey;

  if (!GetKey(hdChain.masterKeyID, key))
    throw std::runtime_error(std::string(__func__) + ": Master key not found");

  masterKey.SetMaster(key.begin(), key.size());

  masterKey.Derive(accountKey, BIP32_HARDENED_KEY_LIMIT);

  assert(internal ? CanSupportFeature(FEATURE_HD_SPLIT) : true);
  accountKey.Derive(chainChildKey,
                    BIP32_HARDENED_KEY_LIMIT + (internal ? 1 : 0));

  do {
    if (internal) {
      chainChildKey.Derive(childKey, hdChain.nInternalChainCounter |
                                         BIP32_HARDENED_KEY_LIMIT);
      metadata.hdKeypath =
          "m/0'/1'/" + std::to_string(hdChain.nInternalChainCounter) + "'";
      hdChain.nInternalChainCounter++;
    } else {
      chainChildKey.Derive(childKey, hdChain.nExternalChainCounter |
                                         BIP32_HARDENED_KEY_LIMIT);
      metadata.hdKeypath =
          "m/0'/0'/" + std::to_string(hdChain.nExternalChainCounter) + "'";
      hdChain.nExternalChainCounter++;
    }
  } while (HaveKey(childKey.key.GetPubKey().GetID()));
  secret = childKey.key;
  metadata.hdMasterKeyID = hdChain.masterKeyID;

  if (!walletdb.WriteHDChain(hdChain))
    throw std::runtime_error(std::string(__func__) +
                             ": Writing HD chain model failed");
}

bool CWallet::AddKeyPubKeyWithDB(CWalletDB &walletdb, const CKey &secret,
                                 const CPubKey &pubkey) {
  AssertLockHeld(cs_wallet);

  bool needsDB = !pwalletdbEncryption;
  if (needsDB) {
    pwalletdbEncryption = &walletdb;
  }
  if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey)) {
    if (needsDB)
      pwalletdbEncryption = nullptr;
    return false;
  }
  if (needsDB)
    pwalletdbEncryption = nullptr;

  CScript script;
  script = GetScriptForDestination(pubkey.GetID());
  if (HaveWatchOnly(script)) {
    RemoveWatchOnly(script);
  }
  script = GetScriptForRawPubKey(pubkey);
  if (HaveWatchOnly(script)) {
    RemoveWatchOnly(script);
  }

  if (!IsCrypted()) {
    return walletdb.WriteKey(pubkey, secret.GetPrivKey(),
                             mapKeyMetadata[pubkey.GetID()]);
  }
  return true;
}

bool CWallet::AddKeyPubKey(const CKey &secret, const CPubKey &pubkey) {
  CWalletDB walletdb(*dbw);
  return CWallet::AddKeyPubKeyWithDB(walletdb, secret, pubkey);
}

bool CWallet::AddCryptedKey(
    const CPubKey &vchPubKey,
    const std::vector<unsigned char> &vchCryptedSecret) {
  if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
    return false;
  {
    LOCK(cs_wallet);
    if (pwalletdbEncryption)
      return pwalletdbEncryption->WriteCryptedKey(
          vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    else
      return CWalletDB(*dbw).WriteCryptedKey(vchPubKey, vchCryptedSecret,
                                             mapKeyMetadata[vchPubKey.GetID()]);
  }
}

bool CWallet::LoadKeyMetadata(const CKeyID &keyID, const CKeyMetadata &meta) {
  AssertLockHeld(cs_wallet);

  UpdateTimeFirstKey(meta.nCreateTime);
  mapKeyMetadata[keyID] = meta;
  return true;
}

bool CWallet::LoadScriptMetadata(const CScriptID &script_id,
                                 const CKeyMetadata &meta) {
  AssertLockHeld(cs_wallet);

  UpdateTimeFirstKey(meta.nCreateTime);
  m_script_metadata[script_id] = meta;
  return true;
}

bool CWallet::LoadCryptedKey(
    const CPubKey &vchPubKey,
    const std::vector<unsigned char> &vchCryptedSecret) {
  return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

void CWallet::UpdateTimeFirstKey(int64_t nCreateTime) {
  AssertLockHeld(cs_wallet);
  if (nCreateTime <= 1) {
    nTimeFirstKey = 1;
  } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
    nTimeFirstKey = nCreateTime;
  }
}

bool CWallet::AddCScript(const CScript &redeemScript) {
  if (!CCryptoKeyStore::AddCScript(redeemScript))
    return false;
  return CWalletDB(*dbw).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript &redeemScript) {
  if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
    std::string strAddr = EncodeDestination(CScriptID(redeemScript));
    LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i "
              "which exceeds maximum size %i thus can never be redeemed. Do "
              "not use address %s.\n",
              __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
    return true;
  }

  return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript &dest) {
  if (!CCryptoKeyStore::AddWatchOnly(dest))
    return false;
  const CKeyMetadata &meta = m_script_metadata[CScriptID(dest)];
  UpdateTimeFirstKey(meta.nCreateTime);
  NotifyWatchonlyChanged(true);
  return CWalletDB(*dbw).WriteWatchOnly(dest, meta);
}

bool CWallet::AddWatchOnly(const CScript &dest, int64_t nCreateTime) {
  m_script_metadata[CScriptID(dest)].nCreateTime = nCreateTime;
  return AddWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest) {
  AssertLockHeld(cs_wallet);
  if (!CCryptoKeyStore::RemoveWatchOnly(dest))
    return false;
  if (!HaveWatchOnly())
    NotifyWatchonlyChanged(false);
  if (!CWalletDB(*dbw).EraseWatchOnly(dest))
    return false;

  return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest) {
  return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString &strWalletPassphrase) {
  CCrypter crypter;
  CKeyingMaterial _vMasterKey;

  {
    LOCK(cs_wallet);
    for (const MasterKeyMap::value_type &pMasterKey : mapMasterKeys) {
      if (!crypter.SetKeyFromPassphrase(strWalletPassphrase,
                                        pMasterKey.second.vchSalt,
                                        pMasterKey.second.nDeriveIterations,
                                        pMasterKey.second.nDerivationMethod))
        return false;
      if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
        continue;

      if (CCryptoKeyStore::Unlock(_vMasterKey))
        return true;
    }
  }
  return false;
}

bool CWallet::ChangeWalletPassphrase(
    const SecureString &strOldWalletPassphrase,
    const SecureString &strNewWalletPassphrase) {
  bool fWasLocked = IsLocked();

  {
    LOCK(cs_wallet);
    Lock();

    CCrypter crypter;
    CKeyingMaterial _vMasterKey;
    for (MasterKeyMap::value_type &pMasterKey : mapMasterKeys) {
      if (!crypter.SetKeyFromPassphrase(strOldWalletPassphrase,
                                        pMasterKey.second.vchSalt,
                                        pMasterKey.second.nDeriveIterations,
                                        pMasterKey.second.nDerivationMethod))
        return false;
      if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
        return false;
      if (CCryptoKeyStore::Unlock(_vMasterKey)) {
        int64_t nStartTime = GetTimeMillis();
        crypter.SetKeyFromPassphrase(strNewWalletPassphrase,
                                     pMasterKey.second.vchSalt,
                                     pMasterKey.second.nDeriveIterations,
                                     pMasterKey.second.nDerivationMethod);
        pMasterKey.second.nDeriveIterations = static_cast<unsigned int>(
            pMasterKey.second.nDeriveIterations *
            (100 / ((double)(GetTimeMillis() - nStartTime))));

        nStartTime = GetTimeMillis();
        crypter.SetKeyFromPassphrase(strNewWalletPassphrase,
                                     pMasterKey.second.vchSalt,
                                     pMasterKey.second.nDeriveIterations,
                                     pMasterKey.second.nDerivationMethod);
        pMasterKey.second.nDeriveIterations =
            (pMasterKey.second.nDeriveIterations +
             static_cast<unsigned int>(
                 pMasterKey.second.nDeriveIterations * 100 /
                 ((double)(GetTimeMillis() - nStartTime)))) /
            2;

        if (pMasterKey.second.nDeriveIterations < 25000)
          pMasterKey.second.nDeriveIterations = 25000;

        LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n",
                  pMasterKey.second.nDeriveIterations);

        if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase,
                                          pMasterKey.second.vchSalt,
                                          pMasterKey.second.nDeriveIterations,
                                          pMasterKey.second.nDerivationMethod))
          return false;
        if (!crypter.Encrypt(_vMasterKey, pMasterKey.second.vchCryptedKey))
          return false;
        CWalletDB(*dbw).WriteMasterKey(pMasterKey.first, pMasterKey.second);
        if (fWasLocked)
          Lock();
        return true;
      }
    }
  }

  return false;
}

void CWallet::SetBestChain(const CBlockLocator &loc) {
  CWalletDB walletdb(*dbw);
  walletdb.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB *pwalletdbIn,
                            bool fExplicit) {
  LOCK(cs_wallet);

  if (nWalletVersion >= nVersion)
    return true;

  if (fExplicit && nVersion > nWalletMaxVersion)
    nVersion = FEATURE_LATEST;

  nWalletVersion = nVersion;

  if (nVersion > nWalletMaxVersion)
    nWalletMaxVersion = nVersion;

  {
    CWalletDB *pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(*dbw);
    if (nWalletVersion > 40000)
      pwalletdb->WriteMinVersion(nWalletVersion);
    if (!pwalletdbIn)
      delete pwalletdb;
  }

  return true;
}

bool CWallet::SetMaxVersion(int nVersion) {
  LOCK(cs_wallet);

  if (nWalletVersion > nVersion)
    return false;

  nWalletMaxVersion = nVersion;

  return true;
}

std::set<uint256> CWallet::GetConflicts(const uint256 &txid) const {
  std::set<uint256> result;
  AssertLockHeld(cs_wallet);

  std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
  if (it == mapWallet.end())
    return result;
  const CWalletTx &wtx = it->second;

  std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

  for (const CTxIn &txin : wtx.tx->vin) {
    if (mapTxSpends.count(txin.prevout) <= 1)
      continue;

    range = mapTxSpends.equal_range(txin.prevout);
    for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
      result.insert(_it->second);
  }
  return result;
}

bool CWallet::HasWalletSpend(const uint256 &txid) const {
  AssertLockHeld(cs_wallet);
  auto iter = mapTxSpends.lower_bound(COutPoint(txid, 0));
  return (iter != mapTxSpends.end() && iter->first.hash == txid);
}

void CWallet::Flush(bool shutdown) { dbw->Flush(shutdown); }

void CWallet::SyncMetaData(
    std::pair<TxSpends::iterator, TxSpends::iterator> range) {
  int nMinOrderPos = std::numeric_limits<int>::max();
  const CWalletTx *copyFrom = nullptr;
  for (TxSpends::iterator it = range.first; it != range.second; ++it) {
    const CWalletTx *wtx = &mapWallet[it->second];
    if (wtx->nOrderPos < nMinOrderPos) {
      nMinOrderPos = wtx->nOrderPos;
      ;
      copyFrom = wtx;
    }
  }

  assert(copyFrom);

  for (TxSpends::iterator it = range.first; it != range.second; ++it) {
    const uint256 &hash = it->second;
    CWalletTx *copyTo = &mapWallet[hash];
    if (copyFrom == copyTo)
      continue;
    assert(copyFrom &&
           "Oldest wallet transaction in range assumed to have been found.");
    if (!copyFrom->IsEquivalentTo(*copyTo))
      continue;
    copyTo->mapValue = copyFrom->mapValue;
    copyTo->vOrderForm = copyFrom->vOrderForm;

    copyTo->nTimeSmart = copyFrom->nTimeSmart;
    copyTo->fFromMe = copyFrom->fFromMe;
    copyTo->strFromAccount = copyFrom->strFromAccount;
  }
}

bool CWallet::IsSpent(const uint256 &hash, unsigned int n) const {
  const COutPoint outpoint(hash, n);
  std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
  range = mapTxSpends.equal_range(outpoint);

  for (TxSpends::const_iterator it = range.first; it != range.second; ++it) {
    const uint256 &wtxid = it->second;
    std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
    if (mit != mapWallet.end()) {
      int depth = mit->second.GetDepthInMainChain();
      if (depth > 0 || (depth == 0 && !mit->second.isAbandoned()))
        return true;
    }
  }
  return false;
}

void CWallet::AddToSpends(const COutPoint &outpoint, const uint256 &wtxid) {
  mapTxSpends.insert(std::make_pair(outpoint, wtxid));

  std::pair<TxSpends::iterator, TxSpends::iterator> range;
  range = mapTxSpends.equal_range(outpoint);
  SyncMetaData(range);
}

void CWallet::AddToSpends(const uint256 &wtxid) {
  auto it = mapWallet.find(wtxid);
  assert(it != mapWallet.end());
  CWalletTx &thisTx = it->second;
  if (thisTx.IsCoinBase())

    return;

  for (const CTxIn &txin : thisTx.tx->vin)
    AddToSpends(txin.prevout, wtxid);
}

bool CWallet::EncryptWallet(const SecureString &strWalletPassphrase) {
  if (IsCrypted())
    return false;

  CKeyingMaterial _vMasterKey;

  _vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
  GetStrongRandBytes(&_vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

  CMasterKey kMasterKey;

  kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
  GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

  CCrypter crypter;
  int64_t nStartTime = GetTimeMillis();
  crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000,
                               kMasterKey.nDerivationMethod);
  kMasterKey.nDeriveIterations = static_cast<unsigned int>(
      2500000 / ((double)(GetTimeMillis() - nStartTime)));

  nStartTime = GetTimeMillis();
  crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt,
                               kMasterKey.nDeriveIterations,
                               kMasterKey.nDerivationMethod);
  kMasterKey.nDeriveIterations =
      (kMasterKey.nDeriveIterations +
       static_cast<unsigned int>(kMasterKey.nDeriveIterations * 100 /
                                 ((double)(GetTimeMillis() - nStartTime)))) /
      2;

  if (kMasterKey.nDeriveIterations < 25000)
    kMasterKey.nDeriveIterations = 25000;

  LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n",
            kMasterKey.nDeriveIterations);

  if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt,
                                    kMasterKey.nDeriveIterations,
                                    kMasterKey.nDerivationMethod))
    return false;
  if (!crypter.Encrypt(_vMasterKey, kMasterKey.vchCryptedKey))
    return false;

  {
    LOCK(cs_wallet);
    mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
    assert(!pwalletdbEncryption);
    pwalletdbEncryption = new CWalletDB(*dbw);
    if (!pwalletdbEncryption->TxnBegin()) {
      delete pwalletdbEncryption;
      pwalletdbEncryption = nullptr;
      return false;
    }
    pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);

    if (!EncryptKeys(_vMasterKey)) {
      pwalletdbEncryption->TxnAbort();
      delete pwalletdbEncryption;

      assert(false);
    }

    SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

    if (!pwalletdbEncryption->TxnCommit()) {
      delete pwalletdbEncryption;

      assert(false);
    }

    delete pwalletdbEncryption;
    pwalletdbEncryption = nullptr;

    Lock();
    Unlock(strWalletPassphrase);

    if (IsHDEnabled()) {
      if (!SetHDMasterKey(GenerateNewHDMasterKey())) {
        return false;
      }
    }

    NewKeyPool();
    Lock();

    dbw->Rewrite();
  }
  NotifyStatusChanged(this);

  return true;
}

DBErrors CWallet::ReorderTransactions() {
  LOCK(cs_wallet);
  CWalletDB walletdb(*dbw);

  typedef std::pair<CWalletTx *, CAccountingEntry *> TxPair;
  typedef std::multimap<int64_t, TxPair> TxItems;
  TxItems txByTime;

  for (auto &entry : mapWallet) {
    CWalletTx *wtx = &entry.second;
    txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, nullptr)));
  }
  std::list<CAccountingEntry> acentries;
  walletdb.ListAccountCreditDebit("", acentries);
  for (CAccountingEntry &entry : acentries) {
    txByTime.insert(std::make_pair(entry.nTime, TxPair(nullptr, &entry)));
  }

  nOrderPosNext = 0;
  std::vector<int64_t> nOrderPosOffsets;
  for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it) {
    CWalletTx *const pwtx = (*it).second.first;
    CAccountingEntry *const pacentry = (*it).second.second;
    int64_t &nOrderPos =
        (pwtx != nullptr) ? pwtx->nOrderPos : pacentry->nOrderPos;

    if (nOrderPos == -1) {
      nOrderPos = nOrderPosNext++;
      nOrderPosOffsets.push_back(nOrderPos);

      if (pwtx) {
        if (!walletdb.WriteTx(*pwtx))
          return DB_LOAD_FAIL;
      } else if (!walletdb.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
        return DB_LOAD_FAIL;
    } else {
      int64_t nOrderPosOff = 0;
      for (const int64_t &nOffsetStart : nOrderPosOffsets) {
        if (nOrderPos >= nOffsetStart)
          ++nOrderPosOff;
      }
      nOrderPos += nOrderPosOff;
      nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

      if (!nOrderPosOff)
        continue;

      if (pwtx) {
        if (!walletdb.WriteTx(*pwtx))
          return DB_LOAD_FAIL;
      } else if (!walletdb.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
        return DB_LOAD_FAIL;
    }
  }
  walletdb.WriteOrderPosNext(nOrderPosNext);

  return DB_LOAD_OK;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb) {
  AssertLockHeld(cs_wallet);

  int64_t nRet = nOrderPosNext++;
  if (pwalletdb) {
    pwalletdb->WriteOrderPosNext(nOrderPosNext);
  } else {
    CWalletDB(*dbw).WriteOrderPosNext(nOrderPosNext);
  }
  return nRet;
}

bool CWallet::AccountMove(std::string strFrom, std::string strTo,
                          CAmount nAmount, std::string strComment) {
  CWalletDB walletdb(*dbw);
  if (!walletdb.TxnBegin())
    return false;

  int64_t nNow = GetAdjustedTime();

  CAccountingEntry debit;
  debit.nOrderPos = IncOrderPosNext(&walletdb);
  debit.strAccount = strFrom;
  debit.nCreditDebit = -nAmount;
  debit.nTime = nNow;
  debit.strOtherAccount = strTo;
  debit.strComment = strComment;
  AddAccountingEntry(debit, &walletdb);

  CAccountingEntry credit;
  credit.nOrderPos = IncOrderPosNext(&walletdb);
  credit.strAccount = strTo;
  credit.nCreditDebit = nAmount;
  credit.nTime = nNow;
  credit.strOtherAccount = strFrom;
  credit.strComment = strComment;
  AddAccountingEntry(credit, &walletdb);

  if (!walletdb.TxnCommit())
    return false;

  return true;
}

bool CWallet::GetAccountDestination(CTxDestination &dest,
                                    std::string strAccount, bool bForceNew) {
  CWalletDB walletdb(*dbw);

  CAccount account;
  walletdb.ReadAccount(strAccount, account);

  if (!bForceNew) {
    if (!account.vchPubKey.IsValid())
      bForceNew = true;
    else {
      CScript scriptPubKey = GetScriptForDestination(
          GetDestinationForKey(account.vchPubKey, g_address_type));
      for (std::map<uint256, CWalletTx>::iterator it = mapWallet.begin();
           it != mapWallet.end() && account.vchPubKey.IsValid(); ++it)
        for (const CTxOut &txout : (*it).second.tx->vout)
          if (txout.scriptPubKey == scriptPubKey) {
            bForceNew = true;
            break;
          }
    }
  }

  if (bForceNew) {
    if (!GetKeyFromPool(account.vchPubKey, false))
      return false;

    LearnRelatedScripts(account.vchPubKey, g_address_type);
    dest = GetDestinationForKey(account.vchPubKey, g_address_type);
    SetAddressBook(dest, strAccount, "receive");
    walletdb.WriteAccount(strAccount, account);
  } else {
    dest = GetDestinationForKey(account.vchPubKey, g_address_type);
  }

  return true;
}

void CWallet::MarkDirty() {
  {
    LOCK(cs_wallet);
    for (std::pair<const uint256, CWalletTx> &item : mapWallet)
      item.second.MarkDirty();
  }
}

bool CWallet::MarkReplaced(const uint256 &originalHash,
                           const uint256 &newHash) {
  LOCK(cs_wallet);

  auto mi = mapWallet.find(originalHash);

  assert(mi != mapWallet.end());

  CWalletTx &wtx = (*mi).second;

  assert(wtx.mapValue.count("replaced_by_txid") == 0);

  wtx.mapValue["replaced_by_txid"] = newHash.ToString();

  CWalletDB walletdb(*dbw, "r+");

  bool success = true;
  if (!walletdb.WriteTx(wtx)) {
    LogPrintf("%s: Updating walletdb tx %s failed", __func__,
              wtx.GetHash().ToString());
    success = false;
  }

  NotifyTransactionChanged(this, originalHash, CT_UPDATED);

  return success;
}

bool CWallet::AddToWallet(const CWalletTx &wtxIn, bool fFlushOnClose) {
  LOCK(cs_wallet);

  CWalletDB walletdb(*dbw, "r+", fFlushOnClose);

  uint256 hash = wtxIn.GetHash();

  std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret =
      mapWallet.insert(std::make_pair(hash, wtxIn));
  CWalletTx &wtx = (*ret.first).second;
  wtx.BindWallet(this);
  bool fInsertedNew = ret.second;
  if (fInsertedNew) {
    wtx.nTimeReceived = GetAdjustedTime();
    wtx.nOrderPos = IncOrderPosNext(&walletdb);
    wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
    wtx.nTimeSmart = ComputeTimeSmart(wtx);
    AddToSpends(hash);
  }

  bool fUpdated = false;
  if (!fInsertedNew) {
    if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock) {
      wtx.hashBlock = wtxIn.hashBlock;
      fUpdated = true;
    }

    if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned()) {
      wtx.hashBlock = wtxIn.hashBlock;
      fUpdated = true;
    }
    if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex)) {
      wtx.nIndex = wtxIn.nIndex;
      fUpdated = true;
    }
    if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe) {
      wtx.fFromMe = wtxIn.fFromMe;
      fUpdated = true;
    }

    if (wtxIn.tx->HasWitness() && !wtx.tx->HasWitness()) {
      wtx.SetTx(wtxIn.tx);
      fUpdated = true;
    }
  }

  LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(),
            (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

  if (fInsertedNew || fUpdated)
    if (!walletdb.WriteTx(wtx))
      return false;

  wtx.MarkDirty();

  NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

  std::string strCmd = gArgs.GetArg("-walletnotify", "");

  if (!strCmd.empty()) {
    boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
    boost::thread t(runCommand, strCmd);
  }

  return true;
}

bool CWallet::LoadToWallet(const CWalletTx &wtxIn) {
  uint256 hash = wtxIn.GetHash();
  CWalletTx &wtx = mapWallet.emplace(hash, wtxIn).first->second;
  wtx.BindWallet(this);
  wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
  AddToSpends(hash);
  for (const CTxIn &txin : wtx.tx->vin) {
    auto it = mapWallet.find(txin.prevout.hash);
    if (it != mapWallet.end()) {
      CWalletTx &prevtx = it->second;
      if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
        MarkConflicted(prevtx.hashBlock, wtx.GetHash());
      }
    }
  }

  return true;
}

bool CWallet::AddToWalletIfInvolvingMe(const CTransactionRef &ptx,
                                       const CBlockIndex *pIndex,
                                       int posInBlock, bool fUpdate) {
  const CTransaction &tx = *ptx;
  {
    AssertLockHeld(cs_wallet);

    if (pIndex != nullptr) {
      for (const CTxIn &txin : tx.vin) {
        std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range =
            mapTxSpends.equal_range(txin.prevout);
        while (range.first != range.second) {
          if (range.first->second != tx.GetHash()) {
            LogPrintf("Transaction %s (in block %s) conflicts with wallet "
                      "transaction %s (both spend %s:%i)\n",
                      tx.GetHash().ToString(),
                      pIndex->GetBlockHash().ToString(),
                      range.first->second.ToString(),
                      range.first->first.hash.ToString(), range.first->first.n);
            MarkConflicted(pIndex->GetBlockHash(), range.first->second);
          }
          range.first++;
        }
      }
    }

    bool fExisted = mapWallet.count(tx.GetHash()) != 0;
    if (fExisted && !fUpdate)
      return false;
    if (fExisted || IsMine(tx) || IsFromMe(tx)) {
      for (const CTxOut &txout : tx.vout) {
        std::vector<CKeyID> vAffected;
        CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
        for (const CKeyID &keyid : vAffected) {
          std::map<CKeyID, int64_t>::const_iterator mi =
              m_pool_key_to_index.find(keyid);
          if (mi != m_pool_key_to_index.end()) {
            LogPrintf("%s: Detected a used keypool key, mark all keypool key "
                      "up to this key as used\n",
                      __func__);
            MarkReserveKeysAsUsed(mi->second);

            if (!TopUpKeyPool()) {
              LogPrintf("%s: Topping up keypool failed (locked wallet)\n",
                        __func__);
            }
          }
        }
      }

      CWalletTx wtx(this, ptx);

      if (pIndex != nullptr)
        wtx.SetMerkleBranch(pIndex, posInBlock);

      return AddToWallet(wtx, false);
    }
  }
  return false;
}

bool CWallet::TransactionCanBeAbandoned(const uint256 &hashTx) const {
  LOCK2(cs_main, cs_wallet);
  const CWalletTx *wtx = GetWalletTx(hashTx);
  return wtx && !wtx->isAbandoned() && wtx->GetDepthInMainChain() <= 0 &&
         !wtx->InMempool();
}

bool CWallet::AbandonTransaction(const uint256 &hashTx) {
  LOCK2(cs_main, cs_wallet);

  CWalletDB walletdb(*dbw, "r+");

  std::set<uint256> todo;
  std::set<uint256> done;

  auto it = mapWallet.find(hashTx);
  assert(it != mapWallet.end());
  CWalletTx &origtx = it->second;
  if (origtx.GetDepthInMainChain() > 0 || origtx.InMempool()) {
    return false;
  }

  todo.insert(hashTx);

  while (!todo.empty()) {
    uint256 now = *todo.begin();
    todo.erase(now);
    done.insert(now);
    auto it = mapWallet.find(now);
    assert(it != mapWallet.end());
    CWalletTx &wtx = it->second;
    int currentconfirm = wtx.GetDepthInMainChain();

    assert(currentconfirm <= 0);

    if (currentconfirm == 0 && !wtx.isAbandoned()) {
      assert(!wtx.InMempool());
      wtx.nIndex = -1;
      wtx.setAbandoned();
      wtx.MarkDirty();
      walletdb.WriteTx(wtx);
      NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);

      TxSpends::const_iterator iter =
          mapTxSpends.lower_bound(COutPoint(hashTx, 0));
      while (iter != mapTxSpends.end() && iter->first.hash == now) {
        if (!done.count(iter->second)) {
          todo.insert(iter->second);
        }
        iter++;
      }

      for (const CTxIn &txin : wtx.tx->vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
          it->second.MarkDirty();
        }
      }
    }
  }

  return true;
}

void CWallet::MarkConflicted(const uint256 &hashBlock, const uint256 &hashTx) {
  LOCK2(cs_main, cs_wallet);

  int conflictconfirms = 0;
  if (mapBlockIndex.count(hashBlock)) {
    CBlockIndex *pindex = mapBlockIndex[hashBlock];
    if (chainActive.Contains(pindex)) {
      conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
    }
  }

  if (conflictconfirms >= 0)
    return;

  CWalletDB walletdb(*dbw, "r+", false);

  std::set<uint256> todo;
  std::set<uint256> done;

  todo.insert(hashTx);

  while (!todo.empty()) {
    uint256 now = *todo.begin();
    todo.erase(now);
    done.insert(now);
    auto it = mapWallet.find(now);
    assert(it != mapWallet.end());
    CWalletTx &wtx = it->second;
    int currentconfirm = wtx.GetDepthInMainChain();
    if (conflictconfirms < currentconfirm) {
      wtx.nIndex = -1;
      wtx.hashBlock = hashBlock;
      wtx.MarkDirty();
      walletdb.WriteTx(wtx);

      TxSpends::const_iterator iter =
          mapTxSpends.lower_bound(COutPoint(now, 0));
      while (iter != mapTxSpends.end() && iter->first.hash == now) {
        if (!done.count(iter->second)) {
          todo.insert(iter->second);
        }
        iter++;
      }

      for (const CTxIn &txin : wtx.tx->vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
          it->second.MarkDirty();
        }
      }
    }
  }
}

void CWallet::SyncTransaction(const CTransactionRef &ptx,
                              const CBlockIndex *pindex, int posInBlock) {
  const CTransaction &tx = *ptx;

  if (!AddToWalletIfInvolvingMe(ptx, pindex, posInBlock, true))
    return;

  for (const CTxIn &txin : tx.vin) {
    auto it = mapWallet.find(txin.prevout.hash);
    if (it != mapWallet.end()) {
      it->second.MarkDirty();
    }
  }
}

void CWallet::TransactionAddedToMempool(const CTransactionRef &ptx) {
  LOCK2(cs_main, cs_wallet);
  SyncTransaction(ptx);

  auto it = mapWallet.find(ptx->GetHash());
  if (it != mapWallet.end()) {
    it->second.fInMempool = true;
  }
}

void CWallet::TransactionRemovedFromMempool(const CTransactionRef &ptx) {
  LOCK(cs_wallet);
  auto it = mapWallet.find(ptx->GetHash());
  if (it != mapWallet.end()) {
    it->second.fInMempool = false;
  }
}

void CWallet::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex,
    const std::vector<CTransactionRef> &vtxConflicted) {
  LOCK2(cs_main, cs_wallet);

  for (const CTransactionRef &ptx : vtxConflicted) {
    SyncTransaction(ptx);
    TransactionRemovedFromMempool(ptx);
  }
  for (size_t i = 0; i < pblock->vtx.size(); i++) {
    SyncTransaction(pblock->vtx[i], pindex, i);
    TransactionRemovedFromMempool(pblock->vtx[i]);
  }

  m_last_block_processed = pindex;
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) {
  LOCK2(cs_main, cs_wallet);

  for (const CTransactionRef &ptx : pblock->vtx) {
    SyncTransaction(ptx);
  }
}

void CWallet::BlockUntilSyncedToCurrentChain() {
  AssertLockNotHeld(cs_main);
  AssertLockNotHeld(cs_wallet);

  {
    LOCK(cs_main);
    const CBlockIndex *initialChainTip = chainActive.Tip();

    if (m_last_block_processed->GetAncestor(initialChainTip->nHeight) ==
        initialChainTip) {
      return;
    }
  }

  SyncWithValidationInterfaceQueue();
}

isminetype CWallet::IsMine(const CTxIn &txin) const {
  {
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator mi =
        mapWallet.find(txin.prevout.hash);
    if (mi != mapWallet.end()) {
      const CWalletTx &prev = (*mi).second;
      if (txin.prevout.n < prev.tx->vout.size())
        return IsMine(prev.tx->vout[txin.prevout.n]);
    }
  }
  return ISMINE_NO;
}

CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter &filter) const {
  {
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator mi =
        mapWallet.find(txin.prevout.hash);
    if (mi != mapWallet.end()) {
      const CWalletTx &prev = (*mi).second;
      if (txin.prevout.n < prev.tx->vout.size())
        if (IsMine(prev.tx->vout[txin.prevout.n]) & filter)
          return prev.tx->vout[txin.prevout.n].nValue;
    }
  }
  return 0;
}

isminetype CWallet::IsMine(const CTxOut &txout) const {
  return ::IsMine(*this, txout.scriptPubKey);
}

CAmount CWallet::GetCredit(const CTxOut &txout,
                           const isminefilter &filter) const {
  if (!MoneyRange(txout.nValue))
    throw std::runtime_error(std::string(__func__) + ": value out of range");
  return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut &txout) const {
  if (::IsMine(*this, txout.scriptPubKey)) {
    CTxDestination address;
    if (!ExtractDestination(txout.scriptPubKey, address))
      return true;

    LOCK(cs_wallet);
    if (!mapAddressBook.count(address))
      return true;
  }
  return false;
}

CAmount CWallet::GetChange(const CTxOut &txout) const {
  if (!MoneyRange(txout.nValue))
    throw std::runtime_error(std::string(__func__) + ": value out of range");
  return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction &tx) const {
  for (const CTxOut &txout : tx.vout)
    if (IsMine(txout))
      return true;
  return false;
}

bool CWallet::IsFromMe(const CTransaction &tx) const {
  return (GetDebit(tx, ISMINE_ALL) > 0);
}

CAmount CWallet::GetDebit(const CTransaction &tx,
                          const isminefilter &filter) const {
  CAmount nDebit = 0;
  for (const CTxIn &txin : tx.vin) {
    nDebit += GetDebit(txin, filter);
    if (!MoneyRange(nDebit))
      throw std::runtime_error(std::string(__func__) + ": value out of range");
  }
  return nDebit;
}

bool CWallet::IsAllFromMe(const CTransaction &tx,
                          const isminefilter &filter) const {
  LOCK(cs_wallet);

  for (const CTxIn &txin : tx.vin) {
    auto mi = mapWallet.find(txin.prevout.hash);
    if (mi == mapWallet.end())
      return false;

    const CWalletTx &prev = (*mi).second;

    if (txin.prevout.n >= prev.tx->vout.size())
      return false;

    if (!(IsMine(prev.tx->vout[txin.prevout.n]) & filter))
      return false;
  }
  return true;
}

CAmount CWallet::GetCredit(const CTransaction &tx,
                           const isminefilter &filter) const {
  CAmount nCredit = 0;
  for (const CTxOut &txout : tx.vout) {
    nCredit += GetCredit(txout, filter);
    if (!MoneyRange(nCredit))
      throw std::runtime_error(std::string(__func__) + ": value out of range");
  }
  return nCredit;
}

CAmount CWallet::GetChange(const CTransaction &tx) const {
  CAmount nChange = 0;
  for (const CTxOut &txout : tx.vout) {
    nChange += GetChange(txout);
    if (!MoneyRange(nChange))
      throw std::runtime_error(std::string(__func__) + ": value out of range");
  }
  return nChange;
}

CPubKey CWallet::GenerateNewHDMasterKey() {
  CKey key;
  key.MakeNewKey(true);

  int64_t nCreationTime = GetTime();
  CKeyMetadata metadata(nCreationTime);

  CPubKey pubkey = key.GetPubKey();
  assert(key.VerifyPubKey(pubkey));

  metadata.hdKeypath = "m";
  metadata.hdMasterKeyID = pubkey.GetID();

  {
    LOCK(cs_wallet);

    mapKeyMetadata[pubkey.GetID()] = metadata;

    if (!AddKeyPubKey(key, pubkey))
      throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
  }

  return pubkey;
}

bool CWallet::SetHDMasterKey(const CPubKey &pubkey) {
  LOCK(cs_wallet);

  CHDChain newHdChain;
  newHdChain.nVersion = CanSupportFeature(FEATURE_HD_SPLIT)
                            ? CHDChain::VERSION_HD_CHAIN_SPLIT
                            : CHDChain::VERSION_HD_BASE;
  newHdChain.masterKeyID = pubkey.GetID();
  SetHDChain(newHdChain, false);

  return true;
}

bool CWallet::SetHDChain(const CHDChain &chain, bool memonly) {
  LOCK(cs_wallet);
  if (!memonly && !CWalletDB(*dbw).WriteHDChain(chain))
    throw std::runtime_error(std::string(__func__) + ": writing chain failed");

  hdChain = chain;
  return true;
}

bool CWallet::IsHDEnabled() const { return !hdChain.masterKeyID.IsNull(); }

int64_t CWalletTx::GetTxTime() const {
  int64_t n = nTimeSmart;
  return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const {
  int nRequests = -1;
  {
    LOCK(pwallet->cs_wallet);
    if (IsCoinBase()) {
      if (!hashUnset()) {
        std::map<uint256, int>::const_iterator mi =
            pwallet->mapRequestCount.find(hashBlock);
        if (mi != pwallet->mapRequestCount.end())
          nRequests = (*mi).second;
      }
    } else {
      std::map<uint256, int>::const_iterator mi =
          pwallet->mapRequestCount.find(GetHash());
      if (mi != pwallet->mapRequestCount.end()) {
        nRequests = (*mi).second;

        if (nRequests == 0 && !hashUnset()) {
          std::map<uint256, int>::const_iterator _mi =
              pwallet->mapRequestCount.find(hashBlock);
          if (_mi != pwallet->mapRequestCount.end())
            nRequests = (*_mi).second;
          else
            nRequests = 1;
        }
      }
    }
  }
  return nRequests;
}

void CWalletTx::GetAmounts(std::list<COutputEntry> &listReceived,
                           std::list<COutputEntry> &listSent, CAmount &nFee,
                           std::string &strSentAccount,
                           const isminefilter &filter) const {
  nFee = 0;
  listReceived.clear();
  listSent.clear();
  strSentAccount = strFromAccount;

  CAmount nDebit = GetDebit(filter);
  if (nDebit > 0)

  {
    CAmount nValueOut = tx->GetValueOut();
    nFee = nDebit - nValueOut;
  }

  for (unsigned int i = 0; i < tx->vout.size(); ++i) {
    const CTxOut &txout = tx->vout[i];
    isminetype fIsMine = pwallet->IsMine(txout);

    if (nDebit > 0) {
      if (pwallet->IsChange(txout))
        continue;
    } else if (!(fIsMine & filter))
      continue;

    CTxDestination address;

    if (!ExtractDestination(txout.scriptPubKey, address) &&
        !txout.scriptPubKey.IsUnspendable()) {
      LogPrintf(
          "CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
          this->GetHash().ToString());
      address = CNoDestination();
    }

    COutputEntry output = {address, txout.nValue, (int)i};

    if (nDebit > 0)
      listSent.push_back(output);

    if (fIsMine & filter)
      listReceived.push_back(output);
  }
}

int64_t CWallet::RescanFromTime(int64_t startTime,
                                const WalletRescanReserver &reserver,
                                bool update) {
  CBlockIndex *startBlock = nullptr;
  {
    LOCK(cs_main);
    startBlock = chainActive.FindEarliestAtLeast(startTime - TIMESTAMP_WINDOW);
    LogPrintf("%s: Rescanning last %i blocks\n", __func__,
              startBlock ? chainActive.Height() - startBlock->nHeight + 1 : 0);
  }

  if (startBlock) {
    const CBlockIndex *const failedBlock =
        ScanForWalletTransactions(startBlock, nullptr, reserver, update);
    if (failedBlock) {
      return failedBlock->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1;
    }
  }
  return startTime;
}

CBlockIndex *CWallet::ScanForWalletTransactions(
    CBlockIndex *pindexStart, CBlockIndex *pindexStop,
    const WalletRescanReserver &reserver, bool fUpdate) {
  int64_t nNow = GetTime();
  const CChainParams &chainParams = Params();

  assert(reserver.isReserved());
  if (pindexStop) {
    assert(pindexStop->nHeight >= pindexStart->nHeight);
  }

  CBlockIndex *pindex = pindexStart;
  CBlockIndex *ret = nullptr;
  {
    fAbortRescan = false;
    ShowProgress(_("Rescanning..."), 0);

    CBlockIndex *tip = nullptr;
    double dProgressStart;
    double dProgressTip;
    {
      LOCK(cs_main);
      tip = chainActive.Tip();
      dProgressStart = GuessVerificationProgress(chainParams.TxData(), pindex);
      dProgressTip = GuessVerificationProgress(chainParams.TxData(), tip);
    }
    while (pindex && !fAbortRescan) {
      if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0) {
        double gvp = 0;
        {
          LOCK(cs_main);
          gvp = GuessVerificationProgress(chainParams.TxData(), pindex);
        }
        ShowProgress(
            _("Rescanning..."),
            std::max(
                1, std::min(99, (int)((gvp - dProgressStart) /
                                      (dProgressTip - dProgressStart) * 100))));
      }
      if (GetTime() >= nNow + 60) {
        nNow = GetTime();
        LOCK(cs_main);
        LogPrintf("Still rescanning. At block %d. Progress=%f\n",
                  pindex->nHeight,
                  GuessVerificationProgress(chainParams.TxData(), pindex));
      }

      CBlock block;
      if (ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
        LOCK2(cs_main, cs_wallet);
        if (pindex && !chainActive.Contains(pindex)) {
          ret = pindex;
          break;
        }
        for (size_t posInBlock = 0; posInBlock < block.vtx.size();
             ++posInBlock) {
          AddToWalletIfInvolvingMe(block.vtx[posInBlock], pindex, posInBlock,
                                   fUpdate);
        }
      } else {
        ret = pindex;
      }
      if (pindex == pindexStop) {
        break;
      }
      {
        LOCK(cs_main);
        pindex = chainActive.Next(pindex);
        if (tip != chainActive.Tip()) {
          tip = chainActive.Tip();

          dProgressTip = GuessVerificationProgress(chainParams.TxData(), tip);
        }
      }
    }
    if (pindex && fAbortRescan) {
      LogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight,
                GuessVerificationProgress(chainParams.TxData(), pindex));
    }
    ShowProgress(_("Rescanning..."), 100);
  }
  return ret;
}

void CWallet::ReacceptWalletTransactions() {
  if (!fBroadcastTransactions)
    return;
  LOCK2(cs_main, cs_wallet);
  std::map<int64_t, CWalletTx *> mapSorted;

  for (std::pair<const uint256, CWalletTx> &item : mapWallet) {
    const uint256 &wtxid = item.first;
    CWalletTx &wtx = item.second;
    assert(wtx.GetHash() == wtxid);

    int nDepth = wtx.GetDepthInMainChain();

    if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
      mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
    }
  }

  for (std::pair<const int64_t, CWalletTx *> &item : mapSorted) {
    CWalletTx &wtx = *(item.second);
    CValidationState state;
    wtx.AcceptToMemoryPool(maxTxFee, state);
  }
}

bool CWalletTx::RelayWalletTransaction(CConnman *connman) {
  assert(pwallet->GetBroadcastTransactions());
  if (!IsCoinBase() && !isAbandoned() && GetDepthInMainChain() == 0) {
    CValidationState state;

    if (InMempool() || AcceptToMemoryPool(maxTxFee, state)) {
      LogPrintf("Relaying wtx %s\n", GetHash().ToString());
      if (connman) {
        CInv inv(MSG_TX, GetHash());
        connman->ForEachNode(
            [&inv](CNode *pnode) { pnode->PushInventory(inv); });
        return true;
      }
    }
  }
  return false;
}

std::set<uint256> CWalletTx::GetConflicts() const {
  std::set<uint256> result;
  if (pwallet != nullptr) {
    uint256 myHash = GetHash();
    result = pwallet->GetConflicts(myHash);
    result.erase(myHash);
  }
  return result;
}

CAmount CWalletTx::GetDebit(const isminefilter &filter) const {
  if (tx->vin.empty())
    return 0;

  CAmount debit = 0;
  if (filter & ISMINE_SPENDABLE) {
    if (fDebitCached)
      debit += nDebitCached;
    else {
      nDebitCached = pwallet->GetDebit(*tx, ISMINE_SPENDABLE);
      fDebitCached = true;
      debit += nDebitCached;
    }
  }
  if (filter & ISMINE_WATCH_ONLY) {
    if (fWatchDebitCached)
      debit += nWatchDebitCached;
    else {
      nWatchDebitCached = pwallet->GetDebit(*tx, ISMINE_WATCH_ONLY);
      fWatchDebitCached = true;
      debit += nWatchDebitCached;
    }
  }
  return debit;
}

CAmount CWalletTx::GetCredit(const isminefilter &filter) const {
  if (IsCoinBase() && GetBlocksToMaturity() > 0)
    return 0;

  CAmount credit = 0;
  if (filter & ISMINE_SPENDABLE) {
    if (fCreditCached)
      credit += nCreditCached;
    else {
      nCreditCached = pwallet->GetCredit(*tx, ISMINE_SPENDABLE);
      fCreditCached = true;
      credit += nCreditCached;
    }
  }
  if (filter & ISMINE_WATCH_ONLY) {
    if (fWatchCreditCached)
      credit += nWatchCreditCached;
    else {
      nWatchCreditCached = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY);
      fWatchCreditCached = true;
      credit += nWatchCreditCached;
    }
  }
  return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache) const {
  if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain()) {
    if (fUseCache && fImmatureCreditCached)
      return nImmatureCreditCached;
    nImmatureCreditCached = pwallet->GetCredit(*tx, ISMINE_SPENDABLE);
    fImmatureCreditCached = true;
    return nImmatureCreditCached;
  }

  return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache) const {
  if (pwallet == nullptr)
    return 0;

  if (IsCoinBase() && GetBlocksToMaturity() > 0)
    return 0;

  if (fUseCache && fAvailableCreditCached)
    return nAvailableCreditCached;

  CAmount nCredit = 0;
  uint256 hashTx = GetHash();
  for (unsigned int i = 0; i < tx->vout.size(); i++) {
    if (!pwallet->IsSpent(hashTx, i)) {
      const CTxOut &txout = tx->vout[i];
      nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
      if (!MoneyRange(nCredit))
        throw std::runtime_error(std::string(__func__) +
                                 " : value out of range");
    }
  }

  nAvailableCreditCached = nCredit;
  fAvailableCreditCached = true;
  return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool fUseCache) const {
  if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain()) {
    if (fUseCache && fImmatureWatchCreditCached)
      return nImmatureWatchCreditCached;
    nImmatureWatchCreditCached = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY);
    fImmatureWatchCreditCached = true;
    return nImmatureWatchCreditCached;
  }

  return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool fUseCache) const {
  if (pwallet == nullptr)
    return 0;

  if (IsCoinBase() && GetBlocksToMaturity() > 0)
    return 0;

  if (fUseCache && fAvailableWatchCreditCached)
    return nAvailableWatchCreditCached;

  CAmount nCredit = 0;
  for (unsigned int i = 0; i < tx->vout.size(); i++) {
    if (!pwallet->IsSpent(GetHash(), i)) {
      const CTxOut &txout = tx->vout[i];
      nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
      if (!MoneyRange(nCredit))
        throw std::runtime_error(std::string(__func__) +
                                 ": value out of range");
    }
  }

  nAvailableWatchCreditCached = nCredit;
  fAvailableWatchCreditCached = true;
  return nCredit;
}

CAmount CWalletTx::GetChange() const {
  if (fChangeCached)
    return nChangeCached;
  nChangeCached = pwallet->GetChange(*tx);
  fChangeCached = true;
  return nChangeCached;
}

bool CWalletTx::InMempool() const { return fInMempool; }

bool CWalletTx::IsTrusted() const {
  if (!CheckFinalTx(*tx))
    return false;
  int nDepth = GetDepthInMainChain();
  if (nDepth >= 1)
    return true;
  if (nDepth < 0)
    return false;
  if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL))

    return false;

  if (!InMempool())
    return false;

  for (const CTxIn &txin : tx->vin) {
    const CWalletTx *parent = pwallet->GetWalletTx(txin.prevout.hash);
    if (parent == nullptr)
      return false;
    const CTxOut &parentOut = parent->tx->vout[txin.prevout.n];
    if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
      return false;
  }
  return true;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx &_tx) const {
  CMutableTransaction tx1 = *this->tx;
  CMutableTransaction tx2 = *_tx.tx;
  for (auto &txin : tx1.vin)
    txin.scriptSig = CScript();
  for (auto &txin : tx2.vin)
    txin.scriptSig = CScript();
  return CTransaction(tx1) == CTransaction(tx2);
}

std::vector<uint256>
CWallet::ResendWalletTransactionsBefore(int64_t nTime, CConnman *connman) {
  std::vector<uint256> result;

  LOCK(cs_wallet);

  std::multimap<unsigned int, CWalletTx *> mapSorted;
  for (std::pair<const uint256, CWalletTx> &item : mapWallet) {
    CWalletTx &wtx = item.second;

    if (wtx.nTimeReceived > nTime)
      continue;
    mapSorted.insert(std::make_pair(wtx.nTimeReceived, &wtx));
  }
  for (std::pair<const unsigned int, CWalletTx *> &item : mapSorted) {
    CWalletTx &wtx = *item.second;
    if (wtx.RelayWalletTransaction(connman))
      result.push_back(wtx.GetHash());
  }
  return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime,
                                       CConnman *connman) {
  if (GetTime() < nNextResend || !fBroadcastTransactions)
    return;
  bool fFirst = (nNextResend == 0);
  nNextResend = GetTime() + GetRand(30 * 60);
  if (fFirst)
    return;

  if (nBestBlockTime < nLastResend)
    return;
  nLastResend = GetTime();

  std::vector<uint256> relayed =
      ResendWalletTransactionsBefore(nBestBlockTime - 5 * 60, connman);
  if (!relayed.empty())
    LogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__,
              relayed.size());
}

CAmount CWallet::GetBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      if (pcoin->IsTrusted())
        nTotal += pcoin->GetAvailableCredit();
    }
  }

  return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 &&
          pcoin->InMempool())
        nTotal += pcoin->GetAvailableCredit();
    }
  }
  return nTotal;
}

CAmount CWallet::GetImmatureBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      nTotal += pcoin->GetImmatureCredit();
    }
  }
  return nTotal;
}

CAmount CWallet::GetWatchOnlyBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      if (pcoin->IsTrusted())
        nTotal += pcoin->GetAvailableWatchOnlyCredit();
    }
  }

  return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 &&
          pcoin->InMempool())
        nTotal += pcoin->GetAvailableWatchOnlyCredit();
    }
  }
  return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const {
  CAmount nTotal = 0;
  {
    LOCK2(cs_main, cs_wallet);
    for (const auto &entry : mapWallet) {
      const CWalletTx *pcoin = &entry.second;
      nTotal += pcoin->GetImmatureWatchOnlyCredit();
    }
  }
  return nTotal;
}

CAmount CWallet::GetLegacyBalance(const isminefilter &filter, int minDepth,
                                  const std::string *account) const {
  LOCK2(cs_main, cs_wallet);

  CAmount balance = 0;
  for (const auto &entry : mapWallet) {
    const CWalletTx &wtx = entry.second;
    const int depth = wtx.GetDepthInMainChain();
    if (depth < 0 || !CheckFinalTx(*wtx.tx) || wtx.GetBlocksToMaturity() > 0) {
      continue;
    }

    CAmount debit = wtx.GetDebit(filter);
    const bool outgoing = debit > 0;
    for (const CTxOut &out : wtx.tx->vout) {
      if (outgoing && IsChange(out)) {
        debit -= out.nValue;
      } else if (IsMine(out) & filter && depth >= minDepth &&
                 (!account || *account == GetAccountName(out.scriptPubKey))) {
        balance += out.nValue;
      }
    }

    if (outgoing && (!account || *account == wtx.strFromAccount)) {
      balance -= debit;
    }
  }

  if (account) {
    balance += CWalletDB(*dbw).GetAccountCreditDebit(*account);
  }

  return balance;
}

CAmount CWallet::GetAvailableBalance(const CCoinControl *coinControl) const {
  LOCK2(cs_main, cs_wallet);

  CAmount balance = 0;
  std::vector<COutput> vCoins;
  AvailableCoins(vCoins, true, coinControl);
  for (const COutput &out : vCoins) {
    if (out.fSpendable) {
      balance += out.tx->tx->vout[out.i].nValue;
    }
  }
  return balance;
}

void CWallet::AvailableCoins(std::vector<COutput> &vCoins, bool fOnlySafe,
                             const CCoinControl *coinControl,
                             const CAmount &nMinimumAmount,
                             const CAmount &nMaximumAmount,
                             const CAmount &nMinimumSumAmount,
                             const uint64_t nMaximumCount, const int nMinDepth,
                             const int nMaxDepth) const {
  vCoins.clear();

  {
    LOCK2(cs_main, cs_wallet);

    CAmount nTotal = 0;

    for (const auto &entry : mapWallet) {
      const uint256 &wtxid = entry.first;
      const CWalletTx *pcoin = &entry.second;

      if (!CheckFinalTx(*pcoin->tx))
        continue;

      if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
        continue;

      int nDepth = pcoin->GetDepthInMainChain();
      if (nDepth < 0)
        continue;

      if (nDepth == 0 && !pcoin->InMempool())
        continue;

      bool safeTx = pcoin->IsTrusted();

      if (nDepth == 0 && pcoin->mapValue.count("replaces_txid")) {
        safeTx = false;
      }

      if (nDepth == 0 && pcoin->mapValue.count("replaced_by_txid")) {
        safeTx = false;
      }

      if (fOnlySafe && !safeTx) {
        continue;
      }

      if (nDepth < nMinDepth || nDepth > nMaxDepth)
        continue;

      for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
        if (pcoin->tx->vout[i].nValue < nMinimumAmount ||
            pcoin->tx->vout[i].nValue > nMaximumAmount)
          continue;

        if (coinControl && coinControl->HasSelected() &&
            !coinControl->fAllowOtherInputs &&
            !coinControl->IsSelected(COutPoint(entry.first, i)))
          continue;

        if (IsLockedCoin(entry.first, i))
          continue;

        if (IsSpent(wtxid, i))
          continue;

        isminetype mine = IsMine(pcoin->tx->vout[i]);

        if (mine == ISMINE_NO) {
          continue;
        }

        bool fSpendableIn = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) ||
                            (coinControl && coinControl->fAllowWatchOnly &&
                             (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO);
        bool fSolvableIn =
            (mine & (ISMINE_SPENDABLE | ISMINE_WATCH_SOLVABLE)) != ISMINE_NO;

        vCoins.push_back(
            COutput(pcoin, i, nDepth, fSpendableIn, fSolvableIn, safeTx));

        if (nMinimumSumAmount != MAX_MONEY) {
          nTotal += pcoin->tx->vout[i].nValue;

          if (nTotal >= nMinimumSumAmount) {
            return;
          }
        }

        if (nMaximumCount > 0 && vCoins.size() >= nMaximumCount) {
          return;
        }
      }
    }
  }
}

std::map<CTxDestination, std::vector<COutput>> CWallet::ListCoins() const {
  std::map<CTxDestination, std::vector<COutput>> result;

  std::vector<COutput> availableCoins;
  AvailableCoins(availableCoins);

  LOCK2(cs_main, cs_wallet);
  for (auto &coin : availableCoins) {
    CTxDestination address;
    if (coin.fSpendable &&
        ExtractDestination(
            FindNonChangeParentOutput(*coin.tx->tx, coin.i).scriptPubKey,
            address)) {
      result[address].emplace_back(std::move(coin));
    }
  }

  std::vector<COutPoint> lockedCoins;
  ListLockedCoins(lockedCoins);
  for (const auto &output : lockedCoins) {
    auto it = mapWallet.find(output.hash);
    if (it != mapWallet.end()) {
      int depth = it->second.GetDepthInMainChain();
      if (depth >= 0 && output.n < it->second.tx->vout.size() &&
          IsMine(it->second.tx->vout[output.n]) == ISMINE_SPENDABLE) {
        CTxDestination address;
        if (ExtractDestination(
                FindNonChangeParentOutput(*it->second.tx, output.n)
                    .scriptPubKey,
                address)) {
          result[address].emplace_back(&it->second, output.n, depth, true, true,
                                       false);
        }
      }
    }
  }

  return result;
}

const CTxOut &CWallet::FindNonChangeParentOutput(const CTransaction &tx,
                                                 int output) const {
  const CTransaction *ptx = &tx;
  int n = output;
  while (IsChange(ptx->vout[n]) && ptx->vin.size() > 0) {
    const COutPoint &prevout = ptx->vin[0].prevout;
    auto it = mapWallet.find(prevout.hash);
    if (it == mapWallet.end() || it->second.tx->vout.size() <= prevout.n ||
        !IsMine(it->second.tx->vout[prevout.n])) {
      break;
    }
    ptx = it->second.tx.get();
    n = prevout.n;
  }
  return ptx->vout[n];
}

static void ApproximateBestSubset(const std::vector<CInputCoin> &vValue,
                                  const CAmount &nTotalLower,
                                  const CAmount &nTargetValue,
                                  std::vector<char> &vfBest, CAmount &nBest,
                                  int iterations = 1000) {
  std::vector<char> vfIncluded;

  vfBest.assign(vValue.size(), true);
  nBest = nTotalLower;

  FastRandomContext insecure_rand;

  for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++) {
    vfIncluded.assign(vValue.size(), false);
    CAmount nTotal = 0;
    bool fReachedTarget = false;
    for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++) {
      for (unsigned int i = 0; i < vValue.size(); i++) {
        if (nPass == 0 ? insecure_rand.randbool() : !vfIncluded[i]) {
          nTotal += vValue[i].txout.nValue;
          vfIncluded[i] = true;
          if (nTotal >= nTargetValue) {
            fReachedTarget = true;
            if (nTotal < nBest) {
              nBest = nTotal;
              vfBest = vfIncluded;
            }
            nTotal -= vValue[i].txout.nValue;
            vfIncluded[i] = false;
          }
        }
      }
    }
  }
}

bool CWallet::SelectCoinsMinConf(const CAmount &nTargetValue,
                                 const int nConfMine, const int nConfTheirs,
                                 const uint64_t nMaxAncestors,
                                 std::vector<COutput> vCoins,
                                 std::set<CInputCoin> &setCoinsRet,
                                 CAmount &nValueRet) const {
  setCoinsRet.clear();
  nValueRet = 0;

  boost::optional<CInputCoin> coinLowestLarger;
  std::vector<CInputCoin> vValue;
  CAmount nTotalLower = 0;

  random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

  for (const COutput &output : vCoins) {
    if (!output.fSpendable)
      continue;

    const CWalletTx *pcoin = output.tx;

    if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
      continue;

    if (!mempool.TransactionWithinChainLimit(pcoin->GetHash(), nMaxAncestors))
      continue;

    int i = output.i;

    CInputCoin coin = CInputCoin(pcoin, i);

    if (coin.txout.nValue == nTargetValue) {
      setCoinsRet.insert(coin);
      nValueRet += coin.txout.nValue;
      return true;
    } else if (coin.txout.nValue < nTargetValue + MIN_CHANGE) {
      vValue.push_back(coin);
      nTotalLower += coin.txout.nValue;
    } else if (!coinLowestLarger ||
               coin.txout.nValue < coinLowestLarger->txout.nValue) {
      coinLowestLarger = coin;
    }
  }

  if (nTotalLower == nTargetValue) {
    for (const auto &input : vValue) {
      setCoinsRet.insert(input);
      nValueRet += input.txout.nValue;
    }
    return true;
  }

  if (nTotalLower < nTargetValue) {
    if (!coinLowestLarger)
      return false;
    setCoinsRet.insert(coinLowestLarger.get());
    nValueRet += coinLowestLarger->txout.nValue;
    return true;
  }

  std::sort(vValue.begin(), vValue.end(), CompareValueOnly());
  std::reverse(vValue.begin(), vValue.end());
  std::vector<char> vfBest;
  CAmount nBest;

  ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest);
  if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE)
    ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE,
                          vfBest, nBest);

  if (coinLowestLarger &&
      ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) ||
       coinLowestLarger->txout.nValue <= nBest)) {
    setCoinsRet.insert(coinLowestLarger.get());
    nValueRet += coinLowestLarger->txout.nValue;
  } else {
    for (unsigned int i = 0; i < vValue.size(); i++)
      if (vfBest[i]) {
        setCoinsRet.insert(vValue[i]);
        nValueRet += vValue[i].txout.nValue;
      }

    if (LogAcceptCategory(BCLog::SELECTCOINS)) {
      LogPrint(BCLog::SELECTCOINS, "SelectCoins() best subset: ");
      for (unsigned int i = 0; i < vValue.size(); i++) {
        if (vfBest[i]) {
          LogPrint(BCLog::SELECTCOINS, "%s ",
                   FormatMoney(vValue[i].txout.nValue));
        }
      }
      LogPrint(BCLog::SELECTCOINS, "total %s\n", FormatMoney(nBest));
    }
  }

  return true;
}

bool CWallet::SelectCoins(const std::vector<COutput> &vAvailableCoins,
                          const CAmount &nTargetValue,
                          std::set<CInputCoin> &setCoinsRet, CAmount &nValueRet,
                          const CCoinControl *coinControl) const {
  std::vector<COutput> vCoins(vAvailableCoins);

  if (coinControl && coinControl->HasSelected() &&
      !coinControl->fAllowOtherInputs) {
    for (const COutput &out : vCoins) {
      if (!out.fSpendable)
        continue;
      nValueRet += out.tx->tx->vout[out.i].nValue;
      setCoinsRet.insert(CInputCoin(out.tx, out.i));
    }
    return (nValueRet >= nTargetValue);
  }

  std::set<CInputCoin> setPresetCoins;
  CAmount nValueFromPresetInputs = 0;

  std::vector<COutPoint> vPresetInputs;
  if (coinControl)
    coinControl->ListSelected(vPresetInputs);
  for (const COutPoint &outpoint : vPresetInputs) {
    std::map<uint256, CWalletTx>::const_iterator it =
        mapWallet.find(outpoint.hash);
    if (it != mapWallet.end()) {
      const CWalletTx *pcoin = &it->second;

      if (pcoin->tx->vout.size() <= outpoint.n)
        return false;
      nValueFromPresetInputs += pcoin->tx->vout[outpoint.n].nValue;
      setPresetCoins.insert(CInputCoin(pcoin, outpoint.n));
    } else
      return false;
  }

  for (std::vector<COutput>::iterator it = vCoins.begin();
       it != vCoins.end() && coinControl && coinControl->HasSelected();) {
    if (setPresetCoins.count(CInputCoin(it->tx, it->i)))
      it = vCoins.erase(it);
    else
      ++it;
  }

  size_t nMaxChainLength =
      std::min(gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT),
               gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));
  bool fRejectLongChains = gArgs.GetBoolArg("-walletrejectlongchains",
                                            DEFAULT_WALLET_REJECT_LONG_CHAINS);

  bool res =
      nTargetValue <= nValueFromPresetInputs ||
      SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, 0, vCoins,
                         setCoinsRet, nValueRet) ||
      SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, 0, vCoins,
                         setCoinsRet, nValueRet) ||
      (bSpendZeroConfChange &&
       SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, 2,
                          vCoins, setCoinsRet, nValueRet)) ||
      (bSpendZeroConfChange &&
       SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1,
                          std::min((size_t)4, nMaxChainLength / 3), vCoins,
                          setCoinsRet, nValueRet)) ||
      (bSpendZeroConfChange &&
       SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1,
                          nMaxChainLength / 2, vCoins, setCoinsRet,
                          nValueRet)) ||
      (bSpendZeroConfChange &&
       SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1,
                          nMaxChainLength, vCoins, setCoinsRet, nValueRet)) ||
      (bSpendZeroConfChange && !fRejectLongChains &&
       SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1,
                          std::numeric_limits<uint64_t>::max(), vCoins,
                          setCoinsRet, nValueRet));

  setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

  nValueRet += nValueFromPresetInputs;

  return res;
}

bool CWallet::SignTransaction(CMutableTransaction &tx) {
  AssertLockHeld(cs_wallet);

  CTransaction txNewConst(tx);
  int nIn = 0;
  for (const auto &input : tx.vin) {
    std::map<uint256, CWalletTx>::const_iterator mi =
        mapWallet.find(input.prevout.hash);
    if (mi == mapWallet.end() ||
        input.prevout.n >= mi->second.tx->vout.size()) {
      return false;
    }
    const CScript &scriptPubKey =
        mi->second.tx->vout[input.prevout.n].scriptPubKey;
    const CAmount &amount = mi->second.tx->vout[input.prevout.n].nValue;
    SignatureData sigdata;
    if (!ProduceSignature(
            TransactionSignatureCreator(this, &txNewConst, nIn, amount,
                                        SIGHASH_ALL | SIGHASH_FORKID),
            scriptPubKey, sigdata)) {
      return false;
    }
    UpdateTransaction(tx, nIn, sigdata);
    nIn++;
  }
  return true;
}

bool CWallet::FundTransaction(CMutableTransaction &tx, CAmount &nFeeRet,
                              int &nChangePosInOut, std::string &strFailReason,
                              bool lockUnspents,
                              const std::set<int> &setSubtractFeeFromOutputs,
                              CCoinControl coinControl) {
  std::vector<CRecipient> vecSend;

  for (size_t idx = 0; idx < tx.vout.size(); idx++) {
    const CTxOut &txOut = tx.vout[idx];
    CRecipient recipient = {txOut.scriptPubKey, txOut.nValue,
                            setSubtractFeeFromOutputs.count(idx) == 1};
    vecSend.push_back(recipient);
  }

  coinControl.fAllowOtherInputs = true;

  for (const CTxIn &txin : tx.vin) {
    coinControl.Select(txin.prevout);
  }

  LOCK2(cs_main, cs_wallet);

  CReserveKey reservekey(this);
  CWalletTx wtx;
  if (!CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePosInOut,
                         strFailReason, coinControl, false)) {
    return false;
  }

  if (nChangePosInOut != -1) {
    tx.vout.insert(tx.vout.begin() + nChangePosInOut,
                   wtx.tx->vout[nChangePosInOut]);

    reservekey.KeepKey();
  }

  for (unsigned int idx = 0; idx < tx.vout.size(); idx++) {
    tx.vout[idx].nValue = wtx.tx->vout[idx].nValue;
  }

  for (const CTxIn &txin : wtx.tx->vin) {
    if (!coinControl.IsSelected(txin.prevout)) {
      tx.vin.push_back(txin);

      if (lockUnspents) {
        LockCoin(txin.prevout);
      }
    }
  }

  return true;
}

OutputType
CWallet::TransactionChangeType(OutputType change_type,
                               const std::vector<CRecipient> &vecSend) {
  if (change_type != OUTPUT_TYPE_NONE) {
    return change_type;
  }

  if (g_address_type == OUTPUT_TYPE_LEGACY) {
    return OUTPUT_TYPE_LEGACY;
  }

  for (const auto &recipient : vecSend) {
    int witnessversion = 0;
    std::vector<unsigned char> witnessprogram;
    if (recipient.scriptPubKey.IsWitnessProgram(witnessversion,
                                                witnessprogram)) {
      return OUTPUT_TYPE_BECH32;
    }
  }

  return g_address_type;
}

bool fWalletUnlockWithoutTransactions = false;

CBeeCreationTransactionInfo
CWallet::GetBCT(const CWalletTx &wtx, bool includeDead, bool scanRewards,
                const Consensus::Params &consensusParams,
                int minHoneyConfirmations) {
  CBeeCreationTransactionInfo bct;

  if (chainActive.Height() == 0)

    return bct;

  int maxDepth =
      consensusParams.beeGestationBlocks + consensusParams.beeLifespanBlocks;

  CScript scriptPubKeyBCF = GetScriptForDestination(
      DecodeDestination(consensusParams.beeCreationAddress));
  CScript scriptPubKeyCF = GetScriptForDestination(
      DecodeDestination(consensusParams.hiveCommunityAddress));

  CAmount beeFeePaid;
  CScript scriptPubKeyHoney;
  if (!wtx.tx->IsBCT(consensusParams, scriptPubKeyBCF, &beeFeePaid,
                     &scriptPubKeyHoney))
    return bct;

  CTxDestination honeyDestination;
  if (!ExtractDestination(scriptPubKeyHoney, honeyDestination)) {
    LogPrintf("** Couldn't extract destination from BCT %s (dest=%s)\n",
              wtx.GetHash().GetHex(), HexStr(scriptPubKeyHoney));
    return bct;
  }
  std::string honeyAddress = EncodeDestination(honeyDestination);

  int depth = wtx.GetDepthInMainChain();
  int blocksLeft = maxDepth - depth;
  blocksLeft++;

  bool isMature = false;
  std::string status = "immature";
  if (blocksLeft < 1) {
    if (!includeDead)

      return bct;
    blocksLeft = 0;
    status = "expired";
    isMature = true;

  } else {
    if (depth > consensusParams.beeGestationBlocks) {
      status = "mature";
      isMature = true;
    }
  }

  int height = chainActive.Height() - depth;
  CAmount beeCost = GetBeeCost(height, consensusParams);
  bool communityContrib = false;
  if (wtx.tx->vout.size() > 1 &&
      wtx.tx->vout[1].scriptPubKey == scriptPubKeyCF) {
    beeFeePaid += wtx.tx->vout[1].nValue;

    communityContrib = true;
  }
  int beeCount = beeFeePaid / beeCost;

  std::string bctTxid = wtx.GetHash().GetHex();
  int blocksFound = 0;
  CAmount rewardsPaid = 0;
  if (isMature && scanRewards) {
    for (const std::pair<uint256, CWalletTx> &pairWtx2 : mapWallet) {
      const CWalletTx &wtx2 = pairWtx2.second;

      if (!wtx2.IsHiveCoinBase())
        continue;

      if (wtx2.GetDepthInMainChain() < minHoneyConfirmations)
        continue;

      std::vector<unsigned char> blockTxid(
          &wtx2.tx->vout[0].scriptPubKey[14],
          &wtx2.tx->vout[0].scriptPubKey[14 + 64]);
      std::string blockTxidStr =
          std::string(blockTxid.begin(), blockTxid.end());

      if (bctTxid != blockTxidStr)
        continue;

      blocksFound++;
      rewardsPaid += wtx2.tx->vout[1].nValue;
    }
  }

  int64_t time = 0;
  if (mapBlockIndex[wtx.hashBlock])
    time = mapBlockIndex[wtx.hashBlock]->GetBlockTime();

  bct.txid = bctTxid;
  bct.time = time;
  bct.beeCount = beeCount;
  bct.beeFeePaid = beeFeePaid;
  bct.communityContrib = communityContrib;
  bct.beeStatus = status;
  bct.honeyAddress = honeyAddress;
  bct.rewardsPaid = rewardsPaid;
  bct.blocksFound = blocksFound;
  bct.blocksLeft = blocksLeft;
  bct.profit = rewardsPaid - beeFeePaid;

  return bct;
}

std::vector<CBeeCreationTransactionInfo>
CWallet::GetBCTs(bool includeDead, bool scanRewards,
                 const Consensus::Params &consensusParams,
                 int minHoneyConfirmations) {
  std::vector<CBeeCreationTransactionInfo> bcts;

  if (chainActive.Height() == 0)

    return bcts;

  CScript scriptPubKeyBCF = GetScriptForDestination(
      DecodeDestination(consensusParams.beeCreationAddress));
  CScript scriptPubKeyCF = GetScriptForDestination(
      DecodeDestination(consensusParams.hiveCommunityAddress));

  for (const std::pair<uint256, CWalletTx> &pairWtx : mapWallet) {
    const CWalletTx &wtx = pairWtx.second;

    if (wtx.GetDepthInMainChain() < 1)
      continue;

    if (wtx.IsCoinBase())
      continue;

    if (!IsAllFromMe(*wtx.tx, ISMINE_SPENDABLE))
      continue;

    CBeeCreationTransactionInfo bct = GetBCT(
        wtx, includeDead, scanRewards, consensusParams, minHoneyConfirmations);
    if (bct.txid != "")
      bcts.push_back(bct);
  }

  return bcts;
}

bool CWallet::CreateBeeTransaction(
    int beeCount, CWalletTx &wtxNew, CReserveKey &reservekeyChange,
    CReserveKey &reservekeyHoney, std::string honeyAddress,
    std::string changeAddress, bool communityContrib,
    std::string &strFailReason, const Consensus::Params &consensusParams) {
  CBlockIndex *pindexPrev = chainActive.Tip();
  assert(pindexPrev != nullptr);

  if (!IsHiveEnabled(pindexPrev, consensusParams)) {
    strFailReason = "Error: The Hive has not yet been activated on the network";
    return false;
  }

  if (beeCount < 1) {
    strFailReason = "Error: At least 1 bee must be created";
    return false;
  }

  CAmount beeCost = GetBeeCost(chainActive.Height(), consensusParams);
  CAmount curBalance = GetAvailableBalance();
  CAmount totalBeeCost = beeCost * beeCount;
  if (totalBeeCost > curBalance) {
    strFailReason = "Error: Insufficient balance to pay bee creation fee";
    return false;
  }

  auto blockReward = GetBlockSubsidy(pindexPrev->nHeight, consensusParams);
  if (IsMinotaurXEnabled(pindexPrev, consensusParams))
    blockReward += blockReward >> 1;

  CAmount totalPotentialReward;
  if (IsHive11Enabled(pindexPrev, consensusParams))
    totalPotentialReward = (consensusParams.beeLifespanBlocks * blockReward) /
                           consensusParams.hiveBlockSpacingTargetTypical_1_1;
  else
    totalPotentialReward = (consensusParams.beeLifespanBlocks * blockReward) /
                           consensusParams.hiveBlockSpacingTargetTypical;

  if (totalPotentialReward < beeCost) {
    strFailReason = "Error: Bee creation would cost more than possible rewards";
    return false;
  }

  CTxDestination destinationFCA;
  if (honeyAddress.empty()) {
    if (!IsLocked())
      TopUpKeyPool();

    CPubKey newKey;
    if (!reservekeyHoney.GetReservedKey(newKey, true)) {
      strFailReason = "Error: Couldn't create a new pubkey";
      return false;
    }

    std::string strLabel = "Hivemined Honey";
    OutputType output_type = OUTPUT_TYPE_LEGACY;
    LearnRelatedScripts(newKey, output_type);
    destinationFCA = GetDestinationForKey(newKey, output_type);
    SetAddressBook(destinationFCA, strLabel, "receive");
  } else {
    destinationFCA = DecodeDestination(honeyAddress);
    if (!IsValidDestination(destinationFCA)) {
      strFailReason = "Error: Invalid honey address specified";
      return false;
    }

    std::vector<std::vector<unsigned char>> vSolutions;
    txnouttype whichType;
    if (!Solver(GetScriptForDestination(destinationFCA), whichType,
                vSolutions)) {
      strFailReason = "Error: Couldn't solve scriptPubKey for honey address";
      return false;
    }
    if (whichType != TX_PUBKEYHASH) {
      strFailReason = "Error: If specifying a honey address, it must be legacy "
                      "format (TX_PUBKEYHASH)";
      return false;
    }

    isminetype isMine =
        ::IsMine((const CKeyStore &)*this,
                 (const CTxDestination &)destinationFCA, SIGVERSION_BASE);
    if (isMine != ISMINE_SPENDABLE) {
      strFailReason = "Error: Wallet doesn't contain the private key for the "
                      "honey address specified";
      return false;
    }
  }

  CTxDestination destinationChange;
  if (!changeAddress.empty()) {
    destinationChange = DecodeDestination(changeAddress);
    if (!IsValidDestination(destinationChange)) {
      strFailReason = "Error: Invalid change address specified";
      return false;
    }

    isminetype isMine =
        ::IsMine((const CKeyStore &)*this,
                 (const CTxDestination &)destinationChange, SIGVERSION_BASE);
    if (isMine != ISMINE_SPENDABLE) {
      strFailReason = "Error: Wallet doesn't contain the private key for the "
                      "change address specified";
      return false;
    }
  }

  std::vector<CRecipient> vecSend;
  CTxDestination destinationBCF =
      DecodeDestination(consensusParams.beeCreationAddress);
  CScript scriptPubKeyBCF = GetScriptForDestination(destinationBCF);
  CScript scriptPubKeyFCA = GetScriptForDestination(destinationFCA);
  scriptPubKeyBCF << OP_RETURN << OP_BEE;
  scriptPubKeyBCF += scriptPubKeyFCA;
  CAmount beeCreationValue = totalBeeCost;
  CAmount donationValue =
      (CAmount)(totalBeeCost / consensusParams.communityContribFactor);

  if (IsMinotaurXEnabled(pindexPrev, consensusParams))
    donationValue += donationValue >> 1;

  if (communityContrib)
    beeCreationValue -= donationValue;
  CRecipient recipientBCF = {scriptPubKeyBCF, beeCreationValue, false};
  vecSend.push_back(recipientBCF);

  if (communityContrib) {
    CTxDestination destinationCF =
        DecodeDestination(consensusParams.hiveCommunityAddress);
    CScript scriptPubKeyCF = GetScriptForDestination(destinationCF);
    CRecipient recipientCF = {scriptPubKeyCF, donationValue, false};
    vecSend.push_back(recipientCF);
  }

  CAmount feeRequired;
  int changePos = communityContrib ? 2 : 1;

  std::string strError;
  CCoinControl coinControl;
  if (!changeAddress.empty())
    coinControl.destChange = destinationChange;
  if (!CreateTransaction(vecSend, wtxNew, reservekeyChange, feeRequired,
                         changePos, strError, coinControl, true)) {
    if (totalBeeCost + feeRequired > curBalance)

      strFailReason = "Error: Insufficient balance to cover bee creation fee "
                      "and transaction fee";
    else
      strFailReason = "Error: Couldn't create BCT: " + strError;
    return false;
  }

  return true;
}

bool CWallet::CreateNickRegistrationTransaction(
    std::string nickname, CWalletTx &wtxNew, CReserveKey &reservekeyChange,
    CReserveKey &reservekeyNickAddress, std::string nickAddress,
    std::string changeAddress, std::string &strFailReason,
    const Consensus::Params &consensusParams) {
  CBlockIndex *pindexPrev = chainActive.Tip();
  assert(pindexPrev != nullptr);

  if (!IsRialtoEnabled(pindexPrev, consensusParams)) {
    strFailReason = "Error: Rialto has not yet been activated on the network";
    return false;
  }

  if (!RialtoIsValidNickFormat(nickname)) {
    strFailReason =
        "Error: Invalid nickname format; must be 3-20 characters in length and "
        "consist of lowercase letters and underscores only.";
    return false;
  }

  if (RialtoNickExists(nickname)) {
    strFailReason = "Error: This nickname is already registered.";
    return false;
  }

  CAmount registrationCost = consensusParams.nickCreationCostStandard;
  if (nickname.length() == 3)
    registrationCost = consensusParams.nickCreationCost3Char;
  else if (nickname.length() == 4)
    registrationCost = consensusParams.nickCreationCost4Char;

  CAmount registrationAntiDust = consensusParams.nickCreationAntiDust;
  CAmount curBalance = GetAvailableBalance();
  if (registrationCost > curBalance) {
    strFailReason =
        "Error: Insufficient balance to pay nickname registration fee";
    return false;
  }

  CTxDestination destinationNA;
  CPubKey pubKey;
  if (nickAddress.empty()) {
    if (!IsLocked())
      TopUpKeyPool();

    if (!reservekeyNickAddress.GetReservedKey(pubKey, true)) {
      strFailReason = "Error: Couldn't create a new pubkey";
      return false;
    }

    std::string strLabel = "Rialto Nick Address for " + nickname;
    LearnRelatedScripts(pubKey, OUTPUT_TYPE_LEGACY);
    destinationNA = GetDestinationForKey(pubKey, OUTPUT_TYPE_LEGACY);
    SetAddressBook(destinationNA, strLabel, "receive");
  } else {
    destinationNA = DecodeDestination(nickAddress);
    if (!IsValidDestination(destinationNA)) {
      strFailReason = "Error: Invalid nick address specified";
      return false;
    }

    std::vector<std::vector<unsigned char>> vSolutions;
    txnouttype whichType;
    if (!Solver(GetScriptForDestination(destinationNA), whichType,
                vSolutions)) {
      strFailReason = "Error: Couldn't solve scriptPubKey for nick address";
      return false;
    }
    if (whichType != TX_PUBKEYHASH) {
      strFailReason = "Error: If specifying a nick address, it must be legacy "
                      "format (TX_PUBKEYHASH)";
      return false;
    }

    isminetype isMine =
        ::IsMine((const CKeyStore &)*this,
                 (const CTxDestination &)destinationNA, SIGVERSION_BASE);
    if (isMine != ISMINE_SPENDABLE) {
      strFailReason = "Error: Wallet doesn't contain the private key for the "
                      "nick address specified";
      return false;
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destinationNA);
    if (!keyID) {
      strFailReason =
          "Error: Can't retrieve key ID for the nick address specified";
      return false;
    }
    CKey key;
    if (!GetKey(*keyID, key)) {
      strFailReason =
          "Error: Can't retrieve key for the nick address specified";
      return false;
    }
    pubKey = key.GetPubKey();
  }

  CTxDestination destinationChange;
  if (!changeAddress.empty()) {
    destinationChange = DecodeDestination(changeAddress);
    if (!IsValidDestination(destinationChange)) {
      strFailReason = "Error: Invalid change address specified";
      return false;
    }

    isminetype isMine =
        ::IsMine((const CKeyStore &)*this,
                 (const CTxDestination &)destinationChange, SIGVERSION_BASE);
    if (isMine != ISMINE_SPENDABLE) {
      strFailReason = "Error: Wallet doesn't contain the private key for the "
                      "change address specified";
      return false;
    }
  }

  std::vector<CRecipient> vecSend;
  CTxDestination destinationNCF =
      DecodeDestination(consensusParams.nickCreationAddress);
  CScript scriptPubKeyNCF = GetScriptForDestination(destinationNCF);
  CRecipient recipientNCF = {scriptPubKeyNCF,
                             registrationCost - registrationAntiDust, false};

  vecSend.push_back(recipientNCF);

  std::vector<unsigned char> nicknameBytes(nickname.begin(), nickname.end());
  std::vector<unsigned char> pubKeyBytes(pubKey.begin(), pubKey.end());
  CScript scriptPubKeyNA;
  scriptPubKeyNA << OP_RETURN << pubKeyBytes << OP_NICK_CREATE << nicknameBytes;

  CRecipient recipientNA = {scriptPubKeyNA, registrationAntiDust, false};

  vecSend.push_back(recipientNA);

  CAmount feeRequired;
  int changePos = 2;

  std::string strError;
  CCoinControl coinControl;
  if (!changeAddress.empty())
    coinControl.destChange = destinationChange;

  if (!CreateTransaction(vecSend, wtxNew, reservekeyChange, feeRequired,
                         changePos, strError, coinControl, true)) {
    if (registrationCost + feeRequired > curBalance)

      strFailReason = "Error: Insufficient balance to cover nick registration "
                      "fee and transaction fee";
    else
      strFailReason = "Error: Couldn't create NCT: " + strError;
    return false;
  }

  return true;
}

bool CWallet::CreateTransaction(const std::vector<CRecipient> &vecSend,
                                CWalletTx &wtxNew, CReserveKey &reservekey,
                                CAmount &nFeeRet, int &nChangePosInOut,
                                std::string &strFailReason,
                                const CCoinControl &coin_control, bool sign) {
  CAmount nValue = 0;
  int nChangePosRequest = nChangePosInOut;
  unsigned int nSubtractFeeFromAmount = 0;
  for (const auto &recipient : vecSend) {
    if (nValue < 0 || recipient.nAmount < 0) {
      strFailReason = _("Transaction amounts must not be negative");
      return false;
    }
    nValue += recipient.nAmount;

    if (recipient.fSubtractFeeFromAmount)
      nSubtractFeeFromAmount++;
  }
  if (vecSend.empty()) {
    strFailReason = _("Transaction must have at least one recipient");
    return false;
  }

  wtxNew.fTimeReceivedIsTxTime = true;
  wtxNew.BindWallet(this);
  CMutableTransaction txNew;

  txNew.nLockTime = chainActive.Height();

  if (GetRandInt(10) == 0)
    txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

  assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
  assert(txNew.nLockTime < LOCKTIME_THRESHOLD);
  FeeCalculation feeCalc;
  CAmount nFeeNeeded;
  unsigned int nBytes;
  {
    std::set<CInputCoin> setCoins;
    LOCK2(cs_main, cs_wallet);
    {
      std::vector<COutput> vAvailableCoins;
      AvailableCoins(vAvailableCoins, true, &coin_control);

      CScript scriptChange;

      if (!boost::get<CNoDestination>(&coin_control.destChange)) {
        scriptChange = GetScriptForDestination(coin_control.destChange);
      } else {
        CPubKey vchPubKey;
        bool ret;
        ret = reservekey.GetReservedKey(vchPubKey, true);
        if (!ret) {
          strFailReason = _("Keypool ran out, please call keypoolrefill first");
          return false;
        }

        const OutputType change_type =
            TransactionChangeType(coin_control.change_type, vecSend);

        LearnRelatedScripts(vchPubKey, change_type);
        scriptChange = GetScriptForDestination(
            GetDestinationForKey(vchPubKey, change_type));
      }
      CTxOut change_prototype_txout(0, scriptChange);
      size_t change_prototype_size =
          GetSerializeSize(change_prototype_txout, SER_DISK, 0);

      CFeeRate discard_rate = GetDiscardRate(::feeEstimator);
      nFeeRet = 0;
      bool pick_new_inputs = true;
      CAmount nValueIn = 0;

      while (true) {
        nChangePosInOut = nChangePosRequest;
        txNew.vin.clear();
        txNew.vout.clear();
        wtxNew.fFromMe = true;
        bool fFirst = true;

        CAmount nValueToSelect = nValue;
        if (nSubtractFeeFromAmount == 0)
          nValueToSelect += nFeeRet;

        for (const auto &recipient : vecSend) {
          CTxOut txout(recipient.nAmount, recipient.scriptPubKey);

          if (recipient.fSubtractFeeFromAmount) {
            assert(nSubtractFeeFromAmount != 0);
            txout.nValue -= nFeeRet / nSubtractFeeFromAmount;

            if (fFirst)

            {
              fFirst = false;
              txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
            }
          }

          if (IsDust(txout, ::dustRelayFee)) {
            if (recipient.fSubtractFeeFromAmount && nFeeRet > 0) {
              if (txout.nValue < 0)
                strFailReason =
                    _("The transaction amount is too small to pay the fee");
              else
                strFailReason = _("The transaction amount is too small to send "
                                  "after the fee has been deducted");
            } else
              strFailReason = _("Transaction amount too small");
            return false;
          }

          txNew.vout.push_back(txout);
        }

        if (pick_new_inputs) {
          nValueIn = 0;
          setCoins.clear();
          if (!SelectCoins(vAvailableCoins, nValueToSelect, setCoins, nValueIn,
                           &coin_control)) {
            strFailReason = _("Insufficient funds");
            return false;
          }
        }

        const CAmount nChange = nValueIn - nValueToSelect;

        if (nChange > 0) {
          CTxOut newTxOut(nChange, scriptChange);

          if (IsDust(newTxOut, discard_rate)) {
            nChangePosInOut = -1;
            nFeeRet += nChange;
          } else {
            if (nChangePosInOut == -1) {
              nChangePosInOut = GetRandInt(txNew.vout.size() + 1);
            } else if ((unsigned int)nChangePosInOut > txNew.vout.size()) {
              strFailReason = _("Change index out of range");
              return false;
            }

            std::vector<CTxOut>::iterator position =
                txNew.vout.begin() + nChangePosInOut;
            txNew.vout.insert(position, newTxOut);
          }
        } else {
          nChangePosInOut = -1;
        }

        const uint32_t nSequence = coin_control.signalRbf
                                       ? MAX_BIP125_RBF_SEQUENCE
                                       : (CTxIn::SEQUENCE_FINAL - 1);
        for (const auto &coin : setCoins)
          txNew.vin.push_back(CTxIn(coin.outpoint, CScript(), nSequence));

        if (!DummySignTx(txNew, setCoins)) {
          strFailReason = _("Signing transaction failed");
          return false;
        }

        nBytes = GetVirtualTransactionSize(txNew);

        for (auto &vin : txNew.vin) {
          vin.scriptSig = CScript();
          vin.scriptWitness.SetNull();
        }

        nFeeNeeded = GetMinimumFee(nBytes, coin_control, ::mempool,
                                   ::feeEstimator, &feeCalc);

        if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes)) {
          strFailReason = _("Transaction too large for fee policy");
          return false;
        }

        if (nFeeRet >= nFeeNeeded) {
          if (nChangePosInOut == -1 && nSubtractFeeFromAmount == 0 &&
              pick_new_inputs) {
            unsigned int tx_size_with_change =
                nBytes + change_prototype_size + 2;

            CAmount fee_needed_with_change =
                GetMinimumFee(tx_size_with_change, coin_control, ::mempool,
                              ::feeEstimator, nullptr);
            CAmount minimum_value_for_change =
                GetDustThreshold(change_prototype_txout, discard_rate);
            if (nFeeRet >= fee_needed_with_change + minimum_value_for_change) {
              pick_new_inputs = false;
              nFeeRet = fee_needed_with_change;
              continue;
            }
          }

          if (nFeeRet > nFeeNeeded && nChangePosInOut != -1 &&
              nSubtractFeeFromAmount == 0) {
            CAmount extraFeePaid = nFeeRet - nFeeNeeded;
            std::vector<CTxOut>::iterator change_position =
                txNew.vout.begin() + nChangePosInOut;
            change_position->nValue += extraFeePaid;
            nFeeRet -= extraFeePaid;
          }
          break;

        } else if (!pick_new_inputs) {
          strFailReason = _("Transaction fee and change calculation failed");
          return false;
        }

        if (nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
          CAmount additionalFeeNeeded = nFeeNeeded - nFeeRet;
          std::vector<CTxOut>::iterator change_position =
              txNew.vout.begin() + nChangePosInOut;

          if (change_position->nValue >=
              MIN_FINAL_CHANGE + additionalFeeNeeded) {
            change_position->nValue -= additionalFeeNeeded;
            nFeeRet += additionalFeeNeeded;
            break;
          }
        }

        if (nSubtractFeeFromAmount > 0) {
          pick_new_inputs = false;
        }

        nFeeRet = nFeeNeeded;
        continue;
      }
    }

    if (nChangePosInOut == -1)
      reservekey.ReturnKey();

    if (sign) {
      CTransaction txNewConst(txNew);
      int nIn = 0;
      for (const auto &coin : setCoins) {
        const CScript &scriptPubKey = coin.txout.scriptPubKey;
        SignatureData sigdata;

        if (!ProduceSignature(TransactionSignatureCreator(
                                  this, &txNewConst, nIn, coin.txout.nValue,
                                  SIGHASH_ALL | SIGHASH_FORKID),
                              scriptPubKey, sigdata))

        {
          strFailReason = _("Signing transaction failed");
          return false;
        } else {
          UpdateTransaction(txNew, nIn, sigdata);
        }

        nIn++;
      }
    }

    wtxNew.SetTx(MakeTransactionRef(std::move(txNew)));

    if (GetTransactionWeight(*wtxNew.tx) >= MAX_STANDARD_TX_WEIGHT) {
      strFailReason = _("Transaction too large");
      return false;
    }
  }

  if (gArgs.GetBoolArg("-walletrejectlongchains",
                       DEFAULT_WALLET_REJECT_LONG_CHAINS)) {
    LockPoints lp;
    CTxMemPoolEntry entry(wtxNew.tx, 0, 0, 0, false, 0, lp);
    CTxMemPool::setEntries setAncestors;
    size_t nLimitAncestors =
        gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
    size_t nLimitAncestorSize =
        gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT) * 1000;
    size_t nLimitDescendants =
        gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
    size_t nLimitDescendantSize =
        gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) *
        1000;
    std::string errString;
    if (!mempool.CalculateMemPoolAncestors(
            entry, setAncestors, nLimitAncestors, nLimitAncestorSize,
            nLimitDescendants, nLimitDescendantSize, errString)) {
      strFailReason = _("Transaction has too long of a mempool chain");
      return false;
    }
  }

  LogPrintf(
      "Fee Calculation: Fee:%d Bytes:%u Needed:%d Tgt:%d (requested %d) "
      "Reason:\"%s\" Decay %.5f: Estimation: (%g - %g) %.2f%% %.1f/(%.1f %d "
      "mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out)\n",
      nFeeRet, nBytes, nFeeNeeded, feeCalc.returnedTarget,
      feeCalc.desiredTarget, StringForFeeReason(feeCalc.reason),
      feeCalc.est.decay, feeCalc.est.pass.start, feeCalc.est.pass.end,
      100 * feeCalc.est.pass.withinTarget /
          (feeCalc.est.pass.totalConfirmed + feeCalc.est.pass.inMempool +
           feeCalc.est.pass.leftMempool),
      feeCalc.est.pass.withinTarget, feeCalc.est.pass.totalConfirmed,
      feeCalc.est.pass.inMempool, feeCalc.est.pass.leftMempool,
      feeCalc.est.fail.start, feeCalc.est.fail.end,
      100 * feeCalc.est.fail.withinTarget /
          (feeCalc.est.fail.totalConfirmed + feeCalc.est.fail.inMempool +
           feeCalc.est.fail.leftMempool),
      feeCalc.est.fail.withinTarget, feeCalc.est.fail.totalConfirmed,
      feeCalc.est.fail.inMempool, feeCalc.est.fail.leftMempool);
  return true;
}

bool CWallet::CommitTransaction(CWalletTx &wtxNew, CReserveKey &reservekey,
                                CConnman *connman, CValidationState &state) {
  {
    LOCK2(cs_main, cs_wallet);
    LogPrintf("CommitTransaction:\n%s", wtxNew.tx->ToString());
    {
      reservekey.KeepKey();

      AddToWallet(wtxNew);

      for (const CTxIn &txin : wtxNew.tx->vin) {
        CWalletTx &coin = mapWallet[txin.prevout.hash];
        coin.BindWallet(this);
        NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
      }
    }

    mapRequestCount[wtxNew.GetHash()] = 0;

    CWalletTx &wtx = mapWallet[wtxNew.GetHash()];

    if (fBroadcastTransactions) {
      if (!wtx.AcceptToMemoryPool(maxTxFee, state)) {
        LogPrintf("CommitTransaction(): Transaction cannot be broadcast "
                  "immediately, %s\n",
                  state.GetRejectReason());

      } else {
        wtx.RelayWalletTransaction(connman);
      }
    }
  }
  return true;
}

void CWallet::ListAccountCreditDebit(const std::string &strAccount,
                                     std::list<CAccountingEntry> &entries) {
  CWalletDB walletdb(*dbw);
  return walletdb.ListAccountCreditDebit(strAccount, entries);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry &acentry) {
  CWalletDB walletdb(*dbw);

  return AddAccountingEntry(acentry, &walletdb);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry &acentry,
                                 CWalletDB *pwalletdb) {
  if (!pwalletdb->WriteAccountingEntry(++nAccountingEntryNumber, acentry)) {
    return false;
  }

  laccentries.push_back(acentry);
  CAccountingEntry &entry = laccentries.back();
  wtxOrdered.insert(std::make_pair(entry.nOrderPos, TxPair(nullptr, &entry)));

  return true;
}

DBErrors CWallet::LoadWallet(bool &fFirstRunRet) {
  LOCK2(cs_main, cs_wallet);

  fFirstRunRet = false;
  DBErrors nLoadWalletRet = CWalletDB(*dbw, "cr+").LoadWallet(this);
  if (nLoadWalletRet == DB_NEED_REWRITE) {
    if (dbw->Rewrite("\x04pool")) {
      setInternalKeyPool.clear();
      setExternalKeyPool.clear();
      m_pool_key_to_index.clear();
    }
  }

  fFirstRunRet = mapKeys.empty() && mapCryptedKeys.empty() &&
                 mapWatchKeys.empty() && setWatchOnly.empty() &&
                 mapScripts.empty();

  if (nLoadWalletRet != DB_LOAD_OK)
    return nLoadWalletRet;

  uiInterface.LoadWallet(this);

  return DB_LOAD_OK;
}

DBErrors CWallet::ZapSelectTx(std::vector<uint256> &vHashIn,
                              std::vector<uint256> &vHashOut) {
  AssertLockHeld(cs_wallet);

  DBErrors nZapSelectTxRet =
      CWalletDB(*dbw, "cr+").ZapSelectTx(vHashIn, vHashOut);
  for (uint256 hash : vHashOut)
    mapWallet.erase(hash);

  if (nZapSelectTxRet == DB_NEED_REWRITE) {
    if (dbw->Rewrite("\x04pool")) {
      setInternalKeyPool.clear();
      setExternalKeyPool.clear();
      m_pool_key_to_index.clear();
    }
  }

  if (nZapSelectTxRet != DB_LOAD_OK)
    return nZapSelectTxRet;

  MarkDirty();

  return DB_LOAD_OK;
}

DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx> &vWtx) {
  DBErrors nZapWalletTxRet = CWalletDB(*dbw, "cr+").ZapWalletTx(vWtx);
  if (nZapWalletTxRet == DB_NEED_REWRITE) {
    if (dbw->Rewrite("\x04pool")) {
      LOCK(cs_wallet);
      setInternalKeyPool.clear();
      setExternalKeyPool.clear();
      m_pool_key_to_index.clear();
    }
  }

  if (nZapWalletTxRet != DB_LOAD_OK)
    return nZapWalletTxRet;

  return DB_LOAD_OK;
}

bool CWallet::SetAddressBook(const CTxDestination &address,
                             const std::string &strName,
                             const std::string &strPurpose) {
  bool fUpdated = false;
  {
    LOCK(cs_wallet);

    std::map<CTxDestination, CAddressBookData>::iterator mi =
        mapAddressBook.find(address);
    fUpdated = mi != mapAddressBook.end();
    mapAddressBook[address].name = strName;
    if (!strPurpose.empty())

      mapAddressBook[address].purpose = strPurpose;
  }
  NotifyAddressBookChanged(this, address, strName,
                           ::IsMine(*this, address) != ISMINE_NO, strPurpose,
                           (fUpdated ? CT_UPDATED : CT_NEW));
  if (!strPurpose.empty() &&
      !CWalletDB(*dbw).WritePurpose(EncodeDestination(address), strPurpose))
    return false;
  return CWalletDB(*dbw).WriteName(EncodeDestination(address), strName);
}

bool CWallet::DelAddressBook(const CTxDestination &address) {
  {
    LOCK(cs_wallet);

    std::string strAddress = EncodeDestination(address);
    for (const std::pair<std::string, std::string> &item :
         mapAddressBook[address].destdata) {
      CWalletDB(*dbw).EraseDestData(strAddress, item.first);
    }
    mapAddressBook.erase(address);
  }

  NotifyAddressBookChanged(
      this, address, "", ::IsMine(*this, address) != ISMINE_NO, "", CT_DELETED);

  CWalletDB(*dbw).ErasePurpose(EncodeDestination(address));
  return CWalletDB(*dbw).EraseName(EncodeDestination(address));
}

const std::string &CWallet::GetAccountName(const CScript &scriptPubKey) const {
  CTxDestination address;
  if (ExtractDestination(scriptPubKey, address) &&
      !scriptPubKey.IsUnspendable()) {
    auto mi = mapAddressBook.find(address);
    if (mi != mapAddressBook.end()) {
      return mi->second.name;
    }
  }

  const static std::string DEFAULT_ACCOUNT_NAME;
  return DEFAULT_ACCOUNT_NAME;
}

bool CWallet::NewKeyPool() {
  {
    LOCK(cs_wallet);
    CWalletDB walletdb(*dbw);

    for (int64_t nIndex : setInternalKeyPool) {
      walletdb.ErasePool(nIndex);
    }
    setInternalKeyPool.clear();

    for (int64_t nIndex : setExternalKeyPool) {
      walletdb.ErasePool(nIndex);
    }
    setExternalKeyPool.clear();

    m_pool_key_to_index.clear();

    if (!TopUpKeyPool()) {
      return false;
    }
    LogPrintf("CWallet::NewKeyPool rewrote keypool\n");
  }
  return true;
}

size_t CWallet::KeypoolCountExternalKeys() {
  AssertLockHeld(cs_wallet);

  return setExternalKeyPool.size();
}

void CWallet::LoadKeyPool(int64_t nIndex, const CKeyPool &keypool) {
  AssertLockHeld(cs_wallet);
  if (keypool.fInternal) {
    setInternalKeyPool.insert(nIndex);
  } else {
    setExternalKeyPool.insert(nIndex);
  }
  m_max_keypool_index = std::max(m_max_keypool_index, nIndex);
  m_pool_key_to_index[keypool.vchPubKey.GetID()] = nIndex;

  CKeyID keyid = keypool.vchPubKey.GetID();
  if (mapKeyMetadata.count(keyid) == 0)
    mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
}

bool CWallet::TopUpKeyPool(unsigned int kpSize) {
  {
    LOCK(cs_wallet);

    if (IsLocked())
      return false;

    unsigned int nTargetSize;
    if (kpSize > 0)
      nTargetSize = kpSize;
    else
      nTargetSize =
          std::max(gArgs.GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t)0);

    int64_t missingExternal =
        std::max(std::max((int64_t)nTargetSize, (int64_t)1) -
                     (int64_t)setExternalKeyPool.size(),
                 (int64_t)0);
    int64_t missingInternal =
        std::max(std::max((int64_t)nTargetSize, (int64_t)1) -
                     (int64_t)setInternalKeyPool.size(),
                 (int64_t)0);

    if (!IsHDEnabled() || !CanSupportFeature(FEATURE_HD_SPLIT)) {
      missingInternal = 0;
    }
    bool internal = false;
    CWalletDB walletdb(*dbw);
    for (int64_t i = missingInternal + missingExternal; i--;) {
      if (i < missingInternal) {
        internal = true;
      }

      assert(m_max_keypool_index < std::numeric_limits<int64_t>::max());

      int64_t index = ++m_max_keypool_index;

      CPubKey pubkey(GenerateNewKey(walletdb, internal));
      if (!walletdb.WritePool(index, CKeyPool(pubkey, internal))) {
        throw std::runtime_error(std::string(__func__) +
                                 ": writing generated key failed");
      }

      if (internal) {
        setInternalKeyPool.insert(index);
      } else {
        setExternalKeyPool.insert(index);
      }
      m_pool_key_to_index[pubkey.GetID()] = index;
    }
    if (missingInternal + missingExternal > 0) {
      LogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n",
                missingInternal + missingExternal, missingInternal,
                setInternalKeyPool.size() + setExternalKeyPool.size(),
                setInternalKeyPool.size());
    }
  }
  return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t &nIndex, CKeyPool &keypool,
                                    bool fRequestedInternal) {
  nIndex = -1;
  keypool.vchPubKey = CPubKey();
  {
    LOCK(cs_wallet);

    if (!IsLocked())
      TopUpKeyPool();

    bool fReturningInternal = IsHDEnabled() &&
                              CanSupportFeature(FEATURE_HD_SPLIT) &&
                              fRequestedInternal;
    std::set<int64_t> &setKeyPool =
        fReturningInternal ? setInternalKeyPool : setExternalKeyPool;

    if (setKeyPool.empty())
      return;

    CWalletDB walletdb(*dbw);

    auto it = setKeyPool.begin();
    nIndex = *it;
    setKeyPool.erase(it);
    if (!walletdb.ReadPool(nIndex, keypool)) {
      throw std::runtime_error(std::string(__func__) + ": read failed");
    }
    if (!HaveKey(keypool.vchPubKey.GetID())) {
      throw std::runtime_error(std::string(__func__) +
                               ": unknown key in key pool");
    }
    if (keypool.fInternal != fReturningInternal) {
      throw std::runtime_error(std::string(__func__) +
                               ": keypool entry misclassified");
    }

    assert(keypool.vchPubKey.IsValid());
    m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
    LogPrintf("keypool reserve %d\n", nIndex);
  }
}

void CWallet::KeepKey(int64_t nIndex) {
  CWalletDB walletdb(*dbw);
  walletdb.ErasePool(nIndex);
  LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex, bool fInternal, const CPubKey &pubkey) {
  {
    LOCK(cs_wallet);
    if (fInternal) {
      setInternalKeyPool.insert(nIndex);
    } else {
      setExternalKeyPool.insert(nIndex);
    }
    m_pool_key_to_index[pubkey.GetID()] = nIndex;
  }
  LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey &result, bool internal) {
  CKeyPool keypool;
  {
    LOCK(cs_wallet);
    int64_t nIndex = 0;
    ReserveKeyFromKeyPool(nIndex, keypool, internal);
    if (nIndex == -1) {
      if (IsLocked())
        return false;
      CWalletDB walletdb(*dbw);
      result = GenerateNewKey(walletdb, internal);
      return true;
    }
    KeepKey(nIndex);
    result = keypool.vchPubKey;
  }
  return true;
}

static int64_t GetOldestKeyTimeInPool(const std::set<int64_t> &setKeyPool,
                                      CWalletDB &walletdb) {
  if (setKeyPool.empty()) {
    return GetTime();
  }

  CKeyPool keypool;
  int64_t nIndex = *(setKeyPool.begin());
  if (!walletdb.ReadPool(nIndex, keypool)) {
    throw std::runtime_error(std::string(__func__) +
                             ": read oldest key in keypool failed");
  }
  assert(keypool.vchPubKey.IsValid());
  return keypool.nTime;
}

int64_t CWallet::GetOldestKeyPoolTime() {
  LOCK(cs_wallet);

  CWalletDB walletdb(*dbw);

  int64_t oldestKey = GetOldestKeyTimeInPool(setExternalKeyPool, walletdb);
  if (IsHDEnabled() && CanSupportFeature(FEATURE_HD_SPLIT)) {
    oldestKey = std::max(GetOldestKeyTimeInPool(setInternalKeyPool, walletdb),
                         oldestKey);
  }

  return oldestKey;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances() {
  std::map<CTxDestination, CAmount> balances;

  {
    LOCK(cs_wallet);
    for (const auto &walletEntry : mapWallet) {
      const CWalletTx *pcoin = &walletEntry.second;

      if (!pcoin->IsTrusted())
        continue;

      if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
        continue;

      int nDepth = pcoin->GetDepthInMainChain();
      if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
        continue;

      for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
        CTxDestination addr;
        if (!IsMine(pcoin->tx->vout[i]))
          continue;
        if (!ExtractDestination(pcoin->tx->vout[i].scriptPubKey, addr))
          continue;

        CAmount n =
            IsSpent(walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

        if (!balances.count(addr))
          balances[addr] = 0;
        balances[addr] += n;
      }
    }
  }

  return balances;
}

std::set<std::set<CTxDestination>> CWallet::GetAddressGroupings() {
  AssertLockHeld(cs_wallet);

  std::set<std::set<CTxDestination>> groupings;
  std::set<CTxDestination> grouping;

  for (const auto &walletEntry : mapWallet) {
    const CWalletTx *pcoin = &walletEntry.second;

    if (pcoin->tx->vin.size() > 0) {
      bool any_mine = false;

      for (CTxIn txin : pcoin->tx->vin) {
        CTxDestination address;
        if (!IsMine(txin))

          continue;
        if (!ExtractDestination(mapWallet[txin.prevout.hash]
                                    .tx->vout[txin.prevout.n]
                                    .scriptPubKey,
                                address))
          continue;
        grouping.insert(address);
        any_mine = true;
      }

      if (any_mine) {
        for (CTxOut txout : pcoin->tx->vout)
          if (IsChange(txout)) {
            CTxDestination txoutAddr;
            if (!ExtractDestination(txout.scriptPubKey, txoutAddr))
              continue;
            grouping.insert(txoutAddr);
          }
      }
      if (grouping.size() > 0) {
        groupings.insert(grouping);
        grouping.clear();
      }
    }

    for (const auto &txout : pcoin->tx->vout)
      if (IsMine(txout)) {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
          continue;
        grouping.insert(address);
        groupings.insert(grouping);
        grouping.clear();
      }
  }

  std::set<std::set<CTxDestination> *> uniqueGroupings;

  std::map<CTxDestination, std::set<CTxDestination> *> setmap;

  for (std::set<CTxDestination> _grouping : groupings) {
    std::set<std::set<CTxDestination> *> hits;
    std::map<CTxDestination, std::set<CTxDestination> *>::iterator it;
    for (CTxDestination address : _grouping)
      if ((it = setmap.find(address)) != setmap.end())
        hits.insert((*it).second);

    std::set<CTxDestination> *merged = new std::set<CTxDestination>(_grouping);
    for (std::set<CTxDestination> *hit : hits) {
      merged->insert(hit->begin(), hit->end());
      uniqueGroupings.erase(hit);
      delete hit;
    }
    uniqueGroupings.insert(merged);

    for (CTxDestination element : *merged)
      setmap[element] = merged;
  }

  std::set<std::set<CTxDestination>> ret;
  for (std::set<CTxDestination> *uniqueGrouping : uniqueGroupings) {
    ret.insert(*uniqueGrouping);
    delete uniqueGrouping;
  }

  return ret;
}

std::set<CTxDestination>
CWallet::GetAccountAddresses(const std::string &strAccount) const {
  LOCK(cs_wallet);
  std::set<CTxDestination> result;
  for (const std::pair<CTxDestination, CAddressBookData> &item :
       mapAddressBook) {
    const CTxDestination &address = item.first;
    const std::string &strName = item.second.name;
    if (strName == strAccount)
      result.insert(address);
  }
  return result;
}

bool CReserveKey::GetReservedKey(CPubKey &pubkey, bool internal) {
  if (nIndex == -1) {
    CKeyPool keypool;
    pwallet->ReserveKeyFromKeyPool(nIndex, keypool, internal);
    if (nIndex != -1)
      vchPubKey = keypool.vchPubKey;
    else {
      return false;
    }
    fInternal = keypool.fInternal;
  }
  assert(vchPubKey.IsValid());
  pubkey = vchPubKey;
  return true;
}

void CReserveKey::KeepKey() {
  if (nIndex != -1)
    pwallet->KeepKey(nIndex);
  nIndex = -1;
  vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey() {
  if (nIndex != -1) {
    pwallet->ReturnKey(nIndex, fInternal, vchPubKey);
  }
  nIndex = -1;
  vchPubKey = CPubKey();
}

void CWallet::MarkReserveKeysAsUsed(int64_t keypool_id) {
  AssertLockHeld(cs_wallet);
  bool internal = setInternalKeyPool.count(keypool_id);
  if (!internal)
    assert(setExternalKeyPool.count(keypool_id));
  std::set<int64_t> *setKeyPool =
      internal ? &setInternalKeyPool : &setExternalKeyPool;
  auto it = setKeyPool->begin();

  CWalletDB walletdb(*dbw);
  while (it != std::end(*setKeyPool)) {
    const int64_t &index = *(it);
    if (index > keypool_id)
      break;

    CKeyPool keypool;
    if (walletdb.ReadPool(index, keypool)) {
      m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
    }
    LearnAllRelatedScripts(keypool.vchPubKey);
    walletdb.ErasePool(index);
    LogPrintf("keypool index %d removed\n", index);
    it = setKeyPool->erase(it);
  }
}

void CWallet::GetScriptForMining(std::shared_ptr<CReserveScript> &script) {
  std::shared_ptr<CReserveKey> rKey = std::make_shared<CReserveKey>(this);
  CPubKey pubkey;
  if (!rKey->GetReservedKey(pubkey))
    return;

  script = rKey;
  script->reserveScript = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
}

void CWallet::LockCoin(const COutPoint &output) {
  AssertLockHeld(cs_wallet);

  setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(const COutPoint &output) {
  AssertLockHeld(cs_wallet);

  setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins() {
  AssertLockHeld(cs_wallet);

  setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const {
  AssertLockHeld(cs_wallet);

  COutPoint outpt(hash, n);

  return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint> &vOutpts) const {
  AssertLockHeld(cs_wallet);

  for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
       it != setLockedCoins.end(); it++) {
    COutPoint outpt = (*it);
    vOutpts.push_back(outpt);
  }
}

void CWallet::GetKeyBirthTimes(
    std::map<CTxDestination, int64_t> &mapKeyBirth) const {
  AssertLockHeld(cs_wallet);

  mapKeyBirth.clear();

  for (const auto &entry : mapKeyMetadata) {
    if (entry.second.nCreateTime) {
      mapKeyBirth[entry.first] = entry.second.nCreateTime;
    }
  }

  CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)];

  std::map<CKeyID, CBlockIndex *> mapKeyFirstBlock;
  for (const CKeyID &keyid : GetKeys()) {
    if (mapKeyBirth.count(keyid) == 0)
      mapKeyFirstBlock[keyid] = pindexMax;
  }

  if (mapKeyFirstBlock.empty())
    return;

  std::vector<CKeyID> vAffected;
  for (const auto &entry : mapWallet) {
    const CWalletTx &wtx = entry.second;
    BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
    if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {
      int nHeight = blit->second->nHeight;
      for (const CTxOut &txout : wtx.tx->vout) {
        CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
        for (const CKeyID &keyid : vAffected) {
          std::map<CKeyID, CBlockIndex *>::iterator rit =
              mapKeyFirstBlock.find(keyid);
          if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
            rit->second = blit->second;
        }
        vAffected.clear();
      }
    }
  }

  for (const auto &entry : mapKeyFirstBlock)
    mapKeyBirth[entry.first] = entry.second->GetBlockTime() - TIMESTAMP_WINDOW;
}

unsigned int CWallet::ComputeTimeSmart(const CWalletTx &wtx) const {
  unsigned int nTimeSmart = wtx.nTimeReceived;
  if (!wtx.hashUnset()) {
    if (mapBlockIndex.count(wtx.hashBlock)) {
      int64_t latestNow = wtx.nTimeReceived;
      int64_t latestEntry = 0;

      int64_t latestTolerated = latestNow + 300;
      const TxItems &txOrdered = wtxOrdered;
      for (auto it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx *const pwtx = it->second.first;
        if (pwtx == &wtx) {
          continue;
        }
        CAccountingEntry *const pacentry = it->second.second;
        int64_t nSmartTime;
        if (pwtx) {
          nSmartTime = pwtx->nTimeSmart;
          if (!nSmartTime) {
            nSmartTime = pwtx->nTimeReceived;
          }
        } else {
          nSmartTime = pacentry->nTime;
        }
        if (nSmartTime <= latestTolerated) {
          latestEntry = nSmartTime;
          if (nSmartTime > latestNow) {
            latestNow = nSmartTime;
          }
          break;
        }
      }

      int64_t blocktime = mapBlockIndex[wtx.hashBlock]->GetBlockTime();
      nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
    } else {
      LogPrintf("%s: found %s in block %s not in index\n", __func__,
                wtx.GetHash().ToString(), wtx.hashBlock.ToString());
    }
  }
  return nTimeSmart;
}

bool CWallet::AddDestData(const CTxDestination &dest, const std::string &key,
                          const std::string &value) {
  if (boost::get<CNoDestination>(&dest))
    return false;

  mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
  return CWalletDB(*dbw).WriteDestData(EncodeDestination(dest), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest,
                            const std::string &key) {
  if (!mapAddressBook[dest].destdata.erase(key))
    return false;
  return CWalletDB(*dbw).EraseDestData(EncodeDestination(dest), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key,
                           const std::string &value) {
  mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
  return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key,
                          std::string *value) const {
  std::map<CTxDestination, CAddressBookData>::const_iterator i =
      mapAddressBook.find(dest);
  if (i != mapAddressBook.end()) {
    CAddressBookData::StringMap::const_iterator j =
        i->second.destdata.find(key);
    if (j != i->second.destdata.end()) {
      if (value)
        *value = j->second;
      return true;
    }
  }
  return false;
}

std::vector<std::string>
CWallet::GetDestValues(const std::string &prefix) const {
  LOCK(cs_wallet);
  std::vector<std::string> values;
  for (const auto &address : mapAddressBook) {
    for (const auto &data : address.second.destdata) {
      if (!data.first.compare(0, prefix.size(), prefix)) {
        values.emplace_back(data.second);
      }
    }
  }
  return values;
}

CWallet *CWallet::CreateWalletFromFile(const std::string walletFile) {
  std::vector<CWalletTx> vWtx;

  if (gArgs.GetBoolArg("-zapwallettxes", false)) {
    uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

    std::unique_ptr<CWalletDBWrapper> dbw(
        new CWalletDBWrapper(&bitdb, walletFile));
    std::unique_ptr<CWallet> tempWallet = MakeUnique<CWallet>(std::move(dbw));
    DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
    if (nZapWalletRet != DB_LOAD_OK) {
      InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
      return nullptr;
    }
  }

  uiInterface.InitMessage(_("Loading wallet..."));

  int64_t nStart = GetTimeMillis();
  bool fFirstRun = true;
  std::unique_ptr<CWalletDBWrapper> dbw(
      new CWalletDBWrapper(&bitdb, walletFile));
  CWallet *walletInstance = new CWallet(std::move(dbw));
  DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
  if (nLoadWalletRet != DB_LOAD_OK) {
    if (nLoadWalletRet == DB_CORRUPT) {
      InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
      return nullptr;
    } else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
      InitWarning(strprintf(
          _("Error reading %s! All keys read correctly, but transaction data"
            " or address book entries might be missing or incorrect."),
          walletFile));
    } else if (nLoadWalletRet == DB_TOO_NEW) {
      InitError(
          strprintf(_("Error loading %s: Wallet requires newer version of %s"),
                    walletFile, _(PACKAGE_NAME)));
      return nullptr;
    } else if (nLoadWalletRet == DB_NEED_REWRITE) {
      InitError(
          strprintf(_("Wallet needed to be rewritten: restart %s to complete"),
                    _(PACKAGE_NAME)));
      return nullptr;
    } else {
      InitError(strprintf(_("Error loading %s"), walletFile));
      return nullptr;
    }
  }

  if (gArgs.GetBoolArg("-upgradewallet", fFirstRun)) {
    int nMaxVersion = gArgs.GetArg("-upgradewallet", 0);
    if (nMaxVersion == 0)

    {
      LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
      nMaxVersion = CLIENT_VERSION;
      walletInstance->SetMinVersion(FEATURE_LATEST);

    } else
      LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
    if (nMaxVersion < walletInstance->GetVersion()) {
      InitError(_("Cannot downgrade wallet"));
      return nullptr;
    }
    walletInstance->SetMaxVersion(nMaxVersion);
  }

  if (fFirstRun) {
    if (!gArgs.GetBoolArg("-usehd", true)) {
      InitError(strprintf(_("Error creating %s: You can't create non-HD "
                            "wallets with this version."),
                          walletFile));
      return nullptr;
    }
    walletInstance->SetMinVersion(FEATURE_NO_DEFAULT_KEY);

    CPubKey masterPubKey = walletInstance->GenerateNewHDMasterKey();
    if (!walletInstance->SetHDMasterKey(masterPubKey))
      throw std::runtime_error(std::string(__func__) +
                               ": Storing master key failed");

    if (!walletInstance->TopUpKeyPool()) {
      InitError(_("Unable to generate initial keys") += "\n");
      return nullptr;
    }

    walletInstance->SetBestChain(chainActive.GetLocator());
  } else if (gArgs.IsArgSet("-usehd")) {
    bool useHD = gArgs.GetBoolArg("-usehd", true);
    if (walletInstance->IsHDEnabled() && !useHD) {
      InitError(strprintf(_("Error loading %s: You can't disable HD on an "
                            "already existing HD wallet"),
                          walletFile));
      return nullptr;
    }
    if (!walletInstance->IsHDEnabled() && useHD) {
      InitError(strprintf(_("Error loading %s: You can't enable HD on an "
                            "already existing non-HD wallet"),
                          walletFile));
      return nullptr;
    }
  }

  LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

  walletInstance->TopUpKeyPool();

  CBlockIndex *pindexRescan = chainActive.Genesis();
  if (!gArgs.GetBoolArg("-rescan", false)) {
    CWalletDB walletdb(*walletInstance->dbw);
    CBlockLocator locator;
    if (walletdb.ReadBestBlock(locator))
      pindexRescan = FindForkInGlobalIndex(chainActive, locator);
  }

  walletInstance->m_last_block_processed = chainActive.Tip();
  RegisterValidationInterface(walletInstance);

  if (chainActive.Tip() && chainActive.Tip() != pindexRescan) {
    if (fPruneMode) {
      CBlockIndex *block = chainActive.Tip();
      while (block && block->pprev &&
             (block->pprev->nStatus & BLOCK_HAVE_DATA) &&
             block->pprev->nTx > 0 && pindexRescan != block)
        block = block->pprev;

      if (pindexRescan != block) {
        InitError(_("Prune: last wallet synchronisation goes beyond pruned "
                    "data. You need to -reindex (download the whole blockchain "
                    "again in case of pruned node)"));
        return nullptr;
      }
    }

    uiInterface.InitMessage(_("Rescanning..."));
    LogPrintf("Rescanning last %i blocks (from block %i)...\n",
              chainActive.Height() - pindexRescan->nHeight,
              pindexRescan->nHeight);

    while (pindexRescan && walletInstance->nTimeFirstKey &&
           (pindexRescan->GetBlockTime() <
            (walletInstance->nTimeFirstKey - TIMESTAMP_WINDOW))) {
      pindexRescan = chainActive.Next(pindexRescan);
    }

    nStart = GetTimeMillis();
    {
      WalletRescanReserver reserver(walletInstance);
      if (!reserver.reserve()) {
        InitError(_("Failed to rescan the wallet during initialization"));
        return nullptr;
      }
      walletInstance->ScanForWalletTransactions(pindexRescan, nullptr, reserver,
                                                true);
    }
    LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
    walletInstance->SetBestChain(chainActive.GetLocator());
    walletInstance->dbw->IncrementUpdateCounter();

    if (gArgs.GetBoolArg("-zapwallettxes", false) &&
        gArgs.GetArg("-zapwallettxes", "1") != "2") {
      CWalletDB walletdb(*walletInstance->dbw);

      for (const CWalletTx &wtxOld : vWtx) {
        uint256 hash = wtxOld.GetHash();
        std::map<uint256, CWalletTx>::iterator mi =
            walletInstance->mapWallet.find(hash);
        if (mi != walletInstance->mapWallet.end()) {
          const CWalletTx *copyFrom = &wtxOld;
          CWalletTx *copyTo = &mi->second;
          copyTo->mapValue = copyFrom->mapValue;
          copyTo->vOrderForm = copyFrom->vOrderForm;
          copyTo->nTimeReceived = copyFrom->nTimeReceived;
          copyTo->nTimeSmart = copyFrom->nTimeSmart;
          copyTo->fFromMe = copyFrom->fFromMe;
          copyTo->strFromAccount = copyFrom->strFromAccount;
          copyTo->nOrderPos = copyFrom->nOrderPos;
          walletdb.WriteTx(*copyTo);
        }
      }
    }
  }
  walletInstance->SetBroadcastTransactions(
      gArgs.GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

  {
    LOCK(walletInstance->cs_wallet);
    LogPrintf("setKeyPool.size() = %u\n", walletInstance->GetKeyPoolSize());
    LogPrintf("mapWallet.size() = %u\n", walletInstance->mapWallet.size());
    LogPrintf("mapAddressBook.size() = %u\n",
              walletInstance->mapAddressBook.size());
  }

  return walletInstance;
}

std::atomic<bool> CWallet::fFlushScheduled(false);

void CWallet::postInitProcess(CScheduler &scheduler) {
  ReacceptWalletTransactions();

  if (!CWallet::fFlushScheduled.exchange(true)) {
    scheduler.scheduleEvery(MaybeCompactWalletDB, 500);
  }
}

bool CWallet::BackupWallet(const std::string &strDest) {
  return dbw->Backup(strDest);
}

CKeyPool::CKeyPool() {
  nTime = GetTime();
  fInternal = false;
}

CKeyPool::CKeyPool(const CPubKey &vchPubKeyIn, bool internalIn) {
  nTime = GetTime();
  vchPubKey = vchPubKeyIn;
  fInternal = internalIn;
}

CWalletKey::CWalletKey(int64_t nExpires) {
  nTimeCreated = (nExpires ? GetTime() : 0);
  nTimeExpires = nExpires;
}

void CMerkleTx::SetMerkleBranch(const CBlockIndex *pindex, int posInBlock) {
  hashBlock = pindex->GetBlockHash();

  nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex *&pindexRet) const {
  if (hashUnset())
    return 0;

  AssertLockHeld(cs_main);

  BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
  if (mi == mapBlockIndex.end())
    return 0;
  CBlockIndex *pindex = (*mi).second;
  if (!pindex || !chainActive.Contains(pindex))
    return 0;

  pindexRet = pindex;
  return ((nIndex == -1) ? (-1) : 1) *
         (chainActive.Height() - pindex->nHeight + 1);
}

int CMerkleTx::GetBlocksToMaturity() const {
  if (!IsCoinBase())
    return 0;
  return std::max(0, (COINBASE_MATURITY + 1) - GetDepthInMainChain());
}

bool CWalletTx::AcceptToMemoryPool(const CAmount &nAbsurdFee,
                                   CValidationState &state) {
  if (mempool.exists(tx->GetHash())) {
    return false;
  }

  bool ret = ::AcceptToMemoryPool(mempool, state, tx, nullptr, nullptr, false,
                                  nAbsurdFee);
  fInMempool = ret;
  return ret;
}

static const std::string OUTPUT_TYPE_STRING_LEGACY = "legacy";
static const std::string OUTPUT_TYPE_STRING_P2SH_SEGWIT = "p2sh-segwit";
static const std::string OUTPUT_TYPE_STRING_BECH32 = "bech32";

OutputType ParseOutputType(const std::string &type, OutputType default_type) {
  if (type.empty()) {
    return default_type;
  } else if (type == OUTPUT_TYPE_STRING_LEGACY) {
    return OUTPUT_TYPE_LEGACY;
  } else if (type == OUTPUT_TYPE_STRING_P2SH_SEGWIT) {
    return OUTPUT_TYPE_P2SH_SEGWIT;
  } else if (type == OUTPUT_TYPE_STRING_BECH32) {
    return OUTPUT_TYPE_BECH32;
  } else {
    return OUTPUT_TYPE_NONE;
  }
}

const std::string &FormatOutputType(OutputType type) {
  switch (type) {
  case OUTPUT_TYPE_LEGACY:
    return OUTPUT_TYPE_STRING_LEGACY;
  case OUTPUT_TYPE_P2SH_SEGWIT:
    return OUTPUT_TYPE_STRING_P2SH_SEGWIT;
  case OUTPUT_TYPE_BECH32:
    return OUTPUT_TYPE_STRING_BECH32;
  default:
    assert(false);
  }
}

void CWallet::LearnRelatedScripts(const CPubKey &key, OutputType type) {
  if (key.IsCompressed() &&
      (type == OUTPUT_TYPE_P2SH_SEGWIT || type == OUTPUT_TYPE_BECH32)) {
    CTxDestination witdest = WitnessV0KeyHash(key.GetID());
    CScript witprog = GetScriptForDestination(witdest);

    assert(IsSolvable(*this, witprog));
    AddCScript(witprog);
  }
}

void CWallet::LearnAllRelatedScripts(const CPubKey &key) {
  LearnRelatedScripts(key, OUTPUT_TYPE_P2SH_SEGWIT);
}

CTxDestination GetDestinationForKey(const CPubKey &key, OutputType type) {
  switch (type) {
  case OUTPUT_TYPE_LEGACY:
    return key.GetID();
  case OUTPUT_TYPE_P2SH_SEGWIT:
  case OUTPUT_TYPE_BECH32: {
    if (!key.IsCompressed())
      return key.GetID();
    CTxDestination witdest = WitnessV0KeyHash(key.GetID());
    CScript witprog = GetScriptForDestination(witdest);
    if (type == OUTPUT_TYPE_P2SH_SEGWIT) {
      return CScriptID(witprog);
    } else {
      return witdest;
    }
  }
  default:
    assert(false);
  }
}

std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey &key) {
  CKeyID keyid = key.GetID();
  if (key.IsCompressed()) {
    CTxDestination segwit = WitnessV0KeyHash(keyid);
    CTxDestination p2sh = CScriptID(GetScriptForDestination(segwit));
    return std::vector<CTxDestination>{std::move(keyid), std::move(p2sh),
                                       std::move(segwit)};
  } else {
    return std::vector<CTxDestination>{std::move(keyid)};
  }
}

CTxDestination CWallet::AddAndGetDestinationForScript(const CScript &script,
                                                      OutputType type) {
  switch (type) {
  case OUTPUT_TYPE_LEGACY:
    return CScriptID(script);
  case OUTPUT_TYPE_P2SH_SEGWIT:
  case OUTPUT_TYPE_BECH32: {
    WitnessV0ScriptHash hash;
    CSHA256().Write(script.data(), script.size()).Finalize(hash.begin());
    CTxDestination witdest = hash;
    CScript witprog = GetScriptForDestination(witdest);

    if (!IsSolvable(*this, witprog))
      return CScriptID(script);

    AddCScript(witprog);
    if (type == OUTPUT_TYPE_BECH32) {
      return witdest;
    } else {
      return CScriptID(witprog);
    }
  }
  default:
    assert(false);
  }
}
