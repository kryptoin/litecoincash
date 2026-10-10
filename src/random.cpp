// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <random.h>

#include <crypto/sha512.h>
#include <support/cleanse.h>
#ifdef WIN32
#include <compat.h>

#include <wincrypt.h>
#endif
#include <util.h>

#include <utilstrencodings.h>

#include <chrono>
#include <limits>
#include <stdlib.h>
#include <thread>

#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_GETRANDOM
#include <linux/random.h>
#include <sys/syscall.h>
#endif
#if defined(HAVE_GETENTROPY) ||                                                \
    (defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX))
#include <unistd.h>
#endif
#if defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)
#include <sys/random.h>
#endif
#ifdef HAVE_SYSCTL_ARND
#include <sys/sysctl.h>
#endif

#include <mutex>

#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
#include <cpuid.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>

[[noreturn]] static void RandFailure() {
  LogPrintf("Failed to read randomness, aborting\n");
  std::abort();
}

static inline int64_t GetPerformanceCounter() {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  return __rdtsc();
#elif !defined(_MSC_VER) && defined(__i386__)
  uint64_t r = 0;
  __asm__ volatile("rdtsc" : "=A"(r));

  return r;
#elif !defined(_MSC_VER) && (defined(__x86_64__) || defined(__amd64__))
  uint64_t r1 = 0, r2 = 0;
  __asm__ volatile("rdtsc" : "=a"(r1), "=d"(r2));

  return (r2 << 32) | r1;
#else

  return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
static std::atomic<bool> hwrand_initialized{false};
static bool rdrand_supported = false;
static constexpr uint32_t CPUID_F1_ECX_RDRAND = 0x40000000;
static void RDRandInit() {
  uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) && (ecx & CPUID_F1_ECX_RDRAND)) {
    LogPrintf("Using RdRand as an additional entropy source\n");
    rdrand_supported = true;
  }
  hwrand_initialized.store(true);
}
#else
static void RDRandInit() {}
#endif

static bool GetHWRand(unsigned char *ent32) {
#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
  assert(hwrand_initialized.load(std::memory_order_relaxed));
  if (rdrand_supported) {
    uint8_t ok;

#ifdef __i386__
    for (int iter = 0; iter < 4; ++iter) {
      uint32_t r1, r2;
      __asm__ volatile(".byte 0x0f, 0xc7, 0xf0;"

                       ".byte 0x0f, 0xc7, 0xf2;"

                       "setc %2"
                       : "=a"(r1), "=d"(r2), "=q"(ok)::"cc");
      if (!ok)
        return false;
      WriteLE32(ent32 + 8 * iter, r1);
      WriteLE32(ent32 + 8 * iter + 4, r2);
    }
#else
    uint64_t r1, r2, r3, r4;
    __asm__ volatile(".byte 0x48, 0x0f, 0xc7, 0xf0, "

                     "0x48, 0x0f, 0xc7, 0xf3, "

                     "0x48, 0x0f, 0xc7, 0xf1, "

                     "0x48, 0x0f, 0xc7, 0xf2; "

                     "setc %4"
                     : "=a"(r1), "=b"(r2), "=c"(r3), "=d"(r4), "=q"(ok)::"cc");
    if (!ok)
      return false;
    WriteLE64(ent32, r1);
    WriteLE64(ent32 + 8, r2);
    WriteLE64(ent32 + 16, r3);
    WriteLE64(ent32 + 24, r4);
#endif
    return true;
  }
#endif
  return false;
}

void RandAddSeed() {
  int64_t nCounter = GetPerformanceCounter();
  RAND_add(&nCounter, sizeof(nCounter), 1.5);
  memory_cleanse((void *)&nCounter, sizeof(nCounter));
}

static void RandAddSeedPerfmon() {
  RandAddSeed();

#ifdef WIN32

  static int64_t nLastPerfmon;
  if (GetTime() < nLastPerfmon + 10 * 60)
    return;
  nLastPerfmon = GetTime();

  std::vector<unsigned char> vData(250000, 0);
  long ret = 0;
  unsigned long nSize = 0;
  const size_t nMaxSize = 10000000;

  while (true) {
    nSize = vData.size();
    ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", nullptr, nullptr,
                           vData.data(), &nSize);
    if (ret != ERROR_MORE_DATA || vData.size() >= nMaxSize)
      break;
    vData.resize(std::max((vData.size() * 3) / 2, nMaxSize));
  }
  RegCloseKey(HKEY_PERFORMANCE_DATA);
  if (ret == ERROR_SUCCESS) {
    RAND_add(vData.data(), nSize, nSize / 100.0);
    memory_cleanse(vData.data(), nSize);
    LogPrint(BCLog::RAND, "%s: %lu bytes\n", __func__, nSize);
  } else {
    static bool warned = false;

    if (!warned) {
      LogPrintf("%s: Warning: RegQueryValueExA(HKEY_PERFORMANCE_DATA) failed "
                "with code %i\n",
                __func__, ret);
      warned = true;
    }
  }
#endif
}

#ifndef WIN32

void GetDevURandom(unsigned char *ent32) {
  int f = open("/dev/urandom", O_RDONLY);
  if (f == -1) {
    RandFailure();
  }
  int have = 0;
  do {
    ssize_t n = read(f, ent32 + have, NUM_OS_RANDOM_BYTES - have);
    if (n <= 0 || n + have > NUM_OS_RANDOM_BYTES) {
      close(f);
      RandFailure();
    }
    have += n;
  } while (have < NUM_OS_RANDOM_BYTES);
  close(f);
}
#endif

void GetOSRand(unsigned char *ent32) {
#if defined(WIN32)
  HCRYPTPROV hProvider;
  int ret = CryptAcquireContextW(&hProvider, nullptr, nullptr, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT);
  if (!ret) {
    RandFailure();
  }
  ret = CryptGenRandom(hProvider, NUM_OS_RANDOM_BYTES, ent32);
  if (!ret) {
    RandFailure();
  }
  CryptReleaseContext(hProvider, 0);
#elif defined(HAVE_SYS_GETRANDOM)

  int rv = syscall(SYS_getrandom, ent32, NUM_OS_RANDOM_BYTES, 0);
  if (rv != NUM_OS_RANDOM_BYTES) {
    if (rv < 0 && errno == ENOSYS) {
      GetDevURandom(ent32);
    } else {
      RandFailure();
    }
  }
#elif defined(HAVE_GETENTROPY) && defined(__OpenBSD__)

  if (getentropy(ent32, NUM_OS_RANDOM_BYTES) != 0) {
    RandFailure();
  }
#elif defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)

  if (&getentropy != nullptr) {
    if (getentropy(ent32, NUM_OS_RANDOM_BYTES) != 0) {
      RandFailure();
    }
  } else {
    GetDevURandom(ent32);
  }
#elif defined(HAVE_SYSCTL_ARND)

  static const int name[2] = {CTL_KERN, KERN_ARND};
  int have = 0;
  do {
    size_t len = NUM_OS_RANDOM_BYTES - have;
    if (sysctl(name, ARRAYLEN(name), ent32 + have, &len, nullptr, 0) != 0) {
      RandFailure();
    }
    have += len;
  } while (have < NUM_OS_RANDOM_BYTES);
#else

  GetDevURandom(ent32);
#endif
}

void GetRandBytes(unsigned char *buf, int num) {
  if (RAND_bytes(buf, num) != 1) {
    RandFailure();
  }
}

static void AddDataToRng(void *data, size_t len);

void RandAddSeedSleep() {
  int64_t nPerfCounter1 = GetPerformanceCounter();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  int64_t nPerfCounter2 = GetPerformanceCounter();

  AddDataToRng(&nPerfCounter1, sizeof(nPerfCounter1));
  AddDataToRng(&nPerfCounter2, sizeof(nPerfCounter2));

  memory_cleanse(&nPerfCounter1, sizeof(nPerfCounter1));
  memory_cleanse(&nPerfCounter2, sizeof(nPerfCounter2));
}

static std::mutex cs_rng_state;
static unsigned char rng_state[32] = {0};
static uint64_t rng_counter = 0;

static void AddDataToRng(void *data, size_t len) {
  CSHA512 hasher;
  hasher.Write((const unsigned char *)&len, sizeof(len));
  hasher.Write((const unsigned char *)data, len);
  unsigned char buf[64];
  {
    std::unique_lock<std::mutex> lock(cs_rng_state);
    hasher.Write(rng_state, sizeof(rng_state));
    hasher.Write((const unsigned char *)&rng_counter, sizeof(rng_counter));
    ++rng_counter;
    hasher.Finalize(buf);
    memcpy(rng_state, buf + 32, 32);
  }
  memory_cleanse(buf, 64);
}

void GetStrongRandBytes(unsigned char *out, int num) {
  assert(num <= 32);
  CSHA512 hasher;
  unsigned char buf[64];

  RandAddSeedPerfmon();
  GetRandBytes(buf, 32);
  hasher.Write(buf, 32);

  GetOSRand(buf);
  hasher.Write(buf, 32);

  if (GetHWRand(buf)) {
    hasher.Write(buf, 32);
  }

  {
    std::unique_lock<std::mutex> lock(cs_rng_state);
    hasher.Write(rng_state, sizeof(rng_state));
    hasher.Write((const unsigned char *)&rng_counter, sizeof(rng_counter));
    ++rng_counter;
    hasher.Finalize(buf);
    memcpy(rng_state, buf + 32, 32);
  }

  memcpy(out, buf, num);
  memory_cleanse(buf, 64);
}

uint64_t GetRand(uint64_t nMax) {
  if (nMax == 0)
    return 0;

  uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
  uint64_t nRand = 0;
  do {
    GetRandBytes((unsigned char *)&nRand, sizeof(nRand));
  } while (nRand >= nRange);
  return (nRand % nMax);
}

int GetRandInt(int nMax) { return GetRand(nMax); }

uint256 GetRandHash() {
  uint256 hash;
  GetRandBytes((unsigned char *)&hash, sizeof(hash));
  return hash;
}

void FastRandomContext::RandomSeed() {
  uint256 seed = GetRandHash();
  rng.SetKey(seed.begin(), 32);
  requires_seed = false;
}

uint256 FastRandomContext::rand256() {
  if (bytebuf_size < 32) {
    FillByteBuffer();
  }
  uint256 ret;
  memcpy(ret.begin(), bytebuf + 64 - bytebuf_size, 32);
  bytebuf_size -= 32;
  return ret;
}

std::vector<unsigned char> FastRandomContext::randbytes(size_t len) {
  std::vector<unsigned char> ret(len);
  if (len > 0) {
    rng.Output(&ret[0], len);
  }
  return ret;
}

FastRandomContext::FastRandomContext(const uint256 &seed)
    : requires_seed(false), bytebuf_size(0), bitbuf_size(0) {
  rng.SetKey(seed.begin(), 32);
}

bool Random_SanityCheck() {
  uint64_t start = GetPerformanceCounter();

  static const ssize_t MAX_TRIES = 1024;
  uint8_t data[NUM_OS_RANDOM_BYTES];
  bool overwritten[NUM_OS_RANDOM_BYTES] = {};

  int num_overwritten;
  int tries = 0;

  do {
    memset(data, 0, NUM_OS_RANDOM_BYTES);
    GetOSRand(data);
    for (int x = 0; x < NUM_OS_RANDOM_BYTES; ++x) {
      overwritten[x] |= (data[x] != 0);
    }

    num_overwritten = 0;
    for (int x = 0; x < NUM_OS_RANDOM_BYTES; ++x) {
      if (overwritten[x]) {
        num_overwritten += 1;
      }
    }

    tries += 1;
  } while (num_overwritten < NUM_OS_RANDOM_BYTES && tries < MAX_TRIES);
  if (num_overwritten != NUM_OS_RANDOM_BYTES)
    return false;

  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  uint64_t stop = GetPerformanceCounter();
  if (stop == start)
    return false;

  RAND_add((const unsigned char *)&start, sizeof(start), 1);
  RAND_add((const unsigned char *)&stop, sizeof(stop), 1);

  return true;
}

FastRandomContext::FastRandomContext(bool fDeterministic)
    : requires_seed(!fDeterministic), bytebuf_size(0), bitbuf_size(0) {
  if (!fDeterministic) {
    return;
  }
  uint256 seed;
  rng.SetKey(seed.begin(), 32);
}

void RandomInit() { RDRandInit(); }
