// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCHEDULER_H
#define BITCOIN_SCHEDULER_H

#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

#include <sync.h>

class CScheduler {
public:
  CScheduler();
  ~CScheduler();

  typedef std::function<void(void)> Function;

  void schedule(Function f, boost::chrono::system_clock::time_point t =
                                boost::chrono::system_clock::now());

  void scheduleFromNow(Function f, int64_t deltaMilliSeconds);

  void scheduleEvery(Function f, int64_t deltaMilliSeconds);

  void serviceQueue();

  void stop(bool drain = false);

  size_t getQueueInfo(boost::chrono::system_clock::time_point &first,
                      boost::chrono::system_clock::time_point &last) const;

  bool AreThreadsServicingQueue() const;

private:
  std::multimap<boost::chrono::system_clock::time_point, Function> taskQueue;
  boost::condition_variable newTaskScheduled;
  mutable boost::mutex newTaskMutex;
  int nThreadsServicingQueue;
  bool stopRequested;
  bool stopWhenEmpty;
  bool shouldStop() const {
    return stopRequested || (stopWhenEmpty && taskQueue.empty());
  }
};

class SingleThreadedSchedulerClient {
private:
  CScheduler *m_pscheduler;

  CCriticalSection m_cs_callbacks_pending;
  std::list<std::function<void(void)>> m_callbacks_pending;
  bool m_are_callbacks_running = false;

  void MaybeScheduleProcessQueue();
  void ProcessQueue();

public:
  explicit SingleThreadedSchedulerClient(CScheduler *pschedulerIn)
      : m_pscheduler(pschedulerIn) {}
  void AddToProcessQueue(std::function<void(void)> func);

  void EmptyQueue();

  size_t CallbacksPending();
};

#endif
