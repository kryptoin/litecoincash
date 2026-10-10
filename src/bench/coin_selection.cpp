// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <wallet/wallet.h>

#include <set>

static void addCoin(const CAmount &nValue, const CWallet &wallet,
                    std::vector<COutput> &vCoins) {
  int nInput = 0;

  static int nextLockTime = 0;
  CMutableTransaction tx;
  tx.nLockTime = nextLockTime++;

  tx.vout.resize(nInput + 1);
  tx.vout[nInput].nValue = nValue;
  CWalletTx *wtx = new CWalletTx(&wallet, MakeTransactionRef(std::move(tx)));

  int nAge = 6 * 24;
  COutput output(wtx, nInput, nAge, true, true, true);
  vCoins.push_back(output);
}

static void CoinSelection(benchmark::State &state) {
  const CWallet wallet;
  std::vector<COutput> vCoins;
  LOCK(wallet.cs_wallet);

  while (state.KeepRunning()) {
    for (COutput output : vCoins)
      delete output.tx;
    vCoins.clear();

    for (int i = 0; i < 1000; i++)
      addCoin(1000 * COIN, wallet, vCoins);
    addCoin(3 * COIN, wallet, vCoins);

    std::set<CInputCoin> setCoinsRet;
    CAmount nValueRet;
    bool success = wallet.SelectCoinsMinConf(1003 * COIN, 1, 6, 0, vCoins,
                                             setCoinsRet, nValueRet);
    assert(success);
    assert(nValueRet == 1003 * COIN);
    assert(setCoinsRet.size() == 2);
  }
}

BENCHMARK(CoinSelection, 650);
