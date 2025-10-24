// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAIN_H
#define BITCOIN_CHAIN_H

#include <arith_uint256.h>
#include <pow.h>
#include <primitives/block.h>
#include <tinyformat.h>
#include <uint256.h>

#include <vector>

static const int64_t MAX_FUTURE_BLOCK_TIME = 2 * 60 * 60;
static const int64_t MAX_FUTURE_BLOCK_TIME_MINOTAURX = (90 * 5 * 60) / 20;

static const int64_t TIMESTAMP_WINDOW = MAX_FUTURE_BLOCK_TIME;

class CBlockFileInfo {
public:
  unsigned int nBlocks;

  unsigned int nSize;

  unsigned int nUndoSize;

  unsigned int nHeightFirst;

  unsigned int nHeightLast;

  uint64_t nTimeFirst;

  uint64_t nTimeLast;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(VARINT(nBlocks));
    READWRITE(VARINT(nSize));
    READWRITE(VARINT(nUndoSize));
    READWRITE(VARINT(nHeightFirst));
    READWRITE(VARINT(nHeightLast));
    READWRITE(VARINT(nTimeFirst));
    READWRITE(VARINT(nTimeLast));
  }

  void SetNull() {
    nBlocks = 0;
    nSize = 0;
    nUndoSize = 0;
    nHeightFirst = 0;
    nHeightLast = 0;
    nTimeFirst = 0;
    nTimeLast = 0;
  }

  CBlockFileInfo() { SetNull(); }

  std::string ToString() const;

  void AddBlock(unsigned int nHeightIn, uint64_t nTimeIn) {
    if (nBlocks == 0 || nHeightFirst > nHeightIn)
      nHeightFirst = nHeightIn;
    if (nBlocks == 0 || nTimeFirst > nTimeIn)
      nTimeFirst = nTimeIn;
    nBlocks++;
    if (nHeightIn > nHeightLast)
      nHeightLast = nHeightIn;
    if (nTimeIn > nTimeLast)
      nTimeLast = nTimeIn;
  }
};

struct CDiskBlockPos {
  int nFile;
  unsigned int nPos;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(VARINT(nFile));
    READWRITE(VARINT(nPos));
  }

  CDiskBlockPos() { SetNull(); }

  CDiskBlockPos(int nFileIn, unsigned int nPosIn) {
    nFile = nFileIn;
    nPos = nPosIn;
  }

  friend bool operator==(const CDiskBlockPos &a, const CDiskBlockPos &b) {
    return (a.nFile == b.nFile && a.nPos == b.nPos);
  }

  friend bool operator!=(const CDiskBlockPos &a, const CDiskBlockPos &b) {
    return !(a == b);
  }

  void SetNull() {
    nFile = -1;
    nPos = 0;
  }
  bool IsNull() const { return (nFile == -1); }

  std::string ToString() const {
    return strprintf("CBlockDiskPos(nFile=%i, nPos=%i)", nFile, nPos);
  }
};

enum BlockStatus : uint32_t {
  BLOCK_VALID_UNKNOWN = 0,

  BLOCK_VALID_HEADER = 1,

  BLOCK_VALID_TREE = 2,

  BLOCK_VALID_TRANSACTIONS = 3,

  BLOCK_VALID_CHAIN = 4,

  BLOCK_VALID_SCRIPTS = 5,

  BLOCK_VALID_MASK = BLOCK_VALID_HEADER | BLOCK_VALID_TREE |
                     BLOCK_VALID_TRANSACTIONS | BLOCK_VALID_CHAIN |
                     BLOCK_VALID_SCRIPTS,

  BLOCK_HAVE_DATA = 8,

  BLOCK_HAVE_UNDO = 16,

  BLOCK_HAVE_MASK = BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,

  BLOCK_FAILED_VALID = 32,

  BLOCK_FAILED_CHILD = 64,

  BLOCK_FAILED_MASK = BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,

  BLOCK_OPT_WITNESS = 128,

};

class CBlockIndex {
public:
  arith_uint256 nChainWork;
  CBlockIndex *pprev;
  CBlockIndex *pskip;

  const uint256 *phashBlock;
  int nFile;
  int nHeight;

  int32_t nSequenceId;
  int32_t nVersion;

  uint256 hashMerkleRoot;

  uint32_t nBits;
  uint32_t nNonce;
  uint32_t nStatus;
  uint32_t nTime;

  unsigned int nChainTx;
  unsigned int nDataPos;
  unsigned int nTimeMax;
  unsigned int nTx;
  unsigned int nUndoPos;

  void SetNull() {
    phashBlock = nullptr;
    pprev = nullptr;
    pskip = nullptr;
    nHeight = 0;
    nFile = 0;
    nDataPos = 0;
    nUndoPos = 0;
    nChainWork = arith_uint256();
    nTx = 0;
    nChainTx = 0;
    nStatus = 0;
    nSequenceId = 0;
    nTimeMax = 0;

    nVersion = 0;
    hashMerkleRoot = uint256();
    nTime = 0;
    nBits = 0;
    nNonce = 0;
  }

  CBlockIndex() { SetNull(); }

  explicit CBlockIndex(const CBlockHeader &block) {
    SetNull();

    nVersion = block.nVersion;
    hashMerkleRoot = block.hashMerkleRoot;
    nTime = block.nTime;
    nBits = block.nBits;
    nNonce = block.nNonce;
  }

  CDiskBlockPos GetBlockPos() const {
    CDiskBlockPos ret;
    if (nStatus & BLOCK_HAVE_DATA) {
      ret.nFile = nFile;
      ret.nPos = nDataPos;
    }
    return ret;
  }

  CDiskBlockPos GetUndoPos() const {
    CDiskBlockPos ret;
    if (nStatus & BLOCK_HAVE_UNDO) {
      ret.nFile = nFile;
      ret.nPos = nUndoPos;
    }
    return ret;
  }

  CBlockHeader GetBlockHeader() const {
    CBlockHeader block;
    block.nVersion = nVersion;
    if (pprev)
      block.hashPrevBlock = pprev->GetBlockHash();
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime = nTime;
    block.nBits = nBits;
    block.nNonce = nNonce;
    return block;
  }

  uint256 GetBlockHash() const { return *phashBlock; }

  uint256 GetBlockPoWHash() const { return GetBlockHeader().GetPoWHash(); }

  int64_t GetBlockTime() const { return (int64_t)nTime; }

  int64_t GetBlockTimeMax() const { return (int64_t)nTimeMax; }

  static constexpr int nMedianTimeSpan = 11;

  int64_t GetMedianTimePast() const {
    int64_t pmedian[nMedianTimeSpan];
    int64_t *pbegin = &pmedian[nMedianTimeSpan];
    int64_t *pend = &pmedian[nMedianTimeSpan];

    const CBlockIndex *pindex = this;
    for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
      *(--pbegin) = pindex->GetBlockTime();

    std::sort(pbegin, pend);
    return pbegin[(pend - pbegin) / 2];
  }

  std::string ToString() const {
    return strprintf(
        "CBlockIndex(pprev=%p, nHeight=%d, merkle=%s, hashBlock=%s)", pprev,
        nHeight, hashMerkleRoot.ToString(), GetBlockHash().ToString());
  }

  bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const {
    assert(!(nUpTo & ~BLOCK_VALID_MASK));

    if (nStatus & BLOCK_FAILED_MASK)
      return false;
    return ((nStatus & BLOCK_VALID_MASK) >= nUpTo);
  }

  bool RaiseValidity(enum BlockStatus nUpTo) {
    assert(!(nUpTo & ~BLOCK_VALID_MASK));

    if (nStatus & BLOCK_FAILED_MASK)
      return false;
    if ((nStatus & BLOCK_VALID_MASK) < nUpTo) {
      nStatus = (nStatus & ~BLOCK_VALID_MASK) | nUpTo;
      return true;
    }
    return false;
  }

  void BuildSkip();

  CBlockIndex *GetAncestor(int height);
  const CBlockIndex *GetAncestor(int height) const;
};

arith_uint256 GetBlockProof(const CBlockIndex &block);

arith_uint256 GetNumHashes(const CBlockIndex &block, POW_TYPE powType);

int64_t GetBlockProofEquivalentTime(const CBlockIndex &to,
                                    const CBlockIndex &from,
                                    const CBlockIndex &tip,
                                    const Consensus::Params &);

const CBlockIndex *LastCommonAncestor(const CBlockIndex *pa,
                                      const CBlockIndex *pb);

class CDiskBlockIndex : public CBlockIndex {
public:
  uint256 hashPrev;

  CDiskBlockIndex() { hashPrev = uint256(); }

  explicit CDiskBlockIndex(const CBlockIndex *pindex) : CBlockIndex(*pindex) {
    hashPrev = (pprev ? pprev->GetBlockHash() : uint256());
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    int _nVersion = s.GetVersion();
    if (!(s.GetType() & SER_GETHASH))
      READWRITE(VARINT(_nVersion));

    READWRITE(VARINT(nHeight));
    READWRITE(VARINT(nStatus));
    READWRITE(VARINT(nTx));
    if (nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO))
      READWRITE(VARINT(nFile));
    if (nStatus & BLOCK_HAVE_DATA)
      READWRITE(VARINT(nDataPos));
    if (nStatus & BLOCK_HAVE_UNDO)
      READWRITE(VARINT(nUndoPos));

    READWRITE(this->nVersion);
    READWRITE(hashPrev);
    READWRITE(hashMerkleRoot);
    READWRITE(nTime);
    READWRITE(nBits);
    READWRITE(nNonce);
  }

  uint256 GetBlockHash() const {
    CBlockHeader block;
    block.nVersion = nVersion;
    block.hashPrevBlock = hashPrev;
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime = nTime;
    block.nBits = nBits;
    block.nNonce = nNonce;
    return block.GetHash();
  }

  std::string ToString() const {
    std::string str = "CDiskBlockIndex(";
    str += CBlockIndex::ToString();
    str += strprintf("\n                hashBlock=%s, hashPrev=%s)",
                     GetBlockHash().ToString(), hashPrev.ToString());
    return str;
  }
};

class CChain {
private:
  std::vector<CBlockIndex *> vChain;

public:
  CBlockIndex *Genesis() const {
    return vChain.size() > 0 ? vChain[0] : nullptr;
  }

  CBlockIndex *Tip() const {
    return vChain.size() > 0 ? vChain[vChain.size() - 1] : nullptr;
  }

  CBlockIndex *operator[](int nHeight) const {
    if (nHeight < 0 || nHeight >= (int)vChain.size())
      return nullptr;
    return vChain[nHeight];
  }

  friend bool operator==(const CChain &a, const CChain &b) {
    return a.vChain.size() == b.vChain.size() &&
           a.vChain[a.vChain.size() - 1] == b.vChain[b.vChain.size() - 1];
  }

  bool Contains(const CBlockIndex *pindex) const {
    return (*this)[pindex->nHeight] == pindex;
  }

  CBlockIndex *Next(const CBlockIndex *pindex) const {
    if (Contains(pindex))
      return (*this)[pindex->nHeight + 1];
    else
      return nullptr;
  }

  int Height() const { return vChain.size() - 1; }

  void SetTip(CBlockIndex *pindex);

  CBlockLocator GetLocator(const CBlockIndex *pindex = nullptr) const;

  const CBlockIndex *FindFork(const CBlockIndex *pindex) const;

  CBlockIndex *FindEarliestAtLeast(int64_t nTime) const;
};

#endif
