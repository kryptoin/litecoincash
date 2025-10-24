// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <base58.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char *pszTimestamp,
                                 const CScript &genesisOutputScript,
                                 uint32_t nTime, uint32_t nNonce,
                                 uint32_t nBits, int32_t nVersion,
                                 const CAmount &genesisReward) {
  CMutableTransaction txNew;
  txNew.nVersion = 1;
  txNew.vin.resize(1);
  txNew.vout.resize(1);
  txNew.vin[0].scriptSig = CScript()
                           << 486604799 << CScriptNum(4)
                           << std::vector<unsigned char>(
                                  (const unsigned char *)pszTimestamp,
                                  (const unsigned char *)pszTimestamp +
                                      strlen(pszTimestamp));
  txNew.vout[0].nValue = genesisReward;
  txNew.vout[0].scriptPubKey = genesisOutputScript;

  CBlock genesis;
  genesis.nTime = nTime;
  genesis.nBits = nBits;
  genesis.nNonce = nNonce;
  genesis.nVersion = nVersion;
  genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
  genesis.hashPrevBlock.SetNull();
  genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
  return genesis;
}

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce,
                                 uint32_t nBits, int32_t nVersion,
                                 const CAmount &genesisReward) {
  const char *pszTimestamp =
      "NY Times 05/Oct/2011 Steve Jobs, Apple’s Visionary, Dies at 56";
  const CScript genesisOutputScript =
      CScript() << ParseHex("040184710fa689ad5023690c80f3a49c8f13f8d45b8c857fbc"
                            "bc8bc4a8e4d3eb4b10f4d4604fa08dce601aaf0f470216fe1b"
                            "51850b4acf21b179c45070ac7b03a9")
                << OP_CHECKSIG;
  return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce,
                            nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d,
                                               int64_t nStartTime,
                                               int64_t nTimeout) {
  consensus.vDeployments[d].nStartTime = nStartTime;
  consensus.vDeployments[d].nTimeout = nTimeout;
}

class CMainParams : public CChainParams {
public:
  CMainParams() {
    strNetworkID = "main";
    consensus.nSubsidyHalvingInterval = 840000;
    consensus.BIP16Height = 218579;

    consensus.BIP34Height = 710000;
    consensus.BIP34Hash = uint256S(
        "fa09d204a83a768ed5a7c8d441fa62f2043abf420cff1226c7b4329aeb9d51cf");
    consensus.BIP65Height = 918684;

    consensus.BIP66Height = 811879;

    consensus.powLimit = uint256S(
        "00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60;

    consensus.nPowTargetSpacing = 2.5 * 60;
    consensus.fPowAllowMinDifficultyBlocks = false;
    consensus.fPowNoRetargeting = false;
    consensus.nRuleChangeActivationThreshold = 6048;

    consensus.nMinerConfirmationWindow = 8064;

    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime =
        1199145601;

    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout =
        1230767999;

    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1485561600;

    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517356801;

    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime =
        1485561600;

    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1517356801;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].bit = 7;
    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].nStartTime = 1545782400;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].nTimeout = 1577318400;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].bit = 9;
    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].nStartTime =
        1568937600;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].nTimeout =
        1600560000;

    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].bit = 7;
    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].nStartTime =
        1631793600;

    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].nTimeout =
        1631793600 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].bit = 9;
    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].nStartTime =
        2000000000;

    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].nTimeout =
        2000000000 + 31536000;

    consensus.powForkTime = 1518982404;

    consensus.lastScryptBlock = 1371111;

    consensus.powLimitSHA = uint256S(
        "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    consensus.slowStartBlocks = 2000;

    consensus.premineAmount = 550000;

    std::vector<unsigned char> vch =
        ParseHex("76a914c9f3305556963e2976ccf3348b89a6cc736b6a4e88ac");
    consensus.premineOutputScript = CScript(vch.begin(), vch.end());

    consensus.totalMoneySupplyHeight = 6215968;

    consensus.minBeeCost = 10000;

    consensus.beeCostFactor = 2500;

    consensus.beeCreationAddress = "CReateLitecoinCashWorkerBeeXYs19YQ";

    consensus.hiveCommunityAddress = "CashCFfv8CmdWo6wyMGQWtmQnaToyhgsWr";

    consensus.communityContribFactor = 10;

    consensus.beeGestationBlocks = 48 * 24;

    consensus.beeLifespanBlocks = 48 * 24 * 14;

    consensus.powLimitHive = uint256S(
        "0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    consensus.minHiveCheckBlock = 1537566;

    consensus.hiveTargetAdjustAggression = 30;

    consensus.hiveBlockSpacingTarget = 2;

    consensus.hiveBlockSpacingTargetTypical = 3;

    consensus.hiveBlockSpacingTargetTypical_1_1 = 2;

    consensus.hiveNonceMarker = 192;

    consensus.minK = 2;

    consensus.maxK = 16;

    consensus.maxHiveDiff = 0.006;

    consensus.maxKPow = 5;

    consensus.powSplit1 = 0.005;

    consensus.powSplit2 = 0.0025;

    consensus.maxConsecutiveHiveBlocks = 2;

    consensus.hiveDifficultyWindow = 36;

    consensus.lwmaAveragingWindow = 90;

    consensus.powTypeLimits.emplace_back(uint256S(
        "0x00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

    consensus.powTypeLimits.emplace_back(uint256S(
        "0x000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

    consensus.nickCreationAddress = "CashCFfv8CmdWo6wyMGQWtmQnaToyhgsWr";

    consensus.nickCreationCost3Char = 1000000000000;

    consensus.nickCreationCost4Char = 100000000000;
    consensus.nickCreationCostStandard = 1000000000;
    consensus.nickCreationAntiDust = 10000;

    consensus.nMinimumChainWork = uint256S(
        "0x00000000000000000000000000000000000000000000ba12a25c1f2da751fc96");

    consensus.defaultAssumeValid = uint256S(
        "0x00000000000000238fc08340331e2735a64ac2baccdc3db0984ef65c08f658b2");

    pchMessageStart[0] = 0xc7;
    pchMessageStart[1] = 0xe4;
    pchMessageStart[2] = 0xba;
    pchMessageStart[3] = 0xf8;
    nDefaultPort = 62458;
    nPruneAfterHeight = 100000;

    genesis = CreateGenesisBlock(1317972665, 2084524493, 0x1e0ffff0, 1,
                                 50 * COIN * COIN_SCALE);
    consensus.hashGenesisBlock = genesis.GetHash();
    assert(consensus.hashGenesisBlock ==
           uint256S("0x12a765e31ffd4059bada1e25190f6e98c99d9714d334efa41a195a7e"
                    "7e04bfe2"));
    assert(genesis.hashMerkleRoot ==
           uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011"
                    "edd4ced9"));

    vSeeds.emplace_back("seeds.litecoinca.sh");

    base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 28);
    base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 5);
    base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1, 50);
    base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 176);
    base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
    base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

    bech32_hrp = "lcc";

    vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main,
                                         pnSeed6_main + ARRAYLEN(pnSeed6_main));

    fDefaultConsistencyChecks = false;
    fRequireStandard = true;
    fMineBlocksOnDemand = false;

    checkpointData = {{{1500, uint256S("0x841a2965955dd288cfa707a755d05a54e45f8"
                                       "bd476835ec9af4402a2b59a2967")},
                       {4032, uint256S("0x9ce90e427198fc0ef05e5905ce3503725b80e"
                                       "26afd35a987965fd7e3d9cf0846")},
                       {8064, uint256S("0xeb984353fc5190f210651f150c40b8a4bab9e"
                                       "eeff0b729fcb3987da694430d70")},
                       {16128, uint256S("0x602edf1859b7f9a6af809f1d9b0e6cb66fdc"
                                        "1d4d9dcd7a4bec03e12a1ccd153d")},
                       {23420, uint256S("0xd80fdf9ca81afd0bd2b2a90ac3a9fe547da5"
                                        "8f2530ec874e978fce0b5101b507")},
                       {50000, uint256S("0x69dc37eb029b68f075a5012dcc0419c12767"
                                        "2adb4f3a32882b2b3e71d07a20a6")},
                       {80000, uint256S("0x4fcb7c02f676a300503f49c764a89955a8f9"
                                        "20b46a8cbecb4867182ecdb2e90a")},
                       {120000, uint256S("0xbd9d26924f05f6daa7f0155f32828ec89e8"
                                         "e29cee9e7121b026a7a3552ac6131")},
                       {161500, uint256S("0xdbe89880474f4bb4f75c227c77ba1cdc024"
                                         "991123b28b8418dbbf7798471ff43")},
                       {179620, uint256S("0x2ad9c65c990ac00426d18e446e0fd7be2ff"
                                         "a69e9a7dcb28358a50b2b78b9f709")},
                       {240000, uint256S("0x7140d1c4b4c2157ca217ee7636f24c9c73d"
                                         "b39c4590c4e6eab2e3ea1555088aa")},
                       {383640, uint256S("0x2b6809f094a9215bafc65eb3f110a35127a"
                                         "34be94b7d0590a096c3f126c6f364")},
                       {409004, uint256S("0x487518d663d9f1fa08611d9395ad74d982b"
                                         "667fbdc0e77e9cf39b4f1355908a3")},
                       {456000, uint256S("0xbf34f71cc6366cd487930d06be22f897e34"
                                         "ca6a40501ac7d401be32456372004")},
                       {638902, uint256S("0x15238656e8ec63d28de29a8c75fcf3a5819"
                                         "afc953dcd9cc45cecc53baec74f38")},
                       {721000, uint256S("0x198a7b4de1df9478e2463bd99d75b714eab"
                                         "235a2e63e741641dc8a759a9840e5")},
                       {1371112, uint256S("0x00000000de1e4e93317241177b5f1d72fc"
                                          "151c6e76815e9b0be4961dfd309d60")},

                       {1695238, uint256S("0x00000000000000238fc08340331e2735a6"
                                          "4ac2baccdc3db0984ef65c08f658b2")},
                       {1718000, uint256S("0x0000000000000059b656b7601a20df8091"
                                          "2e6ab8bf83c63e221cdf460adebe7b")},
                       {2500000, uint256S("0x000000000000000ac539d58f1df2a1e8e7"
                                          "2b5d3cc43355aed7aa19056e35a5e6")}}};

    chainTxData = ChainTxData{1631099985,

                              23615824,

                              0.0151

    };
  }
};

class CTestNetParams : public CChainParams {
public:
  CTestNetParams() {
    strNetworkID = "test";
    consensus.nSubsidyHalvingInterval = 840000;
    consensus.BIP16Height = 0;

    consensus.BIP34Height = 48;
    consensus.BIP34Hash = uint256S(
        "0x00000025140b1236292bc21b2afa9f3bd5c3d4a8cc1d0e3d1ba0ba7fdefc92eb");

    consensus.BIP65Height = 48;
    consensus.BIP66Height = 48;
    consensus.powLimit = uint256S(
        "00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60;

    consensus.nPowTargetSpacing = 2.5 * 60;
    consensus.fPowAllowMinDifficultyBlocks = true;
    consensus.fPowNoRetargeting = false;
    consensus.nRuleChangeActivationThreshold = 15;

    consensus.nMinerConfirmationWindow = 20;
    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime =
        1199145601;

    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout =
        1230767999;

    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1707828286;

    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout =
        1707828286 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime =
        1707828286;

    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout =
        1707828286 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].bit = 7;
    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].nStartTime = 1707828286;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE].nTimeout =
        1707828286 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].bit = 9;
    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].nStartTime =
        1707828695;

    consensus.vDeployments[Consensus::DEPLOYMENT_HIVE_1_1].nTimeout =
        1707828695 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].bit = 7;
    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].nStartTime =
        1707829366;

    consensus.vDeployments[Consensus::DEPLOYMENT_MINOTAURX].nTimeout =
        1707829366 + 31536000;

    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].bit = 9;
    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].nStartTime =
        1707923363;

    consensus.vDeployments[Consensus::DEPLOYMENT_RIALTO].nTimeout =
        1707923363 + 31536000;

    consensus.powForkTime = 1707828195;

    consensus.lastScryptBlock = 10;

    consensus.powLimitSHA = uint256S(
        "000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    consensus.slowStartBlocks = 40;

    consensus.premineAmount = 550000;

    std::vector<unsigned char> vch =
        ParseHex("76a91424af51d38b740a6dc2868dfd70fc16d76901e1e088ac");
    consensus.premineOutputScript = CScript(vch.begin(), vch.end());

    consensus.totalMoneySupplyHeight = 6215968;

    consensus.minBeeCost = 10000;

    consensus.beeCostFactor = 2500;

    consensus.beeCreationAddress = "tEstNetCreateLCCWorkerBeeXXXYq6T3r";

    consensus.hiveCommunityAddress = "tCY5JWV4LYe64ivrAE2rD6P3bYxYtcoTsz";

    consensus.communityContribFactor = 10;

    consensus.beeGestationBlocks = 40;

    consensus.beeLifespanBlocks = 48 * 24 * 14;

    consensus.powLimitHive = uint256S(
        "0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    consensus.minHiveCheckBlock = 50;

    consensus.hiveTargetAdjustAggression = 30;

    consensus.hiveBlockSpacingTarget = 2;

    consensus.hiveBlockSpacingTargetTypical = 3;

    consensus.hiveBlockSpacingTargetTypical_1_1 = 2;

    consensus.hiveNonceMarker = 192;

    consensus.minK = 2;

    consensus.maxK = 10;

    consensus.maxHiveDiff = 0.002;

    consensus.maxKPow = 5;

    consensus.powSplit1 = 0.001;

    consensus.powSplit2 = 0.0005;

    consensus.maxConsecutiveHiveBlocks = 2;

    consensus.hiveDifficultyWindow = 36;

    consensus.lwmaAveragingWindow = 90;

    consensus.powTypeLimits.emplace_back(uint256S(
        "0x000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

    consensus.powTypeLimits.emplace_back(uint256S(
        "0x000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

    consensus.nickCreationAddress = "tKJjaPcSS3nXYBN4QmmYnSanr9oUhSXAZB";

    consensus.nickCreationCost3Char = 100000000000;

    consensus.nickCreationCost4Char = 5000000000;
    consensus.nickCreationCostStandard = 100000000;
    consensus.nickCreationAntiDust = 10000;

    consensus.nMinimumChainWork = uint256S(
        "0x00000000000000000000000000000000000000000000000000000058c519899a");

    consensus.defaultAssumeValid = uint256S(
        "0x56d2eddb8cff67769e5c01eb30baa4897cc90c6c00f579a890adc8adfd608614");

    pchMessageStart[0] = 0xb6;
    pchMessageStart[1] = 0xf5;
    pchMessageStart[2] = 0xd3;
    pchMessageStart[3] = 0xcf;
    nDefaultPort = 62456;
    nPruneAfterHeight = 1000;

    genesis = CreateGenesisBlock(1486949366, 293345, 0x1e0ffff0, 1,
                                 50 * COIN * COIN_SCALE);
    consensus.hashGenesisBlock = genesis.GetHash();
    assert(consensus.hashGenesisBlock ==
           uint256S("0x4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb"
                    "4e3e29a0"));
    assert(genesis.hashMerkleRoot ==
           uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011"
                    "edd4ced9"));

    vFixedSeeds.clear();
    vSeeds.emplace_back("testseeds.litecoinca.sh");

    base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 127);
    base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
    base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1, 58);
    base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
    base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
    base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

    bech32_hrp = "tlcc";

    vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test,
                                         pnSeed6_test + ARRAYLEN(pnSeed6_test));

    fDefaultConsistencyChecks = false;
    fRequireStandard = false;
    fMineBlocksOnDemand = false;

    checkpointData = (CCheckpointData){{
        {0, uint256S("4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4"
                     "e3e29a0")},

        {10, uint256S("894eeba00e8837c2d46687960596f8781f47d6aeb27a94eafb923547"
                      "c053c2f8")},

        {412, uint256S("56d2eddb8cff67769e5c01eb30baa4897cc90c6c00f579a890adc8a"
                       "dfd608614")},

    }};

    chainTxData = ChainTxData{1707835909, 415, 0.001};
  }
};

class CRegTestParams : public CChainParams {
public:
  CRegTestParams() {
    strNetworkID = "regtest";
    consensus.nSubsidyHalvingInterval = 150;
    consensus.BIP16Height = 0;

    consensus.BIP34Height = 100000000;

    consensus.BIP34Hash = uint256();
    consensus.BIP65Height = 1351;

    consensus.BIP66Height = 1251;

    consensus.powLimit = uint256S(
        "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60;

    consensus.nPowTargetSpacing = 2.5 * 60;
    consensus.fPowAllowMinDifficultyBlocks = true;
    consensus.fPowNoRetargeting = true;
    consensus.nRuleChangeActivationThreshold = 108;

    consensus.nMinerConfirmationWindow = 144;

    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout =
        Consensus::BIP9Deployment::NO_TIMEOUT;
    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout =
        Consensus::BIP9Deployment::NO_TIMEOUT;
    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime =
        Consensus::BIP9Deployment::ALWAYS_ACTIVE;
    consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout =
        Consensus::BIP9Deployment::NO_TIMEOUT;

    consensus.powForkTime = 1543765622;

    consensus.lastScryptBlock = 200;

    consensus.powLimitSHA = uint256S(
        "000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    consensus.slowStartBlocks = 40;

    consensus.premineAmount = 550000;

    std::vector<unsigned char> vch =
        ParseHex("76a91424af51d38b740a6dc2868dfd70fc16d76901e1e088ac");
    consensus.premineOutputScript = CScript(vch.begin(), vch.end());

    consensus.totalMoneySupplyHeight = 6215968;

    consensus.hiveNonceMarker = 192;

    consensus.nMinimumChainWork = uint256S("0x00");

    consensus.defaultAssumeValid = uint256S("0x00");

    pchMessageStart[0] = 0xfa;
    pchMessageStart[1] = 0xbf;
    pchMessageStart[2] = 0xb5;
    pchMessageStart[3] = 0xda;
    nDefaultPort = 19444;
    nPruneAfterHeight = 1000;

    genesis = CreateGenesisBlock(1296688602, 0, 0x207fffff, 1,
                                 50 * COIN * COIN_SCALE);
    consensus.hashGenesisBlock = genesis.GetHash();
    assert(consensus.hashGenesisBlock ==
           uint256S("0x530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc"
                    "56c416f9"));
    assert(genesis.hashMerkleRoot ==
           uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011"
                    "edd4ced9"));

    vFixedSeeds.clear();

    vSeeds.clear();

    fDefaultConsistencyChecks = true;
    fRequireStandard = false;
    fMineBlocksOnDemand = true;

    checkpointData = {{{0, uint256S("530827f38f93b43ed12af0b3ad25a288dc02ed74d6"
                                    "d7857862df51fc56c416f9")}}};

    chainTxData = ChainTxData{0, 0, 0};

    base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 111);
    base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
    base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1, 58);
    base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
    base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
    base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

    bech32_hrp = "rlcc";
  }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
  assert(globalChainParams);
  return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string &chain) {
  if (chain == CBaseChainParams::MAIN)
    return std::unique_ptr<CChainParams>(new CMainParams());
  else if (chain == CBaseChainParams::TESTNET)
    return std::unique_ptr<CChainParams>(new CTestNetParams());
  else if (chain == CBaseChainParams::REGTEST)
    return std::unique_ptr<CChainParams>(new CRegTestParams());
  throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string &network) {
  SelectBaseParams(network);
  globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime,
                                 int64_t nTimeout) {
  globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
