// Copyright 2022 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "test/scoped_set_thread_name.h"

#include <errno.h>
#include <pthread.h>

#include <string>

#include "base/check.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <mach/thread_info.h>
#endif

namespace crashpad {
namespace test {

namespace {

#if BUILDFLAG(IS_LINUX)
// The kernel headers define this in linux/sched.h as TASK_COMM_LEN, but the
// userspace copy of that header does not define it.
constexpr size_t kPthreadNameMaxLen = 16;
#elif BUILDFLAG(IS_APPLE)
constexpr size_t kPthreadNameMaxLen = MAXTHREADNAMESIZE;
#else
#error Port to your platform
#endif

void SetCurrentThreadName(const std::string& thread_name) {
#if BUILDFLAG(IS_LINUX)
  PCHECK((errno = pthread_setname_np(pthread_self(), thread_name.c_str())) == 0)
      << "pthread_setname_np";
#elif BUILDFLAG(IS_APPLE)
  // Apple's pthread_setname_np() sets errno instead of returning it.
  PCHECK(pthread_setname_np(thread_name.c_str()) == 0) << "pthread_setname_np";
#else
#error Port to your platform
#endif
}

std::string GetCurrentThreadName() {
  std::string result(kPthreadNameMaxLen, '\0');
  PCHECK((errno = pthread_getname_np(
              pthread_self(), result.data(), result.length())) == 0)
      << "pthread_getname_np";
  const auto result_nul_idx = result.find('\0');
  CHECK(result_nul_idx != std::string::npos)
      << "pthread_getname_np did not NUL terminate";
  result.resize(result_nul_idx);
  return result;
}

}  // namespace

ScopedSetThreadName::ScopedSetThreadName(const std::string& new_thread_name)
    : original_name_(GetCurrentThreadName()) {
  SetCurrentThreadName(new_thread_name);
}

ScopedSetThreadName::~ScopedSetThreadName() {
  SetCurrentThreadName(original_name_);
}

}  // namespace test
}  // namespace crashpad
