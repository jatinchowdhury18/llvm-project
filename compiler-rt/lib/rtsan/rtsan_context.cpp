//===--- rtsan_context.cpp - Realtime Sanitizer -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "rtsan/rtsan_context.h"
#include "rtsan/rtsan.h"

#include "sanitizer_common/sanitizer_allocator_internal.h"

#include <new>

#if ! SANITIZER_WINDOWS
#include <pthread.h>
#else
#include <stdio.h>
#include <windows.h>
#endif

using namespace __sanitizer;
using namespace __rtsan;

#if ! SANITIZER_WINDOWS
static pthread_key_t context_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
#else
static DWORD context_key = 0;
#endif

// InternalFree cannot be passed directly to pthread_key_create
// because it expects a signature with only one arg
static void InternalFreeWrapper(void *ptr) { __sanitizer::InternalFree(ptr); }

static __rtsan::Context &GetContextForThisThreadImpl() {
#if ! SANITIZER_WINDOWS
  auto MakeThreadLocalContextKey = []() {
    CHECK_EQ(pthread_key_create(&context_key, InternalFreeWrapper), 0);
  };

  pthread_once(&key_once, MakeThreadLocalContextKey);
  Context *current_thread_context =
      static_cast<Context *>(pthread_getspecific(context_key));
  if (current_thread_context == nullptr) {
    current_thread_context =
        static_cast<Context *>(InternalAlloc(sizeof(Context)));
    new (current_thread_context) Context();
    pthread_setspecific(context_key, current_thread_context);
  }

  return *current_thread_context;
#else
  if (context_key == 0) {
    printf ("Allocating context key\n");
    context_key = TlsAlloc();
    CHECK_NE(context_key, TLS_OUT_OF_INDEXES);
  }

  printf ("Retrieving context\n");
  Context *current_thread_context =
    static_cast<Context *>(TlsGetValue(context_key));
  if (current_thread_context == nullptr) {
    printf ("Allocating context\n");
    current_thread_context =
        static_cast<Context *>(InternalAlloc(sizeof(Context)));
    new (current_thread_context) Context();
    TlsSetValue(context_key, current_thread_context);
  }
  printf ("Using context %p\n", current_thread_context);

  return *current_thread_context;

  // @TODO:
  // - call TlsFree when we're done with the context key
  // - call InternalFree for the context

#endif
}

__rtsan::Context::Context() = default;

void __rtsan::Context::RealtimePush() { realtime_depth_++; }

void __rtsan::Context::RealtimePop() { realtime_depth_--; }

void __rtsan::Context::BypassPush() { bypass_depth_++; }

void __rtsan::Context::BypassPop() { bypass_depth_--; }

bool __rtsan::Context::InRealtimeContext() const { return realtime_depth_ > 0; }

bool __rtsan::Context::IsBypassed() const { return bypass_depth_ > 0; }

Context &__rtsan::GetContextForThisThread() {
  return GetContextForThisThreadImpl();
}
