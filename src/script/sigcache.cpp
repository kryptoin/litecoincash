// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sigcache.h>

#include <memusage.h>
#include <pubkey.h>
#include <random.h>
#include <uint256.h>
#include <util.h>

#include <boost/thread.hpp>
#include <cuckoocache.h>

namespace {
class CSignatureCache {
private:
  uint256 nonce;
  typedef CuckooCache::cache<uint256, SignatureCacheHasher> map_type;
  map_type setValid;
  boost::shared_mutex cs_sigcache;

public:
  CSignatureCache() { GetRandBytes(nonce.begin(), 32); }

  void ComputeEntry(uint256 &entry, const uint256 &hash,
                    const std::vector<unsigned char> &vchSig,
                    const CPubKey &pubkey) {
    CSHA256()
        .Write(nonce.begin(), 32)
        .Write(hash.begin(), 32)
        .Write(&pubkey[0], pubkey.size())
        .Write(&vchSig[0], vchSig.size())
        .Finalize(entry.begin());
  }

  bool Get(const uint256 &entry, const bool erase) {
    boost::shared_lock<boost::shared_mutex> lock(cs_sigcache);
    return setValid.contains(entry, erase);
  }

  void Set(uint256 &entry) {
    boost::unique_lock<boost::shared_mutex> lock(cs_sigcache);
    setValid.insert(entry);
  }
  uint32_t setup_bytes(size_t n) { return setValid.setup_bytes(n); }
};

static CSignatureCache signatureCache;
} // namespace

void InitSignatureCache() {
  size_t nMaxCacheSize =
      std::min(std::max((int64_t)0, gArgs.GetArg("-maxsigcachesize",
                                                 DEFAULT_MAX_SIG_CACHE_SIZE) /
                                        2),
               MAX_MAX_SIG_CACHE_SIZE) *
      ((size_t)1 << 20);
  size_t nElems = signatureCache.setup_bytes(nMaxCacheSize);
  LogPrintf("Using %zu MiB out of %zu/2 requested for signature cache, able to "
            "store %zu elements\n",
            (nElems * sizeof(uint256)) >> 20, (nMaxCacheSize * 2) >> 20,
            nElems);
}

bool CachingTransactionSignatureChecker::VerifySignature(
    const std::vector<unsigned char> &vchSig, const CPubKey &pubkey,
    const uint256 &sighash) const {
  uint256 entry;
  signatureCache.ComputeEntry(entry, sighash, vchSig, pubkey);
  if (signatureCache.Get(entry, !store))
    return true;
  if (!TransactionSignatureChecker::VerifySignature(vchSig, pubkey, sighash))
    return false;
  if (store)
    signatureCache.Set(entry);
  return true;
}
