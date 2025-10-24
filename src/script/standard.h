// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

#include <script/interpreter.h>
#include <uint256.h>

#include <boost/variant.hpp>

#include <stdint.h>

static const bool DEFAULT_ACCEPT_DATACARRIER = true;

class CKeyID;
class CScript;

class CScriptID : public uint160 {
public:
  CScriptID() : uint160() {}
  CScriptID(const CScript &in);
  CScriptID(const uint160 &in) : uint160(in) {}
};

static const unsigned int MAX_OP_RETURN_RELAY = 83;

extern bool fAcceptDatacarrier;

extern unsigned nMaxDatacarrierBytes;

static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_P2SH;

enum txnouttype {
  TX_NONSTANDARD,

  TX_PUBKEY,
  TX_PUBKEYHASH,
  TX_SCRIPTHASH,
  TX_MULTISIG,
  TX_NULL_DATA,

  TX_WITNESS_V0_SCRIPTHASH,
  TX_WITNESS_V0_KEYHASH,
  TX_WITNESS_UNKNOWN,

};

class CNoDestination {
public:
  friend bool operator==(const CNoDestination &a, const CNoDestination &b) {
    return true;
  }
  friend bool operator<(const CNoDestination &a, const CNoDestination &b) {
    return true;
  }
};

struct WitnessV0ScriptHash : public uint256 {
  WitnessV0ScriptHash() : uint256() {}
  explicit WitnessV0ScriptHash(const uint256 &hash) : uint256(hash) {}
  using uint256::uint256;
};

struct WitnessV0KeyHash : public uint160 {
  WitnessV0KeyHash() : uint160() {}
  explicit WitnessV0KeyHash(const uint160 &hash) : uint160(hash) {}
  using uint160::uint160;
};

struct WitnessUnknown {
  unsigned int version;
  unsigned int length;
  unsigned char program[40];

  friend bool operator==(const WitnessUnknown &w1, const WitnessUnknown &w2) {
    if (w1.version != w2.version)
      return false;
    if (w1.length != w2.length)
      return false;
    return std::equal(w1.program, w1.program + w1.length, w2.program);
  }

  friend bool operator<(const WitnessUnknown &w1, const WitnessUnknown &w2) {
    if (w1.version < w2.version)
      return true;
    if (w1.version > w2.version)
      return false;
    if (w1.length < w2.length)
      return true;
    if (w1.length > w2.length)
      return false;
    return std::lexicographical_compare(w1.program, w1.program + w1.length,
                                        w2.program, w2.program + w2.length);
  }
};

typedef boost::variant<CNoDestination, CKeyID, CScriptID, WitnessV0ScriptHash,
                       WitnessV0KeyHash, WitnessUnknown>
    CTxDestination;

bool IsValidDestination(const CTxDestination &dest);

const char *GetTxnOutputType(txnouttype t);

bool Solver(const CScript &scriptPubKey, txnouttype &typeRet,
            std::vector<std::vector<unsigned char>> &vSolutionsRet);

bool ExtractDestination(const CScript &scriptPubKey,
                        CTxDestination &addressRet);

bool ExtractDestinations(const CScript &scriptPubKey, txnouttype &typeRet,
                         std::vector<CTxDestination> &addressRet,
                         int &nRequiredRet);

CScript GetScriptForDestination(const CTxDestination &dest);

CScript GetScriptForRawPubKey(const CPubKey &pubkey);

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey> &keys);

CScript GetScriptForWitness(const CScript &redeemscript);

#endif
