// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <key.h>
#if defined(HAVE_CONSENSUS_LIB)
#include <script/bitcoinconsensus.h>
#endif
#include <script/script.h>
#include <script/sign.h>
#include <streams.h>

#include <array>

static CMutableTransaction
BuildCreditingTransaction(const CScript &scriptPubKey) {
  CMutableTransaction txCredit;
  txCredit.nVersion = 1;
  txCredit.nLockTime = 0;
  txCredit.vin.resize(1);
  txCredit.vout.resize(1);
  txCredit.vin[0].prevout.SetNull();
  txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
  txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
  txCredit.vout[0].scriptPubKey = scriptPubKey;
  txCredit.vout[0].nValue = 1;

  return txCredit;
}

static CMutableTransaction
BuildSpendingTransaction(const CScript &scriptSig,
                         const CMutableTransaction &txCredit) {
  CMutableTransaction txSpend;
  txSpend.nVersion = 1;
  txSpend.nLockTime = 0;
  txSpend.vin.resize(1);
  txSpend.vout.resize(1);
  txSpend.vin[0].prevout.hash = txCredit.GetHash();
  txSpend.vin[0].prevout.n = 0;
  txSpend.vin[0].scriptSig = scriptSig;
  txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
  txSpend.vout[0].scriptPubKey = CScript();
  txSpend.vout[0].nValue = txCredit.vout[0].nValue;

  return txSpend;
}

static void VerifyScriptBench(benchmark::State &state) {
  const int flags = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_P2SH;
  const int witnessversion = 0;

  CKey key;
  static const std::array<unsigned char, 32> vchKey = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  key.Set(vchKey.begin(), vchKey.end(), false);
  CPubKey pubkey = key.GetPubKey();
  uint160 pubkeyHash;
  CHash160().Write(pubkey.begin(), pubkey.size()).Finalize(pubkeyHash.begin());

  CScript scriptPubKey = CScript()
                         << witnessversion << ToByteVector(pubkeyHash);
  CScript scriptSig;
  CScript witScriptPubkey = CScript()
                            << OP_DUP << OP_HASH160 << ToByteVector(pubkeyHash)
                            << OP_EQUALVERIFY << OP_CHECKSIG;
  CTransaction txCredit = BuildCreditingTransaction(scriptPubKey);
  CMutableTransaction txSpend = BuildSpendingTransaction(scriptSig, txCredit);
  CScriptWitness &witness = txSpend.vin[0].scriptWitness;
  witness.stack.emplace_back();
  key.Sign(SignatureHash(witScriptPubkey, txSpend, 0, SIGHASH_ALL,
                         txCredit.vout[0].nValue, SIGVERSION_WITNESS_V0),
           witness.stack.back(), 0);
  witness.stack.back().push_back(static_cast<unsigned char>(SIGHASH_ALL));
  witness.stack.push_back(ToByteVector(pubkey));

  while (state.KeepRunning()) {
    ScriptError err;
    bool success =
        VerifyScript(txSpend.vin[0].scriptSig, txCredit.vout[0].scriptPubKey,
                     &txSpend.vin[0].scriptWitness, flags,
                     MutableTransactionSignatureChecker(
                         &txSpend, 0, txCredit.vout[0].nValue),
                     &err);
    assert(err == SCRIPT_ERR_OK);
    assert(success);

#if defined(HAVE_CONSENSUS_LIB)
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << txSpend;
    int csuccess = bitcoinconsensus_verify_script_with_amount(
        txCredit.vout[0].scriptPubKey.data(),
        txCredit.vout[0].scriptPubKey.size(), txCredit.vout[0].nValue,
        (const unsigned char *)stream.data(), stream.size(), 0, flags, nullptr);
    assert(csuccess == 1);
#endif
  }
}

BENCHMARK(VerifyScriptBench, 6300);
