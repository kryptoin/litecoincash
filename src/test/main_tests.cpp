// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <validation.h>

#include <test/test_bitcoin.h>

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params &consensusParams) {
  int maxHalvings = 64;
  CAmount nInitialSubsidy = 50 * COIN * COIN_SCALE;

  CAmount nPreviousSubsidy = nInitialSubsidy * 2;

  BOOST_CHECK_EQUAL(nPreviousSubsidy, nInitialSubsidy * 2);
  for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
    int nHeight = nHalvings * consensusParams.nSubsidyHalvingInterval;
    CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
    if (nHeight >= 6164768) {
      BOOST_CHECK(nSubsidy == 0);
    } else {
      BOOST_CHECK(nSubsidy <= nInitialSubsidy);
      BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
      nPreviousSubsidy = nSubsidy;
    }
  }
  BOOST_CHECK_EQUAL(
      GetBlockSubsidy(maxHalvings * consensusParams.nSubsidyHalvingInterval,
                      consensusParams),
      0);
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval) {
  const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
  const Consensus::Params mainConsensusParams = chainParams->GetConsensus();

  Consensus::Params consensusParams;
  consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
  consensusParams.lastScryptBlock = mainConsensusParams.lastScryptBlock;

  consensusParams.slowStartBlocks = mainConsensusParams.slowStartBlocks;

  TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test) {
  const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
  TestBlockSubsidyHalvings(chainParams->GetConsensus());

  TestBlockSubsidyHalvings(150);

  TestBlockSubsidyHalvings(1000);
}

BOOST_AUTO_TEST_CASE(block_subsidy_money_limit)

{
  const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
  const Consensus::Params consensusParams = chainParams->GetConsensus();
  CAmount nSum = 0;
  for (int nHeight = 0; nHeight < 6215971; nHeight += 1) {
    CAmount nSubsidy = GetBlockSubsidy(nHeight, chainParams->GetConsensus());
    nSum += nSubsidy;
    BOOST_CHECK(MoneyRange(nSum));
  }
  BOOST_CHECK_EQUAL(nSum, 8399999998750000ULL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all) {
  boost::signals2::signal<bool(), CombinerAll> Test;
  BOOST_CHECK(Test());
  Test.connect(&ReturnFalse);
  BOOST_CHECK(!Test());
  Test.connect(&ReturnTrue);
  BOOST_CHECK(!Test());
  Test.disconnect(&ReturnFalse);
  BOOST_CHECK(Test());
  Test.disconnect(&ReturnTrue);
  BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
