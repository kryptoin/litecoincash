// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees.h>
#include <policy/policy.h>
#include <txmempool.h>
#include <uint256.h>
#include <util.h>

#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(policyestimator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(BlockPolicyEstimates) {
  CBlockPolicyEstimator feeEst;
  CTxMemPool mpool(&feeEst);
  TestMemPoolEntryHelper entry;
  CAmount basefee(2000);
  CAmount deltaFee(100);
  std::vector<CAmount> feeV;

  for (int j = 0; j < 10; j++) {
    feeV.push_back(basefee * (j + 1));
  }

  std::vector<uint256> txHashes[10];

  CScript garbage;
  for (unsigned int i = 0; i < 128; i++)
    garbage.push_back('X');
  CMutableTransaction tx;
  tx.vin.resize(1);
  tx.vin[0].scriptSig = garbage;
  tx.vout.resize(1);
  tx.vout[0].nValue = 0LL;
  CFeeRate baseRate(basefee, GetVirtualTransactionSize(tx));

  std::vector<CTransactionRef> block;
  int blocknum = 0;

  while (blocknum < 200) {
    for (int j = 0; j < 10; j++) {
      for (int k = 0; k < 4; k++) {
        tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k;

        uint256 hash = tx.GetHash();
        mpool.addUnchecked(
            hash,
            entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
        txHashes[j].push_back(hash);
      }
    }

    for (int h = 0; h <= blocknum % 10; h++) {
      while (txHashes[9 - h].size()) {
        CTransactionRef ptx = mpool.get(txHashes[9 - h].back());
        if (ptx)
          block.push_back(ptx);
        txHashes[9 - h].pop_back();
      }
    }
    mpool.removeForBlock(block, ++blocknum);
    block.clear();

    if (blocknum == 3) {
      BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
      BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() <
                  9 * baseRate.GetFeePerK() + deltaFee);
      BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() >
                  9 * baseRate.GetFeePerK() - deltaFee);
    }
  }

  std::vector<CAmount> origFeeEst;

  for (int i = 1; i < 10; i++) {
    origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());
    if (i > 2) {
      BOOST_CHECK(origFeeEst[i - 1] <= origFeeEst[i - 2]);
    }
    int mult = 11 - i;
    if (i % 2 == 0) {
      BOOST_CHECK(origFeeEst[i - 1] < mult * baseRate.GetFeePerK() + deltaFee);
      BOOST_CHECK(origFeeEst[i - 1] > mult * baseRate.GetFeePerK() - deltaFee);
    }
  }

  for (int i = 10; i <= 48; i++) {
    origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());
  }

  while (blocknum < 250)
    mpool.removeForBlock(block, ++blocknum);

  BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
  for (int i = 2; i < 10; i++) {
    BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() <
                origFeeEst[i - 1] + deltaFee);
    BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() >
                origFeeEst[i - 1] - deltaFee);
  }

  while (blocknum < 265) {
    for (int j = 0; j < 10; j++) {
      for (int k = 0; k < 4; k++) {
        tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k;
        uint256 hash = tx.GetHash();
        mpool.addUnchecked(
            hash,
            entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
        txHashes[j].push_back(hash);
      }
    }
    mpool.removeForBlock(block, ++blocknum);
  }

  for (int i = 1; i < 10; i++) {
    BOOST_CHECK(feeEst.estimateFee(i) == CFeeRate(0) ||
                feeEst.estimateFee(i).GetFeePerK() >
                    origFeeEst[i - 1] - deltaFee);
  }

  for (int j = 0; j < 10; j++) {
    while (txHashes[j].size()) {
      CTransactionRef ptx = mpool.get(txHashes[j].back());
      if (ptx)
        block.push_back(ptx);
      txHashes[j].pop_back();
    }
  }
  mpool.removeForBlock(block, 266);
  block.clear();
  BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
  for (int i = 2; i < 10; i++) {
    BOOST_CHECK(feeEst.estimateFee(i) == CFeeRate(0) ||
                feeEst.estimateFee(i).GetFeePerK() >
                    origFeeEst[i - 1] - deltaFee);
  }

  while (blocknum < 665) {
    for (int j = 0; j < 10; j++) {
      for (int k = 0; k < 4; k++) {
        tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k;
        uint256 hash = tx.GetHash();
        mpool.addUnchecked(
            hash,
            entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
        CTransactionRef ptx = mpool.get(hash);
        if (ptx)
          block.push_back(ptx);
      }
    }
    mpool.removeForBlock(block, ++blocknum);
    block.clear();
  }
  BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
  for (int i = 2; i < 9; i++) {
    BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() <
                origFeeEst[i - 1] - deltaFee);
  }
}

BOOST_AUTO_TEST_SUITE_END()
