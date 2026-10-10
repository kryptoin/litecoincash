// Copyright (c) 2012 Pieter Wuille
// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H

#include <netaddress.h>
#include <protocol.h>
#include <random.h>
#include <sync.h>
#include <timedata.h>
#include <util.h>

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

class CAddrInfo : public CAddress {
public:
  int64_t nLastTry;

  int64_t nLastCountAttempt;

private:
  bool fInTried;
  CNetAddr source;

  friend class CAddrMan;

  int nAttempts;
  int nRandomPos;
  int nRefCount;
  int64_t nLastSuccess;

public:
  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(*(CAddress *)this);
    READWRITE(source);
    READWRITE(nLastSuccess);
    READWRITE(nAttempts);
  }

  void Init() {
    nLastSuccess = 0;
    nLastTry = 0;
    nLastCountAttempt = 0;
    nAttempts = 0;
    nRefCount = 0;
    fInTried = false;
    nRandomPos = -1;
  }

  CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource)
      : CAddress(addrIn), source(addrSource) {
    Init();
  }

  CAddrInfo() : CAddress(), source() { Init(); }

  int GetTriedBucket(const uint256 &nKey) const;

  int GetNewBucket(const uint256 &nKey, const CNetAddr &src) const;

  int GetNewBucket(const uint256 &nKey) const {
    return GetNewBucket(nKey, source);
  }

  int GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const;

  bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

  double GetChance(int64_t nNow = GetAdjustedTime()) const;
};

#define ADDRMAN_TRIED_BUCKET_COUNT_LOG2 8

#define ADDRMAN_NEW_BUCKET_COUNT_LOG2 10

#define ADDRMAN_BUCKET_SIZE_LOG2 6

#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 8

#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 64

#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 8

#define ADDRMAN_HORIZON_DAYS 30

#define ADDRMAN_RETRIES 3

#define ADDRMAN_MAX_FAILURES 10

#define ADDRMAN_MIN_FAIL_DAYS 7

#define ADDRMAN_GETADDR_MAX_PCT 23

#define ADDRMAN_GETADDR_MAX 2500

#define ADDRMAN_TRIED_BUCKET_COUNT (1 << ADDRMAN_TRIED_BUCKET_COUNT_LOG2)
#define ADDRMAN_NEW_BUCKET_COUNT (1 << ADDRMAN_NEW_BUCKET_COUNT_LOG2)
#define ADDRMAN_BUCKET_SIZE (1 << ADDRMAN_BUCKET_SIZE_LOG2)

class CAddrMan {
private:
  mutable CCriticalSection cs;

  int nIdCount;

  std::map<int, CAddrInfo> mapInfo;

  std::map<CNetAddr, int> mapAddr;

  std::vector<int> vRandom;

  int nTried;

  int vvTried[ADDRMAN_TRIED_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

  int nNew;

  int vvNew[ADDRMAN_NEW_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

  int64_t nLastGood;

protected:
  uint256 nKey;

  FastRandomContext insecure_rand;

  CAddrInfo *Find(const CNetAddr &addr, int *pnId = nullptr);

  CAddrInfo *Create(const CAddress &addr, const CNetAddr &addrSource,
                    int *pnId = nullptr);

  void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

  void MakeTried(CAddrInfo &info, int nId);

  void Delete(int nId);

  void ClearNew(int nUBucket, int nUBucketPos);

  void Good_(const CService &addr, int64_t nTime);

  bool Add_(const CAddress &addr, const CNetAddr &source, int64_t nTimePenalty);

  void Attempt_(const CService &addr, bool fCountFailure, int64_t nTime);

  CAddrInfo Select_(bool newOnly);

  virtual int RandomInt(int nMax);

#ifdef DEBUG_ADDRMAN

  int Check_();
#endif

  void GetAddr_(std::vector<CAddress> &vAddr);

  void Connected_(const CService &addr, int64_t nTime);

  void SetServices_(const CService &addr, ServiceFlags nServices);

public:
  template <typename Stream> void Serialize(Stream &s) const {
    LOCK(cs);

    unsigned char nVersion = 1;
    s << nVersion;
    s << ((unsigned char)32);
    s << nKey;
    s << nNew;
    s << nTried;

    int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
    s << nUBuckets;
    std::map<int, int> mapUnkIds;
    int nIds = 0;
    for (const auto &entry : mapInfo) {
      mapUnkIds[entry.first] = nIds;
      const CAddrInfo &info = entry.second;
      if (info.nRefCount) {
        assert(nIds != nNew);

        s << info;
        nIds++;
      }
    }
    nIds = 0;
    for (const auto &entry : mapInfo) {
      const CAddrInfo &info = entry.second;
      if (info.fInTried) {
        assert(nIds != nTried);

        s << info;
        nIds++;
      }
    }
    for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
      int nSize = 0;
      for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
        if (vvNew[bucket][i] != -1)
          nSize++;
      }
      s << nSize;
      for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
        if (vvNew[bucket][i] != -1) {
          int nIndex = mapUnkIds[vvNew[bucket][i]];
          s << nIndex;
        }
      }
    }
  }

  template <typename Stream> void Unserialize(Stream &s) {
    LOCK(cs);

    Clear();

    unsigned char nVersion;
    s >> nVersion;
    unsigned char nKeySize;
    s >> nKeySize;
    if (nKeySize != 32)
      throw std::ios_base::failure(
          "Incorrect keysize in addrman deserialization");
    s >> nKey;
    s >> nNew;
    s >> nTried;
    int nUBuckets = 0;
    s >> nUBuckets;
    if (nVersion != 0) {
      nUBuckets ^= (1 << 30);
    }

    if (nNew > ADDRMAN_NEW_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
      throw std::ios_base::failure(
          "Corrupt CAddrMan serialization, nNew exceeds limit.");
    }

    if (nTried > ADDRMAN_TRIED_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
      throw std::ios_base::failure(
          "Corrupt CAddrMan serialization, nTried exceeds limit.");
    }

    for (int n = 0; n < nNew; n++) {
      CAddrInfo &info = mapInfo[n];
      s >> info;
      mapAddr[info] = n;
      info.nRandomPos = vRandom.size();
      vRandom.push_back(n);
      if (nVersion != 1 || nUBuckets != ADDRMAN_NEW_BUCKET_COUNT) {
        int nUBucket = info.GetNewBucket(nKey);
        int nUBucketPos = info.GetBucketPosition(nKey, true, nUBucket);
        if (vvNew[nUBucket][nUBucketPos] == -1) {
          vvNew[nUBucket][nUBucketPos] = n;
          info.nRefCount++;
        }
      }
    }
    nIdCount = nNew;

    int nLost = 0;
    for (int n = 0; n < nTried; n++) {
      CAddrInfo info;
      s >> info;
      int nKBucket = info.GetTriedBucket(nKey);
      int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);
      if (vvTried[nKBucket][nKBucketPos] == -1) {
        info.nRandomPos = vRandom.size();
        info.fInTried = true;
        vRandom.push_back(nIdCount);
        mapInfo[nIdCount] = info;
        mapAddr[info] = nIdCount;
        vvTried[nKBucket][nKBucketPos] = nIdCount;
        nIdCount++;
      } else {
        nLost++;
      }
    }
    nTried -= nLost;

    for (int bucket = 0; bucket < nUBuckets; bucket++) {
      int nSize = 0;
      s >> nSize;
      for (int n = 0; n < nSize; n++) {
        int nIndex = 0;
        s >> nIndex;
        if (nIndex >= 0 && nIndex < nNew) {
          CAddrInfo &info = mapInfo[nIndex];
          int nUBucketPos = info.GetBucketPosition(nKey, true, bucket);
          if (nVersion == 1 && nUBuckets == ADDRMAN_NEW_BUCKET_COUNT &&
              vvNew[bucket][nUBucketPos] == -1 &&
              info.nRefCount < ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
            info.nRefCount++;
            vvNew[bucket][nUBucketPos] = nIndex;
          }
        }
      }
    }

    int nLostUnk = 0;
    for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin();
         it != mapInfo.end();) {
      if (it->second.fInTried == false && it->second.nRefCount == 0) {
        std::map<int, CAddrInfo>::const_iterator itCopy = it++;
        Delete(itCopy->first);
        nLostUnk++;
      } else {
        it++;
      }
    }
    if (nLost + nLostUnk > 0) {
      LogPrint(BCLog::ADDRMAN,
               "addrman lost %i new and %i tried addresses due to collisions\n",
               nLostUnk, nLost);
    }

    Check();
  }

  void Clear() {
    LOCK(cs);
    std::vector<int>().swap(vRandom);
    nKey = GetRandHash();
    for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
      for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
        vvNew[bucket][entry] = -1;
      }
    }
    for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
      for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
        vvTried[bucket][entry] = -1;
      }
    }

    nIdCount = 0;
    nTried = 0;
    nNew = 0;
    nLastGood = 1;

    mapInfo.clear();
    mapAddr.clear();
  }

  CAddrMan() { Clear(); }

  ~CAddrMan() { nKey.SetNull(); }

  size_t size() const {
    LOCK(cs);

    return vRandom.size();
  }

  void Check() {
#ifdef DEBUG_ADDRMAN
    {
      LOCK(cs);
      int err;
      if ((err = Check_()))
        LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
    }
#endif
  }

  bool Add(const CAddress &addr, const CNetAddr &source,
           int64_t nTimePenalty = 0) {
    LOCK(cs);
    bool fRet = false;
    Check();
    fRet |= Add_(addr, source, nTimePenalty);
    Check();
    if (fRet) {
      LogPrint(BCLog::ADDRMAN, "Added %s from %s: %i tried, %i new\n",
               addr.ToStringIPPort(), source.ToString(), nTried, nNew);
    }
    return fRet;
  }

  bool Add(const std::vector<CAddress> &vAddr, const CNetAddr &source,
           int64_t nTimePenalty = 0) {
    LOCK(cs);
    int nAdd = 0;
    Check();
    for (std::vector<CAddress>::const_iterator it = vAddr.begin();
         it != vAddr.end(); it++)
      nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
    Check();
    if (nAdd) {
      LogPrint(BCLog::ADDRMAN, "Added %i addresses from %s: %i tried, %i new\n",
               nAdd, source.ToString(), nTried, nNew);
    }
    return nAdd > 0;
  }

  void Good(const CService &addr, int64_t nTime = GetAdjustedTime()) {
    LOCK(cs);
    Check();
    Good_(addr, nTime);
    Check();
  }

  void Attempt(const CService &addr, bool fCountFailure,
               int64_t nTime = GetAdjustedTime()) {
    LOCK(cs);
    Check();
    Attempt_(addr, fCountFailure, nTime);
    Check();
  }

  CAddrInfo Select(bool newOnly = false) {
    CAddrInfo addrRet;
    {
      LOCK(cs);
      Check();
      addrRet = Select_(newOnly);
      Check();
    }
    return addrRet;
  }

  std::vector<CAddress> GetAddr() {
    Check();
    std::vector<CAddress> vAddr;
    {
      LOCK(cs);
      GetAddr_(vAddr);
    }
    Check();
    return vAddr;
  }

  void Connected(const CService &addr, int64_t nTime = GetAdjustedTime()) {
    LOCK(cs);
    Check();
    Connected_(addr, nTime);
    Check();
  }

  void SetServices(const CService &addr, ServiceFlags nServices) {
    LOCK(cs);
    Check();
    SetServices_(addr, nServices);
    Check();
  }
};

#endif
