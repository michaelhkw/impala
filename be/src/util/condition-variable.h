// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef IMPALA_UTIL_CONDITION_VARIABLE_H
#define IMPALA_UTIL_CONDITION_VARIABLE_H

#include <boost/thread/pthread/timespec.hpp>
#include <boost/thread/mutex.hpp>
#include <pthread.h>
#include <unistd.h>

namespace impala {

/// Simple wrapper around POSIX pthread condition variable.
/// This has lower overhead than boost's implementation as it
/// doesn't have the logic to deal with thread interruption.
class CACHE_ALIGNED ConditionVariable {
 public:
  ConditionVariable() : num_waiters_(0) { pthread_cond_init(&cv_, NULL); }

  ~ConditionVariable() { pthread_cond_destroy(&cv_); }

  void inline Wait(boost::unique_lock<boost::mutex>& lock) {
    DCHECK(lock.owns_lock());
    ++num_waiters_;
    pthread_mutex_t* mutex = lock.mutex()->native_handle();
    pthread_cond_wait(&cv_, mutex);
    --num_waiters_;
  }

  bool inline TimedWait(boost::unique_lock<boost::mutex>& lock,
      const struct timespec* timeout) {
    DCHECK(lock.owns_lock());
    ++num_waiters_;
    pthread_mutex_t* mutex = lock.mutex()->native_handle();
    bool notified = pthread_cond_timedwait(&cv_, mutex, timeout) == 0;
    --num_waiters_;
    return notified;
  }

  void inline NotifyOne() { pthread_cond_signal(&cv_); }

  void inline NotifyAll() { pthread_cond_broadcast(&cv_); }

  void inline NotifyOneIfWaiting() {
    if (num_waiters_ > 0) NotifyOne();
  }

 private:
  pthread_cond_t cv_;
  int num_waiters_;
};

}
#endif
