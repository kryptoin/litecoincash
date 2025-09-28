// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bloom.h>

#include <hash.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/script.h>
#include <script/standard.h>
#include <streams.h>

#include <algorithm>
#include <math.h>
#include <stdlib.h>

#define LN2SQUARED 0.4804530139182014246671025263266649717305529515945455
#define LN2 0.6931471805599453094172321214581765680755001343602552

CBloomFilter::CBloomFilter(const unsigned int nElements, const double nFPRate,
                           const unsigned int nTweakIn, unsigned char nFlagsIn)
    : vData(std::min((unsigned int)(-1 / LN2SQUARED * nElements * log(nFPRate)),
                     MAX_BLOOM_FILTER_SIZE * 8) /
            8),
      isFull(false), isEmpty(true),
      nHashFuncs(std::min((unsigned int)(vData.size() * 8 / nElements * LN2),
                          MAX_HASH_FUNCS)),
      nTweak(nTweakIn), nFlags(nFlagsIn), fastCacheValid(false),
      setBitsCacheValid(false) {
  // Ensure minimum viable filter size
  if (vData.empty()) {
    vData.resize(1);
    nHashFuncs = std::max(1u, nHashFuncs);
  }
}

CBloomFilter::CBloomFilter(const unsigned int nElements, const double nFPRate,
                           const unsigned int nTweakIn)
    : vData(std::max(
          1u, (unsigned int)(-1 / LN2SQUARED * nElements * log(nFPRate)) / 8)),
      isFull(false), isEmpty(true),
      nHashFuncs(std::max(
          1u, std::min((unsigned int)(vData.size() * 8 / nElements * LN2),
                       MAX_HASH_FUNCS))),
      nTweak(nTweakIn), nFlags(BLOOM_UPDATE_NONE), fastCacheValid(false),
      setBitsCacheValid(false) {}

CBloomFilter::CBloomFilter(const CBloomFilter &other)
    : vData(other.vData), isFull(other.isFull), isEmpty(other.isEmpty),
      nHashFuncs(other.nHashFuncs), nTweak(other.nTweak), nFlags(other.nFlags),
      fastCacheValid(false), setBitsCacheValid(false) {}

CBloomFilter &CBloomFilter::operator=(const CBloomFilter &other) {
  if (this != &other) {
    vData = other.vData;
    isFull = other.isFull;
    isEmpty = other.isEmpty;
    nHashFuncs = other.nHashFuncs;
    nTweak = other.nTweak;
    nFlags = other.nFlags;
    InvalidateFastCache();
    InvalidateSetBitsCache();
  }
  return *this;
}

void CBloomFilter::InvalidateFastCache() const { fastCacheValid = false; }

void CBloomFilter::UpdateFastCache() const {
  if (fastCacheValid || vData.size() > 256)
    return; // Only cache small filters

  const size_t qwords = (vData.size() + 7) / 8;
  vDataFast.resize(qwords);

  for (size_t i = 0; i < qwords; ++i) {
    uint64_t val = 0;
    const size_t base = i * 8;
    for (size_t j = 0; j < 8 && base + j < vData.size(); ++j) {
      val |= static_cast<uint64_t>(vData[base + j]) << (j * 8);
    }
    vDataFast[i] = val;
  }
  fastCacheValid = true;
}

void CBloomFilter::InvalidateSetBitsCache() const { setBitsCacheValid = false; }

size_t CBloomFilter::CountSetBits() const {
  if (setBitsCacheValid)
    return setBitsCache;

  setBitsCache = 0;
  for (size_t i = 0; i < vData.size(); ++i) {
    setBitsCache += __builtin_popcount(vData[i]);
  }
  setBitsCacheValid = true;
  return setBitsCache;
}

inline unsigned int
CBloomFilter::Hash(unsigned int nHashNum,
                   const std::vector<unsigned char> &vDataToHash) const {
  return MurmurHash3(nHashNum * 0xFBA4C795 + nTweak, vDataToHash) %
         (vData.size() * 8);
}

void CBloomFilter::insert(const std::vector<unsigned char> &vKey) {
  if (isFull)
    return;

  for (unsigned int i = 0; i < nHashFuncs; i++) {
    unsigned int nIndex = Hash(i, vKey);
    vData[nIndex >> 3] |= (1 << (7 & nIndex));
  }

  isEmpty = false;
  InvalidateFastCache();
  InvalidateSetBitsCache();
}

void CBloomFilter::insert(const COutPoint &outpoint) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << outpoint;
  std::vector<unsigned char> data(stream.begin(), stream.end());
  insert(data);
}

void CBloomFilter::insert(const uint256 &hash) {
  std::vector<unsigned char> data(hash.begin(), hash.end());
  insert(data);
}

bool CBloomFilter::contains(const std::vector<unsigned char> &vKey) const {
  if (isFull)
    return true;
  if (isEmpty)
    return false;

  // Use fast cache for small filters
  if (vData.size() <= 256) {
    UpdateFastCache();
    if (fastCacheValid) {
      for (unsigned int i = 0; i < nHashFuncs; i++) {
        unsigned int nIndex = Hash(i, vKey);
        const size_t qword_idx = (nIndex >> 3) / 8;
        const size_t bit_in_qword = nIndex & 63;

        if (qword_idx < vDataFast.size()) {
          if (!(vDataFast[qword_idx] & (1ULL << bit_in_qword))) {
            return false;
          }
        } else {
          // Fallback for boundary cases
          if (!(vData[nIndex >> 3] & (1 << (7 & nIndex)))) {
            return false;
          }
        }
      }
      return true;
    }
  }

  // Standard implementation
  for (unsigned int i = 0; i < nHashFuncs; i++) {
    unsigned int nIndex = Hash(i, vKey);
    if (!(vData[nIndex >> 3] & (1 << (7 & nIndex))))
      return false;
  }
  return true;
}

bool CBloomFilter::contains(const COutPoint &outpoint) const {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << outpoint;
  std::vector<unsigned char> data(stream.begin(), stream.end());
  return contains(data);
}

bool CBloomFilter::contains(const uint256 &hash) const {
  std::vector<unsigned char> data(hash.begin(), hash.end());
  return contains(data);
}

void CBloomFilter::clear() {
  std::fill(vData.begin(), vData.end(), 0);
  isFull = false;
  isEmpty = true;
  InvalidateFastCache();
  InvalidateSetBitsCache();
}

void CBloomFilter::reset(const unsigned int nNewTweak) {
  clear();
  nTweak = nNewTweak;
}

bool CBloomFilter::IsWithinSizeConstraints() const {
  return vData.size() <= MAX_BLOOM_FILTER_SIZE && nHashFuncs <= MAX_HASH_FUNCS;
}

bool CBloomFilter::IsRelevantAndUpdate(const CTransaction &tx) {
  bool fFound = false;

  if (isFull)
    return true;
  if (isEmpty)
    return false;

  const uint256 &hash = tx.GetHash();
  if (contains(hash))
    fFound = true;

  for (unsigned int i = 0; i < tx.vout.size(); i++) {
    const CTxOut &txout = tx.vout[i];

    CScript::const_iterator pc = txout.scriptPubKey.begin();
    std::vector<unsigned char> data;
    while (pc < txout.scriptPubKey.end()) {
      opcodetype opcode;
      if (!txout.scriptPubKey.GetOp(pc, opcode, data))
        break;
      if (data.size() != 0 && contains(data)) {
        fFound = true;
        if ((nFlags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_ALL)
          insert(COutPoint(hash, i));
        else if ((nFlags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_P2PUBKEY_ONLY) {
          txnouttype type;
          std::vector<std::vector<unsigned char>> vSolutions;
          if (Solver(txout.scriptPubKey, type, vSolutions) &&
              (type == TX_PUBKEY || type == TX_MULTISIG))
            insert(COutPoint(hash, i));
        }
        break;
      }
    }
  }

  if (fFound)
    return true;

  for (const CTxIn &txin : tx.vin) {
    if (contains(txin.prevout))
      return true;

    CScript::const_iterator pc = txin.scriptSig.begin();
    std::vector<unsigned char> data;
    while (pc < txin.scriptSig.end()) {
      opcodetype opcode;
      if (!txin.scriptSig.GetOp(pc, opcode, data))
        break;
      if (data.size() != 0 && contains(data))
        return true;
    }
  }

  return false;
}

void CBloomFilter::UpdateEmptyFull() {
  bool full = true;
  bool empty = true;

  // Optimized check using 64-bit operations where possible
  const size_t aligned_size = vData.size() & ~7;
  const uint64_t *data64 = reinterpret_cast<const uint64_t *>(vData.data());

  for (size_t i = 0; i < aligned_size / 8; ++i) {
    const uint64_t val = data64[i];
    full &= (val == UINT64_MAX);
    empty &= (val == 0);
    if (!full && !empty) {
      isFull = false;
      isEmpty = false;
      return;
    }
  }

  // Handle remaining bytes
  for (size_t i = aligned_size; i < vData.size(); ++i) {
    full &= (vData[i] == 0xff);
    empty &= (vData[i] == 0);
  }

  isFull = full;
  isEmpty = empty;
}

double CBloomFilter::GetCurrentFPRate() const {
  if (isEmpty)
    return 0.0;
  if (isFull)
    return 1.0;

  const size_t setBits = CountSetBits();
  const double ratio = static_cast<double>(setBits) / (vData.size() * 8);
  return std::pow(ratio, nHashFuncs);
}

size_t CBloomFilter::EstimateElementCount() const {
  if (isEmpty)
    return 0;
  if (isFull)
    return SIZE_MAX;

  const size_t setBits = CountSetBits();
  if (setBits == 0)
    return 0;

  const double ratio = static_cast<double>(setBits) / (vData.size() * 8);
  if (ratio >= 1.0)
    return SIZE_MAX;

  return static_cast<size_t>(-static_cast<double>(vData.size() * 8) /
                             nHashFuncs * log(1.0 - ratio));
}

// CRollingBloomFilter implementation

CRollingBloomFilter::CRollingBloomFilter(const unsigned int nElements,
                                         const double fpRate)
    : hashCacheValid(false) {
  double logFpRate = log(fpRate);

  nHashFuncs = std::max(1, std::min((int)round(logFpRate / log(0.5)), 50));

  nEntriesPerGeneration = (nElements + 1) / 2;
  uint32_t nMaxElements = nEntriesPerGeneration * 3;

  uint32_t nFilterBits = (uint32_t)ceil(-1.0 * nHashFuncs * nMaxElements /
                                        log(1.0 - exp(logFpRate / nHashFuncs)));
  data.clear();
  data.resize(((nFilterBits + 63) / 64) << 1);

  // Pre-allocate hash cache
  hashCache.resize(nHashFuncs);

  reset();
}

void CRollingBloomFilter::InvalidateHashCache() const {
  hashCacheValid = false;
}

void CRollingBloomFilter::ComputeHashCache(
    const std::vector<unsigned char> &vKey) const {
  if (hashCacheValid && lastHashedKey == vKey)
    return;

  for (int n = 0; n < nHashFuncs; n++) {
    hashCache[n] = MurmurHash3(n * 0xFBA4C795 + nTweak, vKey);
  }

  lastHashedKey = vKey;
  hashCacheValid = true;
}

static inline uint32_t
RollingBloomHash(unsigned int nHashNum, uint32_t nTweak,
                 const std::vector<unsigned char> &vDataToHash) {
  return MurmurHash3(nHashNum * 0xFBA4C795 + nTweak, vDataToHash);
}

void CRollingBloomFilter::insert(const std::vector<unsigned char> &vKey) {
  if (nEntriesThisGeneration == nEntriesPerGeneration) {
    nEntriesThisGeneration = 0;
    nGeneration++;
    if (nGeneration == 4) {
      nGeneration = 1;
    }

    const uint64_t nGenerationMask1 = 0ULL - (uint64_t)(nGeneration & 1);
    const uint64_t nGenerationMask2 = 0ULL - (uint64_t)(nGeneration >> 1);

    for (uint32_t p = 0; p < data.size(); p += 2) {
      const uint64_t p1 = data[p];
      const uint64_t p2 = data[p + 1];
      const uint64_t mask = (p1 ^ nGenerationMask1) | (p2 ^ nGenerationMask2);
      data[p] = p1 & mask;
      data[p + 1] = p2 & mask;
    }

    InvalidateHashCache();
  }
  nEntriesThisGeneration++;

  ComputeHashCache(vKey);

  const uint64_t gen1 = (uint64_t)(nGeneration & 1);
  const uint64_t gen2 = (uint64_t)(nGeneration >> 1);

  for (int n = 0; n < nHashFuncs; n++) {
    const uint32_t h = hashCache[n];
    const int bit = h & 0x3F;
    const uint32_t pos = (h >> 6) % data.size();
    const uint64_t bitMask = 1ULL << bit;

    data[pos & ~1] = (data[pos & ~1] & ~bitMask) | (gen1 << bit);
    data[pos | 1] = (data[pos | 1] & ~bitMask) | (gen2 << bit);
  }
}

void CRollingBloomFilter::insert(const uint256 &hash) {
  std::vector<unsigned char> vData(hash.begin(), hash.end());
  insert(vData);
}

bool CRollingBloomFilter::contains(
    const std::vector<unsigned char> &vKey) const {
  ComputeHashCache(vKey);

  for (int n = 0; n < nHashFuncs; n++) {
    const uint32_t h = hashCache[n];
    const int bit = h & 0x3F;
    const uint32_t pos = (h >> 6) % data.size();

    if (!(((data[pos & ~1] | data[pos | 1]) >> bit) & 1)) {
      return false;
    }
  }
  return true;
}

bool CRollingBloomFilter::contains(const uint256 &hash) const {
  std::vector<unsigned char> vData(hash.begin(), hash.end());
  return contains(vData);
}

void CRollingBloomFilter::reset() {
  nTweak = GetRand(std::numeric_limits<unsigned int>::max());
  nEntriesThisGeneration = 0;
  nGeneration = 1;
  for (std::vector<uint64_t>::iterator it = data.begin(); it != data.end();
       it++) {
    *it = 0;
  }
  InvalidateHashCache();
}