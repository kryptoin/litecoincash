// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <consensus/validation.h>
#include <rpc/server.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

extern UniValue importmulti(const JSONRPCRequest &request);
extern UniValue dumpwallet(const JSONRPCRequest &request);
extern UniValue importwallet(const JSONRPCRequest &request);

#define RUN_TESTS 100

#define RANDOM_REPEATS 5

std::vector<std::unique_ptr<CWalletTx>> wtxn;

typedef std::set<CInputCoin> CoinSet;

BOOST_FIXTURE_TEST_SUITE(wallet_tests, WalletTestingSetup)

static const CWallet testWallet;
static std::vector<COutput> vCoins;

static void add_coin(const CAmount &nValue, int nAge = 6 * 24,
                     bool fIsFromMe = false, int nInput = 0) {
  static int nextLockTime = 0;
  CMutableTransaction tx;
  tx.nLockTime = nextLockTime++;

  tx.vout.resize(nInput + 1);
  tx.vout[nInput].nValue = nValue;
  if (fIsFromMe) {
    tx.vin.resize(1);
  }
  std::unique_ptr<CWalletTx> wtx(
      new CWalletTx(&testWallet, MakeTransactionRef(std::move(tx))));
  if (fIsFromMe) {
    wtx->fDebitCached = true;
    wtx->nDebitCached = 1;
  }
  COutput output(wtx.get(), nInput, nAge, true, true, true);
  vCoins.push_back(output);
  wtxn.emplace_back(std::move(wtx));
}

static void empty_wallet(void) {
  vCoins.clear();
  wtxn.clear();
}

static bool equal_sets(CoinSet a, CoinSet b) {
  std::pair<CoinSet::iterator, CoinSet::iterator> ret =
      mismatch(a.begin(), a.end(), b.begin());
  return ret.first == a.end() && ret.second == b.end();
}

BOOST_AUTO_TEST_CASE(coin_selection_tests) {
  CoinSet setCoinsRet, setCoinsRet2;
  CAmount nValueRet;

  LOCK(testWallet.cs_wallet);

  for (int i = 0; i < RUN_TESTS; i++) {
    empty_wallet();

    BOOST_CHECK(!testWallet.SelectCoinsMinConf(1 * CENT, 1, 6, 0, vCoins,
                                               setCoinsRet, nValueRet));

    add_coin(1 * CENT, 4);

    BOOST_CHECK(!testWallet.SelectCoinsMinConf(1 * CENT, 1, 6, 0, vCoins,
                                               setCoinsRet, nValueRet));

    BOOST_CHECK(testWallet.SelectCoinsMinConf(1 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1 * CENT);

    add_coin(2 * CENT);

    BOOST_CHECK(!testWallet.SelectCoinsMinConf(3 * CENT, 1, 6, 0, vCoins,
                                               setCoinsRet, nValueRet));

    BOOST_CHECK(testWallet.SelectCoinsMinConf(3 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 3 * CENT);

    add_coin(5 * CENT);

    add_coin(10 * CENT, 3, true);

    add_coin(20 * CENT);

    BOOST_CHECK(!testWallet.SelectCoinsMinConf(38 * CENT, 1, 6, 0, vCoins,
                                               setCoinsRet, nValueRet));

    BOOST_CHECK(!testWallet.SelectCoinsMinConf(38 * CENT, 6, 6, 0, vCoins,
                                               setCoinsRet, nValueRet));

    BOOST_CHECK(testWallet.SelectCoinsMinConf(37 * CENT, 1, 6, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 37 * CENT);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(38 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 38 * CENT);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(34 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 35 * CENT);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(7 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 7 * CENT);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(8 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK(nValueRet == 8 * CENT);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(9 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 10 * CENT);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    empty_wallet();

    add_coin(6 * CENT);
    add_coin(7 * CENT);
    add_coin(8 * CENT);
    add_coin(20 * CENT);
    add_coin(30 * CENT);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(71 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK(!testWallet.SelectCoinsMinConf(72 * CENT, 1, 1, 0, vCoins,
                                               setCoinsRet, nValueRet));

    BOOST_CHECK(testWallet.SelectCoinsMinConf(16 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 20 * CENT);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    add_coin(5 * CENT);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(16 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 18 * CENT);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

    add_coin(18 * CENT);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(16 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 18 * CENT);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(11 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 11 * CENT);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

    add_coin(1 * COIN);
    add_coin(2 * COIN);
    add_coin(3 * COIN);
    add_coin(4 * COIN);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(95 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1 * COIN);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(195 * CENT, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 2 * COIN);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    empty_wallet();
    add_coin(MIN_CHANGE * 1 / 10);
    add_coin(MIN_CHANGE * 2 / 10);
    add_coin(MIN_CHANGE * 3 / 10);
    add_coin(MIN_CHANGE * 4 / 10);
    add_coin(MIN_CHANGE * 5 / 10);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE);

    add_coin(1111 * MIN_CHANGE);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE);

    add_coin(MIN_CHANGE * 6 / 10);
    add_coin(MIN_CHANGE * 7 / 10);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE);

    empty_wallet();
    for (int j = 0; j < 20; j++)
      add_coin(50000 * COIN);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(500000 * COIN, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 500000 * COIN);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 10U);

    empty_wallet();
    add_coin(MIN_CHANGE * 5 / 10);
    add_coin(MIN_CHANGE * 6 / 10);
    add_coin(MIN_CHANGE * 7 / 10);
    add_coin(1111 * MIN_CHANGE);
    BOOST_CHECK(testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1111 * MIN_CHANGE);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

    empty_wallet();
    add_coin(MIN_CHANGE * 4 / 10);
    add_coin(MIN_CHANGE * 6 / 10);
    add_coin(MIN_CHANGE * 8 / 10);
    add_coin(1111 * MIN_CHANGE);
    BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE, 1, 1, 0, vCoins,
                                              setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

    empty_wallet();
    add_coin(MIN_CHANGE * 5 / 100);
    add_coin(MIN_CHANGE * 1);
    add_coin(MIN_CHANGE * 100);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE * 10001 / 100, 1, 1, 0,
                                              vCoins, setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE * 10105 / 100);

    BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE * 9990 / 100, 1, 1, 0,
                                              vCoins, setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 101 * MIN_CHANGE);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

    for (CAmount amt = 1500; amt < COIN; amt *= 10) {
      empty_wallet();

      for (uint16_t j = 0; j < 676; j++)
        add_coin(amt);
      BOOST_CHECK(testWallet.SelectCoinsMinConf(2000, 1, 1, 0, vCoins,
                                                setCoinsRet, nValueRet));
      if (amt - 2000 < MIN_CHANGE) {
        uint16_t returnSize = std::ceil((2000.0 + MIN_CHANGE) / amt);
        CAmount returnValue = amt * returnSize;
        BOOST_CHECK_EQUAL(nValueRet, returnValue);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), returnSize);
      } else {
        BOOST_CHECK_EQUAL(nValueRet, amt);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);
      }
    }

    {
      empty_wallet();
      for (int i2 = 0; i2 < 100; i2++)
        add_coin(COIN);

      BOOST_CHECK(testWallet.SelectCoinsMinConf(50 * COIN, 1, 6, 0, vCoins,
                                                setCoinsRet, nValueRet));
      BOOST_CHECK(testWallet.SelectCoinsMinConf(50 * COIN, 1, 6, 0, vCoins,
                                                setCoinsRet2, nValueRet));
      BOOST_CHECK(!equal_sets(setCoinsRet, setCoinsRet2));

      int fails = 0;
      for (int j = 0; j < RANDOM_REPEATS; j++) {
        BOOST_CHECK(testWallet.SelectCoinsMinConf(COIN, 1, 6, 0, vCoins,
                                                  setCoinsRet, nValueRet));
        BOOST_CHECK(testWallet.SelectCoinsMinConf(COIN, 1, 6, 0, vCoins,
                                                  setCoinsRet2, nValueRet));
        if (equal_sets(setCoinsRet, setCoinsRet2))
          fails++;
      }
      BOOST_CHECK_NE(fails, RANDOM_REPEATS);

      add_coin(5 * CENT);
      add_coin(10 * CENT);
      add_coin(15 * CENT);
      add_coin(20 * CENT);
      add_coin(25 * CENT);

      fails = 0;
      for (int j = 0; j < RANDOM_REPEATS; j++) {
        BOOST_CHECK(testWallet.SelectCoinsMinConf(90 * CENT, 1, 6, 0, vCoins,
                                                  setCoinsRet, nValueRet));
        BOOST_CHECK(testWallet.SelectCoinsMinConf(90 * CENT, 1, 6, 0, vCoins,
                                                  setCoinsRet2, nValueRet));
        if (equal_sets(setCoinsRet, setCoinsRet2))
          fails++;
      }
      BOOST_CHECK_NE(fails, RANDOM_REPEATS);
    }
  }
  empty_wallet();
}

BOOST_AUTO_TEST_CASE(ApproximateBestSubset) {
  CoinSet setCoinsRet;
  CAmount nValueRet;

  LOCK(testWallet.cs_wallet);

  empty_wallet();

  for (int i = 0; i < 1000; i++)
    add_coin(1000 * COIN);
  add_coin(3 * COIN);

  BOOST_CHECK(testWallet.SelectCoinsMinConf(1003 * COIN, 1, 6, 0, vCoins,
                                            setCoinsRet, nValueRet));
  BOOST_CHECK_EQUAL(nValueRet, 1003 * COIN);
  BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

  empty_wallet();
}

static void AddKey(CWallet &wallet, const CKey &key) {
  LOCK(wallet.cs_wallet);
  wallet.AddKeyPubKey(key, key.GetPubKey());
}

BOOST_FIXTURE_TEST_CASE(rescan, TestChain100Setup) {
  CBlockIndex *const nullBlock = nullptr;
  CBlockIndex *oldTip = chainActive.Tip();
  GetBlockFileInfo(oldTip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE;
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  CBlockIndex *newTip = chainActive.Tip();

  LOCK(cs_main);

  {
    CWallet wallet;
    AddKey(wallet, coinbaseKey);
    WalletRescanReserver reserver(&wallet);
    reserver.reserve();
    BOOST_CHECK_EQUAL(
        nullBlock, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
    BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 100 * COIN * COIN_SCALE);
  }

  PruneOneBlockFile(oldTip->GetBlockPos().nFile);
  UnlinkPrunedFiles({oldTip->GetBlockPos().nFile});

  {
    CWallet wallet;
    AddKey(wallet, coinbaseKey);
    WalletRescanReserver reserver(&wallet);
    reserver.reserve();
    BOOST_CHECK_EQUAL(
        oldTip, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
    BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 50 * COIN * COIN_SCALE);
  }

  {
    CWallet wallet;
    vpwallets.insert(vpwallets.begin(), &wallet);
    UniValue keys;
    keys.setArray();
    UniValue key;
    key.setObject();
    key.pushKV("scriptPubKey",
               HexStr(GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
    key.pushKV("timestamp", 0);
    key.pushKV("internal", UniValue(true));
    keys.push_back(key);
    key.clear();
    key.setObject();
    CKey futureKey;
    futureKey.MakeNewKey(true);
    key.pushKV("scriptPubKey",
               HexStr(GetScriptForRawPubKey(futureKey.GetPubKey())));
    key.pushKV("timestamp", newTip->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1);
    key.pushKV("internal", UniValue(true));
    keys.push_back(key);
    JSONRPCRequest request;
    request.params.setArray();
    request.params.push_back(keys);

    UniValue response = importmulti(request);
    BOOST_CHECK_EQUAL(
        response.write(),
        strprintf("[{\"success\":false,\"error\":{\"code\":-1,\"message\":"
                  "\"Rescan failed for key with creation "
                  "timestamp %d. There was an error reading a block from time "
                  "%d, which is after or within %d "
                  "seconds of key creation, and could contain transactions "
                  "pertaining to the key. As a result, "
                  "transactions and coins using this key may not appear in the "
                  "wallet. This error could be caused "
                  "by pruning or data corruption (see litecoincashd log for "
                  "details) and could be dealt with by "
                  "downloading and rescanning the relevant blocks (see "
                  "-reindex and -rescan "
                  "options).\"}},{\"success\":true}]",
                  0, oldTip->GetBlockTimeMax(), TIMESTAMP_WINDOW));
    vpwallets.erase(vpwallets.begin());
  }
}

BOOST_FIXTURE_TEST_CASE(importwallet_rescan, TestChain100Setup) {
  g_address_type = OUTPUT_TYPE_DEFAULT;
  g_change_type = OUTPUT_TYPE_DEFAULT;

  const int64_t BLOCK_TIME = chainActive.Tip()->GetBlockTimeMax() + 5;
  SetMockTime(BLOCK_TIME);
  coinbaseTxns.emplace_back(
      *CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()))
           .vtx[0]);
  coinbaseTxns.emplace_back(
      *CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()))
           .vtx[0]);

  const int64_t KEY_TIME = BLOCK_TIME + TIMESTAMP_WINDOW;
  SetMockTime(KEY_TIME);
  coinbaseTxns.emplace_back(
      *CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()))
           .vtx[0]);

  LOCK(cs_main);

  {
    CWallet wallet;
    LOCK(wallet.cs_wallet);
    wallet.mapKeyMetadata[coinbaseKey.GetPubKey().GetID()].nCreateTime =
        KEY_TIME;
    wallet.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());

    JSONRPCRequest request;
    request.params.setArray();
    request.params.push_back((pathTemp / "wallet.backup").string());
    vpwallets.insert(vpwallets.begin(), &wallet);
    ::dumpwallet(request);
  }

  {
    CWallet wallet;

    JSONRPCRequest request;
    request.params.setArray();
    request.params.push_back((pathTemp / "wallet.backup").string());
    vpwallets[0] = &wallet;
    ::importwallet(request);

    LOCK(wallet.cs_wallet);
    BOOST_CHECK_EQUAL(wallet.mapWallet.size(), 3);
    BOOST_CHECK_EQUAL(coinbaseTxns.size(), 103);
    for (size_t i = 0; i < coinbaseTxns.size(); ++i) {
      bool found = wallet.GetWalletTx(coinbaseTxns[i].GetHash());
      bool expected = i >= 100;
      BOOST_CHECK_EQUAL(found, expected);
    }
  }

  SetMockTime(0);
  vpwallets.erase(vpwallets.begin());
}

BOOST_FIXTURE_TEST_CASE(coin_mark_dirty_immature_credit, TestChain100Setup) {
  CWallet wallet;
  CWalletTx wtx(&wallet, MakeTransactionRef(coinbaseTxns.back()));
  LOCK2(cs_main, wallet.cs_wallet);
  wtx.hashBlock = chainActive.Tip()->GetBlockHash();
  wtx.nIndex = 0;

  BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), 0);

  wtx.MarkDirty();
  wallet.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
  BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), 50 * COIN * COIN_SCALE);
}

static int64_t AddTx(CWallet &wallet, uint32_t lockTime, int64_t mockTime,
                     int64_t blockTime) {
  CMutableTransaction tx;
  tx.nLockTime = lockTime;
  SetMockTime(mockTime);
  CBlockIndex *block = nullptr;
  if (blockTime > 0) {
    LOCK(cs_main);
    auto inserted = mapBlockIndex.emplace(GetRandHash(), new CBlockIndex);
    assert(inserted.second);
    const uint256 &hash = inserted.first->first;
    block = inserted.first->second;
    block->nTime = blockTime;
    block->phashBlock = &hash;
  }

  CWalletTx wtx(&wallet, MakeTransactionRef(tx));
  if (block) {
    wtx.SetMerkleBranch(block, 0);
  }
  wallet.AddToWallet(wtx);
  LOCK(wallet.cs_wallet);
  return wallet.mapWallet.at(wtx.GetHash()).nTimeSmart;
}

BOOST_AUTO_TEST_CASE(ComputeTimeSmart) {
  CWallet wallet;

  BOOST_CHECK_EQUAL(AddTx(wallet, 1, 100, 120), 100);

  BOOST_CHECK_EQUAL(AddTx(wallet, 1, 200, 220), 100);

  BOOST_CHECK_EQUAL(AddTx(wallet, 2, 300, 0), 300);

  BOOST_CHECK_EQUAL(AddTx(wallet, 3, 420, 400), 400);

  BOOST_CHECK_EQUAL(AddTx(wallet, 4, 500, 390), 400);

  BOOST_CHECK_EQUAL(AddTx(wallet, 5, 50, 600), 300);

  SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(LoadReceiveRequests) {
  CTxDestination dest = CKeyID();
  LOCK(pwalletMain->cs_wallet);
  pwalletMain->AddDestData(dest, "misc", "val_misc");
  pwalletMain->AddDestData(dest, "rr0", "val_rr0");
  pwalletMain->AddDestData(dest, "rr1", "val_rr1");

  auto values = pwalletMain->GetDestValues("rr");
  BOOST_CHECK_EQUAL(values.size(), 2);
  BOOST_CHECK_EQUAL(values[0], "val_rr0");
  BOOST_CHECK_EQUAL(values[1], "val_rr1");
}

class ListCoinsTestingSetup : public TestChain100Setup {
public:
  ListCoinsTestingSetup() {
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    ::bitdb.MakeMock();
    g_address_type = OUTPUT_TYPE_DEFAULT;
    g_change_type = OUTPUT_TYPE_DEFAULT;
    wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(
        new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
    bool firstRun;
    wallet->LoadWallet(firstRun);
    AddKey(*wallet, coinbaseKey);
    WalletRescanReserver reserver(wallet.get());
    reserver.reserve();
    wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
  }

  ~ListCoinsTestingSetup() {
    wallet.reset();
    ::bitdb.Flush(true);
    ::bitdb.Reset();
  }

  CWalletTx &AddTx(CRecipient recipient) {
    CWalletTx wtx;
    CReserveKey reservekey(wallet.get());
    CAmount fee;
    int changePos = -1;
    std::string error;
    CCoinControl dummy;
    BOOST_CHECK(wallet->CreateTransaction({recipient}, wtx, reservekey, fee,
                                          changePos, error, dummy));
    CValidationState state;
    BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));
    CMutableTransaction blocktx;
    {
      LOCK(wallet->cs_wallet);
      blocktx = CMutableTransaction(*wallet->mapWallet.at(wtx.GetHash()).tx);
    }
    CreateAndProcessBlock({CMutableTransaction(blocktx)},
                          GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    LOCK(wallet->cs_wallet);
    auto it = wallet->mapWallet.find(wtx.GetHash());
    BOOST_CHECK(it != wallet->mapWallet.end());
    it->second.SetMerkleBranch(chainActive.Tip(), 1);
    return it->second;
  }

  std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_CASE(ListCoins, ListCoinsTestingSetup) {
  std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

  auto list = wallet->ListCoins();
  BOOST_CHECK_EQUAL(list.size(), 1);
  BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(),
                    coinbaseAddress);
  BOOST_CHECK_EQUAL(list.begin()->second.size(), 1);

  BOOST_CHECK_EQUAL(50 * COIN * COIN_SCALE, wallet->GetAvailableBalance());

  AddTx(CRecipient{GetScriptForRawPubKey({}), 1 * COIN, false});
  list = wallet->ListCoins();
  BOOST_CHECK_EQUAL(list.size(), 1);
  BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(),
                    coinbaseAddress);
  BOOST_CHECK_EQUAL(list.begin()->second.size(), 2);

  std::vector<COutput> available;
  wallet->AvailableCoins(available);
  BOOST_CHECK_EQUAL(available.size(), 2);
  for (const auto &group : list) {
    for (const auto &coin : group.second) {
      LOCK(wallet->cs_wallet);
      wallet->LockCoin(COutPoint(coin.tx->GetHash(), coin.i));
    }
  }
  wallet->AvailableCoins(available);
  BOOST_CHECK_EQUAL(available.size(), 0);

  list = wallet->ListCoins();
  BOOST_CHECK_EQUAL(list.size(), 1);
  BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(),
                    coinbaseAddress);
  BOOST_CHECK_EQUAL(list.begin()->second.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
