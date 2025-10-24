// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BLOOM_H
#define BITCOIN_BLOOM_H

#include <serialize.h>

#include <vector>

class COutPoint;
class CTransaction;
class uint256;

static const unsigned int MAX_BLOOM_FILTER_SIZE = 36000;

static const unsigned int MAX_HASH_FUNCS = 50;

enum bloomflags {
  BLOOM_UPDATE_NONE = 0,
  BLOOM_UPDATE_ALL = 1,

  BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
  BLOOM_UPDATE_MASK = 3,
};

class CBloomFilter {
private:
  std::vector<unsigned char> vData;
  bool isFull;
  bool isEmpty;
  unsigned int nHashFuncs;
  unsigned int nTweak;
  unsigned char nFlags;

  unsigned int Hash(unsigned int nHashNum,
                    const std::vector<unsigned char> &vDataToHash) const;

  CBloomFilter(const unsigned int nElements, const double nFPRate,
               const unsigned int nTweak);
  friend class CRollingBloomFilter;

public:
  CBloomFilter(const unsigned int nElements, const double nFPRate,
               const unsigned int nTweak, unsigned char nFlagsIn);
  CBloomFilter()
      : isFull(true), isEmpty(false), nHashFuncs(0), nTweak(0), nFlags(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(vData);
    READWRITE(nHashFuncs);
    READWRITE(nTweak);
    READWRITE(nFlags);
  }

  void insert(const std::vector<unsigned char> &vKey);
  void insert(const COutPoint &outpoint);
  void insert(const uint256 &hash);

  bool contains(const std::vector<unsigned char> &vKey) const;
  bool contains(const COutPoint &outpoint) const;
  bool contains(const uint256 &hash) const;

  void clear();
  void reset(const unsigned int nNewTweak);

  bool IsWithinSizeConstraints() const;

  bool IsRelevantAndUpdate(const CTransaction &tx);

  void UpdateEmptyFull();
};

class CRollingBloomFilter {
public:
  CRollingBloomFilter(const unsigned int nElements, const double nFPRate);

  void insert(const std::vector<unsigned char> &vKey);
  void insert(const uint256 &hash);
  bool contains(const std::vector<unsigned char> &vKey) const;
  bool contains(const uint256 &hash) const;

  void reset();

private:
  int nEntriesPerGeneration;
  int nEntriesThisGeneration;
  int nGeneration;
  std::vector<uint64_t> data;
  unsigned int nTweak;
  int nHashFuncs;
};

#endif
