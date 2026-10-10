// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bloom.h>

#include <hash.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/script.h>
#include <script/standard.h>
#include <streams.h>

#include <math.h>
#include <stdlib.h>

#define LN2SQUARED 0.4804530139182014246671025263266649717305529515945455
#define LN2 0.6931471805599453094172321214581765680755001343602552

CBloomFilter::CBloomFilter(const unsigned int nElements, const double nFPRate,
                           const unsigned int nTweakIn, unsigned char nFlagsIn)
    :

      vData(std::min((unsigned int)(-1 / LN2SQUARED * nElements * log(nFPRate)),
                     MAX_BLOOM_FILTER_SIZE * 8) /
            8),

      isFull(false), isEmpty(true),
      nHashFuncs(std::min((unsigned int)(vData.size() * 8 / nElements * LN2),
                          MAX_HASH_FUNCS)),
      nTweak(nTweakIn), nFlags(nFlagsIn) {}

CBloomFilter::CBloomFilter(const unsigned int nElements, const double nFPRate,
                           const unsigned int nTweakIn)
    : vData((unsigned int)(-1 / LN2SQUARED * nElements * log(nFPRate)) / 8),
      isFull(false), isEmpty(true),
      nHashFuncs((unsigned int)(vData.size() * 8 / nElements * LN2)),
      nTweak(nTweakIn), nFlags(BLOOM_UPDATE_NONE) {}

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
  vData.assign(vData.size(), 0);
  isFull = false;
  isEmpty = true;
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
  for (unsigned int i = 0; i < vData.size(); i++) {
    full &= vData[i] == 0xff;
    empty &= vData[i] == 0;
  }
  isFull = full;
  isEmpty = empty;
}

CRollingBloomFilter::CRollingBloomFilter(const unsigned int nElements,
                                         const double fpRate) {
  double logFpRate = log(fpRate);

  nHashFuncs = std::max(1, std::min((int)round(logFpRate / log(0.5)), 50));

  nEntriesPerGeneration = (nElements + 1) / 2;
  uint32_t nMaxElements = nEntriesPerGeneration * 3;

  uint32_t nFilterBits = (uint32_t)ceil(-1.0 * nHashFuncs * nMaxElements /
                                        log(1.0 - exp(logFpRate / nHashFuncs)));
  data.clear();

  data.resize(((nFilterBits + 63) / 64) << 1);
  reset();
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
    uint64_t nGenerationMask1 = 0 - (uint64_t)(nGeneration & 1);
    uint64_t nGenerationMask2 = 0 - (uint64_t)(nGeneration >> 1);

    for (uint32_t p = 0; p < data.size(); p += 2) {
      uint64_t p1 = data[p], p2 = data[p + 1];
      uint64_t mask = (p1 ^ nGenerationMask1) | (p2 ^ nGenerationMask2);
      data[p] = p1 & mask;
      data[p + 1] = p2 & mask;
    }
  }
  nEntriesThisGeneration++;

  for (int n = 0; n < nHashFuncs; n++) {
    uint32_t h = RollingBloomHash(n, nTweak, vKey);
    int bit = h & 0x3F;
    uint32_t pos = (h >> 6) % data.size();

    data[pos & ~1] = (data[pos & ~1] & ~(((uint64_t)1) << bit)) |
                     ((uint64_t)(nGeneration & 1)) << bit;
    data[pos | 1] = (data[pos | 1] & ~(((uint64_t)1) << bit)) |
                    ((uint64_t)(nGeneration >> 1)) << bit;
  }
}

void CRollingBloomFilter::insert(const uint256 &hash) {
  std::vector<unsigned char> vData(hash.begin(), hash.end());
  insert(vData);
}

bool CRollingBloomFilter::contains(
    const std::vector<unsigned char> &vKey) const {
  for (int n = 0; n < nHashFuncs; n++) {
    uint32_t h = RollingBloomHash(n, nTweak, vKey);
    int bit = h & 0x3F;
    uint32_t pos = (h >> 6) % data.size();

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
}
