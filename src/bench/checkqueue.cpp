// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <boost/thread/thread.hpp>
#include <checkqueue.h>
#include <prevector.h>
#include <random.h>
#include <util.h>
#include <validation.h>
#include <vector>

static const int MIN_CORES = 2;
static const size_t BATCHES = 101;
static const size_t BATCH_SIZE = 30;
static const int PREVECTOR_SIZE = 28;
static const unsigned int QUEUE_BATCH_SIZE = 128;

static void CCheckQueueSpeedPrevectorJob(benchmark::State &state) {
  struct PrevectorJob {
    prevector<PREVECTOR_SIZE, uint8_t> p;
    PrevectorJob() {}
    explicit PrevectorJob(FastRandomContext &insecure_rand) {
      p.resize(insecure_rand.randrange(PREVECTOR_SIZE * 2));
    }
    bool operator()() { return true; }
    void swap(PrevectorJob &x) { p.swap(x.p); };
  };
  CCheckQueue<PrevectorJob> queue{QUEUE_BATCH_SIZE};
  boost::thread_group tg;
  for (auto x = 0; x < std::max(MIN_CORES, GetNumCores()); ++x) {
    tg.create_thread([&] { queue.Thread(); });
  }
  while (state.KeepRunning()) {
    FastRandomContext insecure_rand(true);
    CCheckQueueControl<PrevectorJob> control(&queue);
    std::vector<std::vector<PrevectorJob>> vBatches(BATCHES);
    for (auto &vChecks : vBatches) {
      vChecks.reserve(BATCH_SIZE);
      for (size_t x = 0; x < BATCH_SIZE; ++x)
        vChecks.emplace_back(insecure_rand);
      control.Add(vChecks);
    }

    control.Wait();
  }
  tg.interrupt_all();
  tg.join_all();
}
BENCHMARK(CCheckQueueSpeedPrevectorJob, 1400);
