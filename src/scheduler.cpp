// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <scheduler.h>

#include <random.h>
#include <reverselock.h>

#include <assert.h>
#include <boost/bind.hpp>
#include <utility>

CScheduler::CScheduler()
    : nThreadsServicingQueue(0), stopRequested(false), stopWhenEmpty(false) {}

CScheduler::~CScheduler() { assert(nThreadsServicingQueue == 0); }

#if BOOST_VERSION < 105000
static boost::system_time
toPosixTime(const boost::chrono::system_clock::time_point &t) {
  return boost::posix_time::from_time_t(0) +
         boost::posix_time::milliseconds(
             boost::chrono::duration_cast<boost::chrono::milliseconds>(
                 t.time_since_epoch())
                 .count());
}
#endif

void CScheduler::serviceQueue() {
  boost::unique_lock<boost::mutex> lock(newTaskMutex);
  ++nThreadsServicingQueue;

  while (!shouldStop()) {
    try {
      if (!shouldStop() && taskQueue.empty()) {
        reverse_lock<boost::unique_lock<boost::mutex>> rlock(lock);

        RandAddSeedSleep();
      }
      while (!shouldStop() && taskQueue.empty()) {
        newTaskScheduled.wait(lock);
      }

#if BOOST_VERSION < 105000
      while (!shouldStop() && !taskQueue.empty() &&
             newTaskScheduled.timed_wait(
                 lock, toPosixTime(taskQueue.begin()->first))) {
      }
#else

      while (!shouldStop() && !taskQueue.empty()) {
        boost::chrono::system_clock::time_point timeToWaitFor =
            taskQueue.begin()->first;
        if (newTaskScheduled.wait_until<>(lock, timeToWaitFor) ==
            boost::cv_status::timeout)
          break;
      }
#endif

      if (shouldStop() || taskQueue.empty())
        continue;

      Function f = taskQueue.begin()->second;
      taskQueue.erase(taskQueue.begin());

      {
        reverse_lock<boost::unique_lock<boost::mutex>> rlock(lock);
        f();
      }
    } catch (...) {
      --nThreadsServicingQueue;
      throw;
    }
  }
  --nThreadsServicingQueue;
  newTaskScheduled.notify_one();
}

void CScheduler::stop(bool drain) {
  {
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    if (drain)
      stopWhenEmpty = true;
    else
      stopRequested = true;
  }
  newTaskScheduled.notify_all();
}

void CScheduler::schedule(CScheduler::Function f,
                          boost::chrono::system_clock::time_point t) {
  {
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    taskQueue.insert(std::make_pair(t, f));
  }
  newTaskScheduled.notify_one();
}

void CScheduler::scheduleFromNow(CScheduler::Function f,
                                 int64_t deltaMilliSeconds) {
  schedule(f, boost::chrono::system_clock::now() +
                  boost::chrono::milliseconds(deltaMilliSeconds));
}

static void Repeat(CScheduler *s, CScheduler::Function f,
                   int64_t deltaMilliSeconds) {
  f();
  s->scheduleFromNow(boost::bind(&Repeat, s, f, deltaMilliSeconds),
                     deltaMilliSeconds);
}

void CScheduler::scheduleEvery(CScheduler::Function f,
                               int64_t deltaMilliSeconds) {
  scheduleFromNow(boost::bind(&Repeat, this, f, deltaMilliSeconds),
                  deltaMilliSeconds);
}

size_t
CScheduler::getQueueInfo(boost::chrono::system_clock::time_point &first,
                         boost::chrono::system_clock::time_point &last) const {
  boost::unique_lock<boost::mutex> lock(newTaskMutex);
  size_t result = taskQueue.size();
  if (!taskQueue.empty()) {
    first = taskQueue.begin()->first;
    last = taskQueue.rbegin()->first;
  }
  return result;
}

bool CScheduler::AreThreadsServicingQueue() const {
  boost::unique_lock<boost::mutex> lock(newTaskMutex);
  return nThreadsServicingQueue;
}

void SingleThreadedSchedulerClient::MaybeScheduleProcessQueue() {
  {
    LOCK(m_cs_callbacks_pending);

    if (m_are_callbacks_running)
      return;
    if (m_callbacks_pending.empty())
      return;
  }
  m_pscheduler->schedule(
      std::bind(&SingleThreadedSchedulerClient::ProcessQueue, this));
}

void SingleThreadedSchedulerClient::ProcessQueue() {
  std::function<void(void)> callback;
  {
    LOCK(m_cs_callbacks_pending);
    if (m_are_callbacks_running)
      return;
    if (m_callbacks_pending.empty())
      return;
    m_are_callbacks_running = true;

    callback = std::move(m_callbacks_pending.front());
    m_callbacks_pending.pop_front();
  }

  struct RAIICallbacksRunning {
    SingleThreadedSchedulerClient *instance;
    explicit RAIICallbacksRunning(SingleThreadedSchedulerClient *_instance)
        : instance(_instance) {}
    ~RAIICallbacksRunning() {
      {
        LOCK(instance->m_cs_callbacks_pending);
        instance->m_are_callbacks_running = false;
      }
      instance->MaybeScheduleProcessQueue();
    }
  } raiicallbacksrunning(this);

  callback();
}

void SingleThreadedSchedulerClient::AddToProcessQueue(
    std::function<void(void)> func) {
  assert(m_pscheduler);

  {
    LOCK(m_cs_callbacks_pending);
    m_callbacks_pending.emplace_back(std::move(func));
  }
  MaybeScheduleProcessQueue();
}

void SingleThreadedSchedulerClient::EmptyQueue() {
  assert(!m_pscheduler->AreThreadsServicingQueue());
  bool should_continue = true;
  while (should_continue) {
    ProcessQueue();
    LOCK(m_cs_callbacks_pending);
    should_continue = !m_callbacks_pending.empty();
  }
}

size_t SingleThreadedSchedulerClient::CallbacksPending() {
  LOCK(m_cs_callbacks_pending);
  return m_callbacks_pending.size();
}
