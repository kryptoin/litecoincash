// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RANDOM_H
#define BITCOIN_RANDOM_H

#include <crypto/chacha20.h>
#include <crypto/common.h>
#include <uint256.h>

#include <stdint.h>

void RandAddSeed();

void GetRandBytes(unsigned char *buf, int num);
uint64_t GetRand(uint64_t nMax);
int GetRandInt(int nMax);
uint256 GetRandHash();

void RandAddSeedSleep();

void GetStrongRandBytes(unsigned char *buf, int num);

class FastRandomContext {
private:
  bool requires_seed;
  ChaCha20 rng;

  unsigned char bytebuf[64];
  int bytebuf_size;

  uint64_t bitbuf;
  int bitbuf_size;

  void RandomSeed();

  void FillByteBuffer() {
    if (requires_seed) {
      RandomSeed();
    }
    rng.Output(bytebuf, sizeof(bytebuf));
    bytebuf_size = sizeof(bytebuf);
  }

  void FillBitBuffer() {
    bitbuf = rand64();
    bitbuf_size = 64;
  }

public:
  explicit FastRandomContext(bool fDeterministic = false);

  explicit FastRandomContext(const uint256 &seed);

  uint64_t rand64() {
    if (bytebuf_size < 8)
      FillByteBuffer();
    uint64_t ret = ReadLE64(bytebuf + 64 - bytebuf_size);
    bytebuf_size -= 8;
    return ret;
  }

  uint64_t randbits(int bits) {
    if (bits == 0) {
      return 0;
    } else if (bits > 32) {
      return rand64() >> (64 - bits);
    } else {
      if (bitbuf_size < bits)
        FillBitBuffer();
      uint64_t ret = bitbuf & (~(uint64_t)0 >> (64 - bits));
      bitbuf >>= bits;
      bitbuf_size -= bits;
      return ret;
    }
  }

  uint64_t randrange(uint64_t range) {
    --range;
    int bits = CountBits(range);
    while (true) {
      uint64_t ret = randbits(bits);
      if (ret <= range)
        return ret;
    }
  }

  std::vector<unsigned char> randbytes(size_t len);

  uint32_t rand32() { return randbits(32); }

  uint256 rand256();

  bool randbool() { return randbits(1); }
};

static const int NUM_OS_RANDOM_BYTES = 32;

void GetOSRand(unsigned char *ent32);

bool Random_SanityCheck();

void RandomInit();

#endif
