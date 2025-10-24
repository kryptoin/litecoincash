// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <versionbits.h>

#include <boost/test/unit_test.hpp>

int32_t TestTime(int nHeight) { return 1415926536 + 600 * nHeight; }

static const Consensus::Params paramsDummy = Consensus::Params();

class TestConditionChecker : public AbstractThresholdConditionChecker {
private:
  mutable ThresholdConditionCache cache;

public:
  int64_t BeginTime(const Consensus::Params &params) const override {
    return TestTime(10000);
  }
  int64_t EndTime(const Consensus::Params &params) const override {
    return TestTime(20000);
  }
  int Period(const Consensus::Params &params) const override { return 1000; }
  int Threshold(const Consensus::Params &params) const override { return 900; }
  bool Condition(const CBlockIndex *pindex,
                 const Consensus::Params &params) const override {
    return (pindex->nVersion & 0x100);
  }

  ThresholdState GetStateFor(const CBlockIndex *pindexPrev) const {
    return AbstractThresholdConditionChecker::GetStateFor(pindexPrev,
                                                          paramsDummy, cache);
  }
  int GetStateSinceHeightFor(const CBlockIndex *pindexPrev) const {
    return AbstractThresholdConditionChecker::GetStateSinceHeightFor(
        pindexPrev, paramsDummy, cache);
  }
};

class TestAlwaysActiveConditionChecker : public TestConditionChecker {
public:
  int64_t BeginTime(const Consensus::Params &params) const override {
    return Consensus::BIP9Deployment::ALWAYS_ACTIVE;
  }
};

#define CHECKERS 6

class VersionBitsTester {
  std::vector<CBlockIndex *> vpblock;

  TestConditionChecker checker[CHECKERS];

  TestAlwaysActiveConditionChecker checker_always[CHECKERS];

  int num;

public:
  VersionBitsTester() : num(0) {}

  VersionBitsTester &Reset() {
    for (unsigned int i = 0; i < vpblock.size(); i++) {
      delete vpblock[i];
    }
    for (unsigned int i = 0; i < CHECKERS; i++) {
      checker[i] = TestConditionChecker();
      checker_always[i] = TestAlwaysActiveConditionChecker();
    }
    vpblock.clear();
    return *this;
  }

  ~VersionBitsTester() { Reset(); }

  VersionBitsTester &Mine(unsigned int height, int32_t nTime,
                          int32_t nVersion) {
    while (vpblock.size() < height) {
      CBlockIndex *pindex = new CBlockIndex();
      pindex->nHeight = vpblock.size();
      pindex->pprev = vpblock.size() > 0 ? vpblock.back() : nullptr;
      pindex->nTime = nTime;
      pindex->nVersion = nVersion;
      pindex->BuildSkip();
      vpblock.push_back(pindex);
    }
    return *this;
  }

  VersionBitsTester &TestStateSinceHeight(int height) {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateSinceHeightFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                height,
                            strprintf("Test %i for StateSinceHeight", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateSinceHeightFor(
                vpblock.empty() ? nullptr : vpblock.back()) == 0,
            strprintf("Test %i for StateSinceHeight (always active)", num));
      }
    }
    num++;
    return *this;
  }

  VersionBitsTester &TestDefined() {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                THRESHOLD_DEFINED,
                            strprintf("Test %i for DEFINED", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateFor(
                vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE,
            strprintf("Test %i for ACTIVE (always active)", num));
      }
    }
    num++;
    return *this;
  }

  VersionBitsTester &TestStarted() {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                THRESHOLD_STARTED,
                            strprintf("Test %i for STARTED", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateFor(
                vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE,
            strprintf("Test %i for ACTIVE (always active)", num));
      }
    }
    num++;
    return *this;
  }

  VersionBitsTester &TestLockedIn() {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                THRESHOLD_LOCKED_IN,
                            strprintf("Test %i for LOCKED_IN", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateFor(
                vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE,
            strprintf("Test %i for ACTIVE (always active)", num));
      }
    }
    num++;
    return *this;
  }

  VersionBitsTester &TestActive() {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                THRESHOLD_ACTIVE,
                            strprintf("Test %i for ACTIVE", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateFor(
                vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE,
            strprintf("Test %i for ACTIVE (always active)", num));
      }
    }
    num++;
    return *this;
  }

  VersionBitsTester &TestFailed() {
    for (int i = 0; i < CHECKERS; i++) {
      if (InsecureRandBits(i) == 0) {
        BOOST_CHECK_MESSAGE(checker[i].GetStateFor(
                                vpblock.empty() ? nullptr : vpblock.back()) ==
                                THRESHOLD_FAILED,
                            strprintf("Test %i for FAILED", num));
        BOOST_CHECK_MESSAGE(
            checker_always[i].GetStateFor(
                vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE,
            strprintf("Test %i for ACTIVE (always active)", num));
      }
    }
    num++;
    return *this;
  }

  CBlockIndex *Tip() { return vpblock.size() ? vpblock.back() : nullptr; }
};

BOOST_FIXTURE_TEST_SUITE(versionbits_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(versionbits_test) {
  for (int i = 0; i < 64; i++) {
    VersionBitsTester()
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1, TestTime(1), 0x100)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(11, TestTime(11), 0x100)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(989, TestTime(989), 0x100)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(999, TestTime(20000), 0x100)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1000, TestTime(20000), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)
        .Mine(1999, TestTime(30001), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)
        .Mine(2000, TestTime(30002), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)
        .Mine(2001, TestTime(30003), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)
        .Mine(2999, TestTime(30004), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)
        .Mine(3000, TestTime(30005), 0x100)
        .TestFailed()
        .TestStateSinceHeight(1000)

        .Reset()
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1, TestTime(1), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1000, TestTime(10000) - 1, 0x100)
        .TestDefined()
        .TestStateSinceHeight(0)

        .Mine(2000, TestTime(10000), 0x100)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2051, TestTime(10010), 0)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2950, TestTime(10020), 0x100)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(3000, TestTime(20000), 0)
        .TestFailed()
        .TestStateSinceHeight(3000)

        .Mine(4000, TestTime(20010), 0x100)
        .TestFailed()
        .TestStateSinceHeight(3000)

        .Reset()
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1, TestTime(1), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1000, TestTime(10000) - 1, 0x101)
        .TestDefined()
        .TestStateSinceHeight(0)

        .Mine(2000, TestTime(10000), 0x101)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2999, TestTime(30000), 0x100)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(3000, TestTime(30000), 0x100)
        .TestFailed()
        .TestStateSinceHeight(3000)

        .Mine(3999, TestTime(30001), 0)
        .TestFailed()
        .TestStateSinceHeight(3000)
        .Mine(4000, TestTime(30002), 0)
        .TestFailed()
        .TestStateSinceHeight(3000)
        .Mine(14333, TestTime(30003), 0)
        .TestFailed()
        .TestStateSinceHeight(3000)
        .Mine(24000, TestTime(40000), 0)
        .TestFailed()
        .TestStateSinceHeight(3000)

        .Reset()
        .TestDefined()
        .Mine(1, TestTime(1), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1000, TestTime(10000) - 1, 0x101)
        .TestDefined()
        .TestStateSinceHeight(0)

        .Mine(2000, TestTime(10000), 0x101)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2050, TestTime(10010), 0x200)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2950, TestTime(10020), 0x100)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(2999, TestTime(19999), 0x200)
        .TestStarted()
        .TestStateSinceHeight(2000)

        .Mine(3000, TestTime(29999), 0x200)
        .TestLockedIn()
        .TestStateSinceHeight(3000)

        .Mine(3999, TestTime(30001), 0)
        .TestLockedIn()
        .TestStateSinceHeight(3000)
        .Mine(4000, TestTime(30002), 0)
        .TestActive()
        .TestStateSinceHeight(4000)
        .Mine(14333, TestTime(30003), 0)
        .TestActive()
        .TestStateSinceHeight(4000)
        .Mine(24000, TestTime(40000), 0)
        .TestActive()
        .TestStateSinceHeight(4000)

        .Reset()
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(999, TestTime(999), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(1000, TestTime(1000), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(2000, TestTime(2000), 0)
        .TestDefined()
        .TestStateSinceHeight(0)
        .Mine(3000, TestTime(10000), 0)
        .TestStarted()
        .TestStateSinceHeight(3000)
        .Mine(4000, TestTime(10000), 0)
        .TestStarted()
        .TestStateSinceHeight(3000)
        .Mine(5000, TestTime(10000), 0)
        .TestStarted()
        .TestStateSinceHeight(3000)
        .Mine(6000, TestTime(20000), 0)
        .TestFailed()
        .TestStateSinceHeight(6000)
        .Mine(7000, TestTime(20000), 0x100)
        .TestFailed()
        .TestStateSinceHeight(6000);
  }

  const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
  const Consensus::Params &mainnetParams = chainParams->GetConsensus();
  for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
    uint32_t bitmask =
        VersionBitsMask(mainnetParams, (Consensus::DeploymentPos)i);

    BOOST_CHECK_EQUAL(bitmask & ~(uint32_t)VERSIONBITS_TOP_MASK, bitmask);

    for (int j = i + 1; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; j++) {
      if (VersionBitsMask(mainnetParams, (Consensus::DeploymentPos)j) ==
          bitmask) {
        BOOST_CHECK(mainnetParams.vDeployments[j].nStartTime >
                        mainnetParams.vDeployments[i].nTimeout ||
                    mainnetParams.vDeployments[i].nStartTime >
                        mainnetParams.vDeployments[j].nTimeout);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(versionbits_computeblockversion) {
  const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
  const Consensus::Params &mainnetParams = chainParams->GetConsensus();

  int64_t bit = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit;
  int64_t nStartTime =
      mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime;
  int64_t nTimeout =
      mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout;

  assert(nStartTime < nTimeout);

  VersionBitsTester firstChain, secondChain;

  int64_t nTime = nStartTime - 1;

  CBlockIndex *lastBlock = nullptr;
  lastBlock =
      firstChain.Mine(8064, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
  BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit),
                    0);

  for (int i = 1; i < 8060; i++) {
    lastBlock =
        firstChain.Mine(8064 + i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION)
            .Tip();

    BOOST_CHECK_EQUAL(
        ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);
  }

  nTime = nStartTime;
  for (int i = 8060; i <= 8064; i++) {
    lastBlock =
        firstChain.Mine(8064 + i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION)
            .Tip();
    BOOST_CHECK_EQUAL(
        ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);
  }

  lastBlock =
      firstChain.Mine(24192, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();

  BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
              0);

  BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) &
                        VERSIONBITS_TOP_MASK,
                    VERSIONBITS_TOP_BITS);

  nTime += 600;
  int blocksToMine = 16128;

  int nHeight = 24192;

  while (nTime < nTimeout && blocksToMine > 0) {
    lastBlock =
        firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION)
            .Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
                0);
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) &
                          VERSIONBITS_TOP_MASK,
                      VERSIONBITS_TOP_BITS);
    blocksToMine--;
    nTime += 600;
    nHeight += 1;
  }

  nTime = nTimeout;

  for (int i = 0; i < 8063; i++) {
    lastBlock =
        firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION)
            .Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
                0);
    nHeight += 1;
  }

  lastBlock =
      firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION)
          .Tip();
  BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit),
                    0);

  nTime = nStartTime;

  lastBlock =
      secondChain.Mine(8064, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
  BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
              0);

  lastBlock =
      secondChain.Mine(16128, nTime, VERSIONBITS_TOP_BITS | (1 << bit)).Tip();

  BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
              0);

  lastBlock =
      secondChain.Mine(24191, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
  BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) !=
              0);
  lastBlock =
      secondChain.Mine(24192, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
  BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit),
                    0);
}

BOOST_AUTO_TEST_SUITE_END()
