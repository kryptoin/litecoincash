// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes
// only local node policy logic

#include <policy/policy.h>

#include <coins.h>
#include <consensus/validation.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validation.h>

#include <chainparams.h>

#include <base58.h>

CAmount GetDustThreshold(const CTxOut &txout, const CFeeRate &dustRelayFeeIn) {
  if (txout.scriptPubKey.IsUnspendable())
    return 0;

  size_t nSize = GetSerializeSize(txout, SER_DISK, 0);
  int witnessversion = 0;
  std::vector<unsigned char> witnessprogram;

  if (txout.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
    nSize += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
  } else {
    nSize += (32 + 4 + 1 + 107 + 4);
  }

  return dustRelayFeeIn.GetFee(nSize);
}

bool IsDust(const CTxOut &txout, const CFeeRate &dustRelayFeeIn) {
  return (txout.nValue < GetDustThreshold(txout, dustRelayFeeIn));
}

bool IsStandard(const CScript &scriptPubKey, txnouttype &whichType,
                const bool witnessEnabled) {
  std::vector<std::vector<unsigned char>> vSolutions;
  if (!Solver(scriptPubKey, whichType, vSolutions))
    return false;

  if (whichType == TX_MULTISIG) {
    unsigned char m = vSolutions.front()[0];
    unsigned char n = vSolutions.back()[0];

    if (n < 1 || n > 3)
      return false;
    if (m < 1 || m > n)
      return false;
  } else if (whichType == TX_NULL_DATA &&
             (!fAcceptDatacarrier ||
              scriptPubKey.size() > nMaxDatacarrierBytes))
    return false;

  else if (!witnessEnabled && (whichType == TX_WITNESS_V0_KEYHASH ||
                               whichType == TX_WITNESS_V0_SCRIPTHASH))
    return false;

  return whichType != TX_NONSTANDARD && whichType != TX_WITNESS_UNKNOWN;
}

bool IsStandardTx(const CTransaction &tx, std::string &reason,
                  const bool witnessEnabled) {
  if (tx.nVersion > CTransaction::MAX_STANDARD_VERSION || tx.nVersion < 1) {
    reason = "version";
    return false;
  }

  unsigned int sz = GetTransactionWeight(tx);
  if (sz >= MAX_STANDARD_TX_WEIGHT) {
    reason = "tx-size";
    return false;
  }

  for (const CTxIn &txin : tx.vin) {
    if (txin.scriptSig.size() > 1650) {
      reason = "scriptsig-size";
      return false;
    }
    if (!txin.scriptSig.IsPushOnly()) {
      reason = "scriptsig-not-pushonly";
      return false;
    }
  }

  unsigned int nDataOut = 0;
  txnouttype whichType;

  const Consensus::Params &consensusParams = Params().GetConsensus();

  CScript scriptPubKeyBCF = GetScriptForDestination(
      DecodeDestination(consensusParams.beeCreationAddress));

  for (const CTxOut &txout : tx.vout) {
    if (CScript::IsBCTScript(txout.scriptPubKey, scriptPubKeyBCF))

      return true;

    if (!::IsStandard(txout.scriptPubKey, whichType, witnessEnabled)) {
      reason = "scriptpubkey";
      return false;
    }

    if (whichType == TX_NULL_DATA)
      nDataOut++;
    else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
      reason = "bare-multisig";
      return false;
    } else if (IsDust(txout, ::dustRelayFee)) {
      reason = "dust";
      return false;
    }
  }

  if (nDataOut > 1) {
    reason = "multi-op-return";
    return false;
  }

  return true;
}

bool AreInputsStandard(const CTransaction &tx,
                       const CCoinsViewCache &mapInputs) {
  if (tx.IsCoinBase())
    return true;

  for (unsigned int i = 0; i < tx.vin.size(); i++) {
    const CTxOut &prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

    std::vector<std::vector<unsigned char>> vSolutions;
    txnouttype whichType;

    const CScript &prevScript = prev.scriptPubKey;
    if (!Solver(prevScript, whichType, vSolutions))
      return false;

    if (whichType == TX_SCRIPTHASH) {
      std::vector<std::vector<unsigned char>> stack;

      if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE,
                      BaseSignatureChecker(), SIGVERSION_BASE))
        return false;
      if (stack.empty())
        return false;
      CScript subscript(stack.back().begin(), stack.back().end());
      if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS) {
        return false;
      }
    }
  }

  return true;
}

bool IsWitnessStandard(const CTransaction &tx,
                       const CCoinsViewCache &mapInputs) {
  if (tx.IsCoinBase())
    return true;

  for (unsigned int i = 0; i < tx.vin.size(); i++) {
    if (tx.vin[i].scriptWitness.IsNull())
      continue;

    const CTxOut &prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

    CScript prevScript = prev.scriptPubKey;

    if (prevScript.IsPayToScriptHash()) {
      std::vector<std::vector<unsigned char>> stack;

      if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE,
                      BaseSignatureChecker(), SIGVERSION_BASE))
        return false;
      if (stack.empty())
        return false;
      prevScript = CScript(stack.back().begin(), stack.back().end());
    }

    int witnessversion = 0;
    std::vector<unsigned char> witnessprogram;

    if (!prevScript.IsWitnessProgram(witnessversion, witnessprogram))
      return false;

    if (witnessversion == 0 && witnessprogram.size() == 32) {
      if (tx.vin[i].scriptWitness.stack.back().size() >
          MAX_STANDARD_P2WSH_SCRIPT_SIZE)
        return false;
      size_t sizeWitnessStack = tx.vin[i].scriptWitness.stack.size() - 1;
      if (sizeWitnessStack > MAX_STANDARD_P2WSH_STACK_ITEMS)
        return false;
      for (unsigned int j = 0; j < sizeWitnessStack; j++) {
        if (tx.vin[i].scriptWitness.stack[j].size() >
            MAX_STANDARD_P2WSH_STACK_ITEM_SIZE)
          return false;
      }
    }
  }
  return true;
}

CFeeRate incrementalRelayFee = CFeeRate(DEFAULT_INCREMENTAL_RELAY_FEE);
CFeeRate dustRelayFee = CFeeRate(DUST_RELAY_TX_FEE);
unsigned int nBytesPerSigOp = DEFAULT_BYTES_PER_SIGOP;

int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost) {
  return (std::max(nWeight, nSigOpCost * nBytesPerSigOp) +
          WITNESS_SCALE_FACTOR - 1) /
         WITNESS_SCALE_FACTOR;
}

int64_t GetVirtualTransactionSize(const CTransaction &tx, int64_t nSigOpCost) {
  return GetVirtualTransactionSize(GetTransactionWeight(tx), nSigOpCost);
}
