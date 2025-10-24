// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MERKLEBLOCK_H
#define BITCOIN_MERKLEBLOCK_H

#include <bloom.h>
#include <primitives/block.h>
#include <serialize.h>
#include <uint256.h>

#include <vector>

class CPartialMerkleTree {
protected:
  unsigned int nTransactions;

  std::vector<bool> vBits;

  std::vector<uint256> vHash;

  bool fBad;

  unsigned int CalcTreeWidth(int height) const {
    return (nTransactions + (1 << height) - 1) >> height;
  }

  uint256 CalcHash(int height, unsigned int pos,
                   const std::vector<uint256> &vTxid);

  void TraverseAndBuild(int height, unsigned int pos,
                        const std::vector<uint256> &vTxid,
                        const std::vector<bool> &vMatch);

  uint256 TraverseAndExtract(int height, unsigned int pos,
                             unsigned int &nBitsUsed, unsigned int &nHashUsed,
                             std::vector<uint256> &vMatch,
                             std::vector<unsigned int> &vnIndex);

public:
  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(nTransactions);
    READWRITE(vHash);
    std::vector<unsigned char> vBytes;
    if (ser_action.ForRead()) {
      READWRITE(vBytes);
      CPartialMerkleTree &us = *(const_cast<CPartialMerkleTree *>(this));
      us.vBits.resize(vBytes.size() * 8);
      for (unsigned int p = 0; p < us.vBits.size(); p++)
        us.vBits[p] = (vBytes[p / 8] & (1 << (p % 8))) != 0;
      us.fBad = false;
    } else {
      vBytes.resize((vBits.size() + 7) / 8);
      for (unsigned int p = 0; p < vBits.size(); p++)
        vBytes[p / 8] |= vBits[p] << (p % 8);
      READWRITE(vBytes);
    }
  }

  CPartialMerkleTree(const std::vector<uint256> &vTxid,
                     const std::vector<bool> &vMatch);

  CPartialMerkleTree();

  uint256 ExtractMatches(std::vector<uint256> &vMatch,
                         std::vector<unsigned int> &vnIndex);
};

class CMerkleBlock {
public:
  CBlockHeader header;
  CPartialMerkleTree txn;

  std::vector<std::pair<unsigned int, uint256>> vMatchedTxn;

  CMerkleBlock(const CBlock &block, CBloomFilter &filter)
      : CMerkleBlock(block, &filter, nullptr) {}

  CMerkleBlock(const CBlock &block, const std::set<uint256> &txids)
      : CMerkleBlock(block, nullptr, &txids) {}

  CMerkleBlock() {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(header);
    READWRITE(txn);
  }

private:
  CMerkleBlock(const CBlock &block, CBloomFilter *filter,
               const std::set<uint256> *txids);
};

#endif
