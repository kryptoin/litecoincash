// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2025 The Litecoin Cash Core developers
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

  // Performance optimization cache (mutable for const methods)
  mutable std::vector<uint64_t> vDataFast;
  mutable bool fastCacheValid;
  mutable size_t setBitsCache;
  mutable bool setBitsCacheValid;

  void InvalidateFastCache() const;
  void UpdateFastCache() const;
  void InvalidateSetBitsCache() const;
  size_t CountSetBits() const;

  unsigned int Hash(unsigned int nHashNum,
                    const std::vector<unsigned char> &vDataToHash) const;

  CBloomFilter(const unsigned int nElements, const double nFPRate,
               const unsigned int nTweak);
  friend class CRollingBloomFilter;

public:
  CBloomFilter(const unsigned int nElements, const double nFPRate,
               const unsigned int nTweak, unsigned char nFlagsIn);
  CBloomFilter()
      : isFull(true), isEmpty(false), nHashFuncs(0), nTweak(0), nFlags(0),
        fastCacheValid(false), setBitsCacheValid(false) {}

  // Copy semantics
  CBloomFilter(const CBloomFilter &other);
  CBloomFilter &operator=(const CBloomFilter &other);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(vData);
    READWRITE(nHashFuncs);
    READWRITE(nTweak);
    READWRITE(nFlags);
    // Invalidate caches after deserialization
    if (ser_action.ForRead()) {
      InvalidateFastCache();
      InvalidateSetBitsCache();
    }
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

  // Additional utility methods (optional - can be removed if not needed)
  double GetCurrentFPRate() const;
  size_t EstimateElementCount() const;
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

  // Hash computation cache for performance
  mutable std::vector<uint32_t> hashCache;
  mutable std::vector<unsigned char> lastHashedKey;
  mutable bool hashCacheValid;

  void InvalidateHashCache() const;
  void ComputeHashCache(const std::vector<unsigned char> &vKey) const;
};

#endif