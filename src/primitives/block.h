// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <crypto/minotaurx/yespower/yespower.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

const uint256 HIGH_HASH = uint256S(
    "0x0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

const std::string DEFAULT_POW_TYPE = "sha256d";

const std::string POW_TYPE_NAMES[] = {"sha256d", "minotaurx"};

enum POW_TYPE {
  POW_TYPE_SHA256,
  POW_TYPE_MINOTAURX,

  NUM_BLOCK_TYPES
};

class CBlockHeader {
public:
  int32_t nVersion;
  uint256 hashPrevBlock;
  uint256 hashMerkleRoot;
  uint32_t nTime;
  uint32_t nBits;
  uint32_t nNonce;

  CBlockHeader() { SetNull(); }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(this->nVersion);
    READWRITE(hashPrevBlock);
    READWRITE(hashMerkleRoot);
    READWRITE(nTime);
    READWRITE(nBits);
    READWRITE(nNonce);
  }

  void SetNull() {
    nVersion = 0;
    hashPrevBlock.SetNull();
    hashMerkleRoot.SetNull();
    nTime = 0;
    nBits = 0;
    nNonce = 0;
  }

  bool IsNull() const { return (nBits == 0); }

  uint256 GetHash() const;

  uint256 GetPoWHash() const;

  static uint256 MinotaurHashArbitrary(const char *data);

  static uint256 MinotaurHashString(std::string data);

  int64_t GetBlockTime() const { return (int64_t)nTime; }

  bool IsHiveMined(const Consensus::Params &consensusParams) const {
    return (nNonce == consensusParams.hiveNonceMarker);
  }

  POW_TYPE GetPoWType() const { return (POW_TYPE)((nVersion >> 16) & 0xFF); }

  std::string GetPoWTypeName() const {
    if (nVersion >= 0x20000000)
      return POW_TYPE_NAMES[0];

    POW_TYPE pt = GetPoWType();
    if (pt >= NUM_BLOCK_TYPES)
      return "unrecognised";
    return POW_TYPE_NAMES[pt];
  }
};

class CBlock : public CBlockHeader {
public:
  std::vector<CTransactionRef> vtx;

  mutable bool fChecked;

  CBlock() { SetNull(); }

  CBlock(const CBlockHeader &header) {
    SetNull();
    *((CBlockHeader *)this) = header;
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(*(CBlockHeader *)this);
    READWRITE(vtx);
  }

  void SetNull() {
    CBlockHeader::SetNull();
    vtx.clear();
    fChecked = false;
  }

  CBlockHeader GetBlockHeader() const {
    CBlockHeader block;
    block.nVersion = nVersion;
    block.hashPrevBlock = hashPrevBlock;
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime = nTime;
    block.nBits = nBits;
    block.nNonce = nNonce;
    return block;
  }

  std::string ToString() const;
};

struct CBlockLocator {
  std::vector<uint256> vHave;

  CBlockLocator() {}

  explicit CBlockLocator(const std::vector<uint256> &vHaveIn)
      : vHave(vHaveIn) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(nVersion);
    READWRITE(vHave);
  }

  void SetNull() { vHave.clear(); }

  bool IsNull() const { return vHave.empty(); }
};

#endif
