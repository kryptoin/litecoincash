// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SUPPORT_LOCKEDPOOL_H
#define BITCOIN_SUPPORT_LOCKEDPOOL_H

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>

class LockedPageAllocator {
public:
  virtual ~LockedPageAllocator() {}

  virtual void *AllocateLocked(size_t len, bool *lockingSuccess) = 0;

  virtual void FreeLocked(void *addr, size_t len) = 0;

  virtual size_t GetLimit() = 0;
};

class Arena {
public:
  Arena(void *base, size_t size, size_t alignment);
  virtual ~Arena();

  Arena(const Arena &other) = delete;

  Arena &operator=(const Arena &) = delete;

  struct Stats {
    size_t used;
    size_t free;
    size_t total;
    size_t chunks_used;
    size_t chunks_free;
  };

  void *alloc(size_t size);

  void free(void *ptr);

  Stats stats() const;

#ifdef ARENA_DEBUG
  void walk() const;
#endif

  bool addressInArena(void *ptr) const { return ptr >= base && ptr < end; }

private:
  std::map<char *, size_t> chunks_free;
  std::map<char *, size_t> chunks_used;

  char *base;

  char *end;

  size_t alignment;
};

class LockedPool {
public:
  static const size_t ARENA_SIZE = 256 * 1024;

  static const size_t ARENA_ALIGN = 16;

  typedef bool (*LockingFailed_Callback)();

  struct Stats {
    size_t used;
    size_t free;
    size_t total;
    size_t locked;
    size_t chunks_used;
    size_t chunks_free;
  };

  explicit LockedPool(std::unique_ptr<LockedPageAllocator> allocator,
                      LockingFailed_Callback lf_cb_in = nullptr);
  ~LockedPool();

  LockedPool(const LockedPool &other) = delete;

  LockedPool &operator=(const LockedPool &) = delete;

  void *alloc(size_t size);

  void free(void *ptr);

  Stats stats() const;

private:
  std::unique_ptr<LockedPageAllocator> allocator;

  class LockedPageArena : public Arena {
  public:
    LockedPageArena(LockedPageAllocator *alloc_in, void *base_in, size_t size,
                    size_t align);
    ~LockedPageArena();

  private:
    void *base;
    size_t size;
    LockedPageAllocator *allocator;
  };

  bool new_arena(size_t size, size_t align);

  std::list<LockedPageArena> arenas;
  LockingFailed_Callback lf_cb;
  size_t cumulative_bytes_locked;

  mutable std::mutex mutex;
};

class LockedPoolManager : public LockedPool {
public:
  static LockedPoolManager &Instance() {
    std::call_once(LockedPoolManager::init_flag,
                   LockedPoolManager::CreateInstance);
    return *LockedPoolManager::_instance;
  }

private:
  explicit LockedPoolManager(std::unique_ptr<LockedPageAllocator> allocator);

  static void CreateInstance();

  static bool LockingFailed();

  static LockedPoolManager *_instance;
  static std::once_flag init_flag;
};

#endif
