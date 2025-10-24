// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <utiltime.h>
#include <validation.h>

static void Sleep100ms(benchmark::State &state) {
  while (state.KeepRunning()) {
    MilliSleep(100);
  }
}

BENCHMARK(Sleep100ms, 10);

#include <math.h>

volatile double sum = 0.0;

static void Trig(benchmark::State &state) {
  double d = 0.01;
  while (state.KeepRunning()) {
    sum += sin(d);
    d += 0.000001;
  }
}

BENCHMARK(Trig, 12 * 1000 * 1000);
