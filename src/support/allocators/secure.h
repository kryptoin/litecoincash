// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SUPPORT_ALLOCATORS_SECURE_H
#define BITCOIN_SUPPORT_ALLOCATORS_SECURE_H

#include <support/cleanse.h>
#include <support/lockedpool.h>

#include <string>

template <typename T> struct secure_allocator : public std::allocator<T> {
  typedef std::allocator<T> base;
  typedef typename base::size_type size_type;
  typedef typename base::difference_type difference_type;
  typedef typename base::pointer pointer;
  typedef typename base::const_pointer const_pointer;
  typedef typename base::reference reference;
  typedef typename base::const_reference const_reference;
  typedef typename base::value_type value_type;
  secure_allocator() noexcept {}
  secure_allocator(const secure_allocator &a) noexcept : base(a) {}
  template <typename U>
  secure_allocator(const secure_allocator<U> &a) noexcept : base(a) {}
  ~secure_allocator() noexcept {}
  template <typename _Other> struct rebind {
    typedef secure_allocator<_Other> other;
  };

  T *allocate(std::size_t n, const void *hint = 0) {
    return static_cast<T *>(LockedPoolManager::Instance().alloc(sizeof(T) * n));
  }

  void deallocate(T *p, std::size_t n) {
    if (p != nullptr) {
      memory_cleanse(p, sizeof(T) * n);
    }
    LockedPoolManager::Instance().free(p);
  }
};

typedef std::basic_string<char, std::char_traits<char>, secure_allocator<char>>
    SecureString;

#endif
