// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <core_io.h>
#include <key.h>
#include <keystore.h>
#include <miner.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <random.h>
#include <script/sign.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>
#include <txmempool.h>
#include <utiltime.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

bool CheckInputs(const CTransaction &tx, CValidationState &state,
                 const CCoinsViewCache &inputs, bool fScriptChecks,
                 unsigned int flags, bool cacheSigStore,
                 bool cacheFullScriptStore, PrecomputedTransactionData &txdata,
                 std::vector<CScriptCheck> *pvChecks);

BOOST_AUTO_TEST_SUITE(tx_validationcache_tests)

static bool ToMemPool(CMutableTransaction &tx) {
  LOCK(cs_main);

  CValidationState state;
  return AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), nullptr,
                            nullptr, true, 0);
}

BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend, TestChain100Setup) {
  CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey())
                                   << OP_CHECKSIG;

  std::vector<CMutableTransaction> spends;
  spends.resize(2);
  for (int i = 0; i < 2; i++) {
    spends[i].nVersion = 1;
    spends[i].vin.resize(1);
    spends[i].vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    spends[i].vin[0].prevout.n = 0;
    spends[i].vout.resize(1);
    spends[i].vout[0].nValue = 11 * CENT;
    spends[i].vout[0].scriptPubKey = scriptPubKey;

    std::vector<unsigned char> vchSig;
    uint256 hash =
        SignatureHash(scriptPubKey, spends[i], 0, SIGHASH_ALL | SIGHASH_FORKID,
                      0, SIGVERSION_BASE);

    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL | SIGHASH_FORKID);

    spends[i].vin[0].scriptSig << vchSig;
  }

  CBlock block;

  block = CreateAndProcessBlock(spends, scriptPubKey);
  BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());

  BOOST_CHECK(ToMemPool(spends[0]));
  block = CreateAndProcessBlock(spends, scriptPubKey);
  BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
  mempool.clear();

  BOOST_CHECK(ToMemPool(spends[1]));
  block = CreateAndProcessBlock(spends, scriptPubKey);
  BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
  mempool.clear();

  std::vector<CMutableTransaction> oneSpend;
  oneSpend.push_back(spends[0]);
  BOOST_CHECK(ToMemPool(spends[1]));
  block = CreateAndProcessBlock(oneSpend, scriptPubKey);
  BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());

  BOOST_CHECK_EQUAL(mempool.size(), 0);
}

void ValidateCheckInputsForAllFlags(CMutableTransaction &tx,
                                    uint32_t failing_flags, bool add_to_cache) {
  PrecomputedTransactionData txdata(tx);

  for (uint32_t test_flags = 0; test_flags < (1U << 16); test_flags += 1) {
    CValidationState state;

    if ((test_flags & SCRIPT_VERIFY_CLEANSTACK)) {
      test_flags |= SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
    }
    if ((test_flags & SCRIPT_VERIFY_WITNESS)) {
      test_flags |= SCRIPT_VERIFY_P2SH;
    }
    bool ret = CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true,
                           add_to_cache, txdata, nullptr);

    bool expected_return_value = !(test_flags & failing_flags);
    BOOST_CHECK_EQUAL(ret, expected_return_value);

    if (ret && add_to_cache) {
      std::vector<CScriptCheck> scriptchecks;
      BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags,
                              true, add_to_cache, txdata, &scriptchecks));
      BOOST_CHECK(scriptchecks.empty());
    } else {
      std::vector<CScriptCheck> scriptchecks;
      BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags,
                              true, add_to_cache, txdata, &scriptchecks));
      BOOST_CHECK_EQUAL(scriptchecks.size(), tx.vin.size());
    }
  }
}

BOOST_FIXTURE_TEST_CASE(checkinputs_test, TestChain100Setup) {
  {
    LOCK(cs_main);
    InitScriptExecutionCache();
  }

  CScript p2pk_scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey())
                                        << OP_CHECKSIG;
  CScript p2sh_scriptPubKey =
      GetScriptForDestination(CScriptID(p2pk_scriptPubKey));
  CScript p2pkh_scriptPubKey =
      GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
  CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

  CBasicKeyStore keystore;
  keystore.AddKey(coinbaseKey);
  keystore.AddCScript(p2pk_scriptPubKey);

  CMutableTransaction spend_tx;

  spend_tx.nVersion = 1;
  spend_tx.vin.resize(1);
  spend_tx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
  spend_tx.vin[0].prevout.n = 0;
  spend_tx.vout.resize(4);
  spend_tx.vout[0].nValue = 11 * CENT;
  spend_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
  spend_tx.vout[1].nValue = 11 * CENT;
  spend_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
  spend_tx.vout[2].nValue = 11 * CENT;
  spend_tx.vout[2].scriptPubKey =
      CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
  spend_tx.vout[3].nValue = 11 * CENT;
  spend_tx.vout[3].scriptPubKey =
      CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP
                << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

  {
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(p2pk_scriptPubKey, spend_tx, 0, SIGHASH_ALL, 0,
                                 SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)0);

    vchSig.push_back((unsigned char)SIGHASH_ALL);
    spend_tx.vin[0].scriptSig << vchSig;
  }

  {
    LOCK(cs_main);

    CValidationState state;
    PrecomputedTransactionData ptd_spend_tx(spend_tx);

    BOOST_CHECK(!CheckInputs(spend_tx, state, pcoinsTip.get(), true,
                             SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true,
                             true, ptd_spend_tx, nullptr));

    std::vector<CScriptCheck> scriptchecks;
    BOOST_CHECK(CheckInputs(spend_tx, state, pcoinsTip.get(), true,
                            SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true,
                            true, ptd_spend_tx, &scriptchecks));
    BOOST_CHECK_EQUAL(scriptchecks.size(), 1);

    ValidateCheckInputsForAllFlags(spend_tx,
                                   SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S |
                                       SCRIPT_VERIFY_STRICTENC,
                                   false);
  }

  CBlock block;

  block = CreateAndProcessBlock({spend_tx}, p2pk_scriptPubKey);
  BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
  BOOST_CHECK(pcoinsTip->GetBestBlock() == block.GetHash());

  LOCK(cs_main);

  {
    CMutableTransaction invalid_under_p2sh_tx;
    invalid_under_p2sh_tx.nVersion = 1;
    invalid_under_p2sh_tx.vin.resize(1);
    invalid_under_p2sh_tx.vin[0].prevout.hash = spend_tx.GetHash();
    invalid_under_p2sh_tx.vin[0].prevout.n = 0;
    invalid_under_p2sh_tx.vout.resize(1);
    invalid_under_p2sh_tx.vout[0].nValue = 11 * CENT;
    invalid_under_p2sh_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;
    std::vector<unsigned char> vchSig2(p2pk_scriptPubKey.begin(),
                                       p2pk_scriptPubKey.end());
    invalid_under_p2sh_tx.vin[0].scriptSig << vchSig2;

    ValidateCheckInputsForAllFlags(invalid_under_p2sh_tx, SCRIPT_VERIFY_P2SH,
                                   true);
  }

  {
    CMutableTransaction invalid_with_cltv_tx;
    invalid_with_cltv_tx.nVersion = 1;
    invalid_with_cltv_tx.nLockTime = 100;
    invalid_with_cltv_tx.vin.resize(1);
    invalid_with_cltv_tx.vin[0].prevout.hash = spend_tx.GetHash();
    invalid_with_cltv_tx.vin[0].prevout.n = 2;
    invalid_with_cltv_tx.vin[0].nSequence = 0;
    invalid_with_cltv_tx.vout.resize(1);
    invalid_with_cltv_tx.vout[0].nValue = 11 * CENT;
    invalid_with_cltv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

    std::vector<unsigned char> vchSig;
    uint256 hash =
        SignatureHash(spend_tx.vout[2].scriptPubKey, invalid_with_cltv_tx, 0,
                      SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

    ValidateCheckInputsForAllFlags(invalid_with_cltv_tx,
                                   SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true);

    invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
    CValidationState state;
    PrecomputedTransactionData txdata(invalid_with_cltv_tx);
    BOOST_CHECK(CheckInputs(invalid_with_cltv_tx, state, pcoinsTip.get(), true,
                            SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true, true,
                            txdata, nullptr));
  }

  {
    CMutableTransaction invalid_with_csv_tx;
    invalid_with_csv_tx.nVersion = 2;
    invalid_with_csv_tx.vin.resize(1);
    invalid_with_csv_tx.vin[0].prevout.hash = spend_tx.GetHash();
    invalid_with_csv_tx.vin[0].prevout.n = 3;
    invalid_with_csv_tx.vin[0].nSequence = 100;
    invalid_with_csv_tx.vout.resize(1);
    invalid_with_csv_tx.vout[0].nValue = 11 * CENT;
    invalid_with_csv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

    std::vector<unsigned char> vchSig;
    uint256 hash =
        SignatureHash(spend_tx.vout[3].scriptPubKey, invalid_with_csv_tx, 0,
                      SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

    ValidateCheckInputsForAllFlags(invalid_with_csv_tx,
                                   SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true);

    invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
    CValidationState state;
    PrecomputedTransactionData txdata(invalid_with_csv_tx);
    BOOST_CHECK(CheckInputs(invalid_with_csv_tx, state, pcoinsTip.get(), true,
                            SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true, true,
                            txdata, nullptr));
  }

  {
    CMutableTransaction valid_with_witness_tx;
    valid_with_witness_tx.nVersion = 1;
    valid_with_witness_tx.vin.resize(1);
    valid_with_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
    valid_with_witness_tx.vin[0].prevout.n = 1;
    valid_with_witness_tx.vout.resize(1);
    valid_with_witness_tx.vout[0].nValue = 11 * CENT;
    valid_with_witness_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

    SignatureData sigdata;
    ProduceSignature(
        MutableTransactionSignatureCreator(&keystore, &valid_with_witness_tx, 0,
                                           11 * CENT, SIGHASH_ALL),
        spend_tx.vout[1].scriptPubKey, sigdata);
    UpdateTransaction(valid_with_witness_tx, 0, sigdata);

    ValidateCheckInputsForAllFlags(valid_with_witness_tx, 0, true);

    valid_with_witness_tx.vin[0].scriptWitness.SetNull();
    ValidateCheckInputsForAllFlags(valid_with_witness_tx, SCRIPT_VERIFY_WITNESS,
                                   true);
  }

  {
    CMutableTransaction tx;

    tx.nVersion = 1;
    tx.vin.resize(2);
    tx.vin[0].prevout.hash = spend_tx.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[1].prevout.hash = spend_tx.GetHash();
    tx.vin[1].prevout.n = 1;
    tx.vout.resize(1);
    tx.vout[0].nValue = 22 * CENT;
    tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

    for (int i = 0; i < 2; ++i) {
      SignatureData sigdata;
      ProduceSignature(MutableTransactionSignatureCreator(
                           &keystore, &tx, i, 11 * CENT, SIGHASH_ALL),
                       spend_tx.vout[i].scriptPubKey, sigdata);
      UpdateTransaction(tx, i, sigdata);
    }

    ValidateCheckInputsForAllFlags(tx, 0, true);

    tx.vin[1].scriptWitness.SetNull();

    CValidationState state;
    PrecomputedTransactionData txdata(tx);

    BOOST_CHECK(!CheckInputs(tx, state, pcoinsTip.get(), true,
                             SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true,
                             true, txdata, nullptr));

    std::vector<CScriptCheck> scriptchecks;

    BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true,
                            SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true,
                            true, txdata, &scriptchecks));

    BOOST_CHECK_EQUAL(scriptchecks.size(), 2);
  }
}

BOOST_AUTO_TEST_SUITE_END()
