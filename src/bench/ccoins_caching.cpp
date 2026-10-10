// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <coins.h>
#include <policy/policy.h>
#include <wallet/crypter.h>

#include <vector>

static std::vector<CMutableTransaction>
SetupDummyInputs(CBasicKeyStore &keystoreRet, CCoinsViewCache &coinsRet) {
  std::vector<CMutableTransaction> dummyTransactions;
  dummyTransactions.resize(2);

  CKey key[4];
  for (int i = 0; i < 4; i++) {
    key[i].MakeNewKey(i % 2);
    keystoreRet.AddKey(key[i]);
  }

  dummyTransactions[0].vout.resize(2);
  dummyTransactions[0].vout[0].nValue = 11 * CENT;
  dummyTransactions[0].vout[0].scriptPubKey << ToByteVector(key[0].GetPubKey())
                                            << OP_CHECKSIG;
  dummyTransactions[0].vout[1].nValue = 50 * CENT;
  dummyTransactions[0].vout[1].scriptPubKey << ToByteVector(key[1].GetPubKey())
                                            << OP_CHECKSIG;
  AddCoins(coinsRet, dummyTransactions[0], 0);

  dummyTransactions[1].vout.resize(2);
  dummyTransactions[1].vout[0].nValue = 21 * CENT;
  dummyTransactions[1].vout[0].scriptPubKey =
      GetScriptForDestination(key[2].GetPubKey().GetID());
  dummyTransactions[1].vout[1].nValue = 22 * CENT;
  dummyTransactions[1].vout[1].scriptPubKey =
      GetScriptForDestination(key[3].GetPubKey().GetID());
  AddCoins(coinsRet, dummyTransactions[1], 0);

  return dummyTransactions;
}

static void CCoinsCaching(benchmark::State &state) {
  CBasicKeyStore keystore;
  CCoinsView coinsDummy;
  CCoinsViewCache coins(&coinsDummy);
  std::vector<CMutableTransaction> dummyTransactions =
      SetupDummyInputs(keystore, coins);

  CMutableTransaction t1;
  t1.vin.resize(3);
  t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
  t1.vin[0].prevout.n = 1;
  t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
  t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
  t1.vin[1].prevout.n = 0;
  t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0)
                      << std::vector<unsigned char>(33, 4);
  t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
  t1.vin[2].prevout.n = 1;
  t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0)
                      << std::vector<unsigned char>(33, 4);
  t1.vout.resize(2);
  t1.vout[0].nValue = 90 * CENT;
  t1.vout[0].scriptPubKey << OP_1;

  while (state.KeepRunning()) {
    bool success = AreInputsStandard(t1, coins);
    assert(success);
    CAmount value = coins.GetValueIn(t1);
    assert(value == (50 + 21 + 22) * CENT);
  }
}

BENCHMARK(CCoinsCaching, 170 * 1000);
