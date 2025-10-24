// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <support/cleanse.h>

#include <cstring>

#if defined(_MSC_VER)
#include <Windows.h>

#endif

void memory_cleanse(void *ptr, size_t len) {
  std::memset(ptr, 0, len);

#if defined(_MSC_VER)
  SecureZeroMemory(ptr, len);
#else
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}
