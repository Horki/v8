// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access-inl.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

ThreadIsolation::TrustedData ThreadIsolation::trusted_data_;
ThreadIsolation::UntrustedData ThreadIsolation::untrusted_data_;

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT
thread_local int RwxMemoryWriteScope::code_space_write_nesting_level_ = 0;
#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT

#if V8_HAS_PKU_JIT_WRITE_PROTECT

bool RwxMemoryWriteScope::IsPKUWritable() {
  DCHECK(ThreadIsolation::initialized());
  return base::MemoryProtectionKey::GetKeyPermission(ThreadIsolation::pkey()) ==
         base::MemoryProtectionKey::kNoRestrictions;
}

void RwxMemoryWriteScope::SetDefaultPermissionsForSignalHandler() {
  DCHECK(ThreadIsolation::initialized());
  if (!RwxMemoryWriteScope::IsSupportedUntrusted()) return;
  base::MemoryProtectionKey::SetPermissionsForKey(
      ThreadIsolation::untrusted_pkey(),
      base::MemoryProtectionKey::kDisableWrite);
}

#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

RwxMemoryWriteScopeForTesting::RwxMemoryWriteScopeForTesting()
    : RwxMemoryWriteScope("For Testing") {}

RwxMemoryWriteScopeForTesting::~RwxMemoryWriteScopeForTesting() {}

// static
void ThreadIsolation::Initialize(
    ThreadIsolatedAllocator* thread_isolated_allocator) {
#if DEBUG
  untrusted_data_.initialized = true;
#endif

  if (!thread_isolated_allocator) {
    return;
  }

  if (v8_flags.jitless) {
    return;
  }

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  if (!base::MemoryProtectionKey::InitializeMemoryProtectionKeySupport()) {
    return;
  }
#endif

  // Check that our compile time assumed page size that we use for padding was
  // large enough.
  CHECK_GE(THREAD_ISOLATION_ALIGN_SZ,
           GetPlatformPageAllocator()->CommitPageSize());

  trusted_data_.allocator = thread_isolated_allocator;

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  trusted_data_.pkey = trusted_data_.allocator->Pkey();
  untrusted_data_.pkey = trusted_data_.pkey;
  base::MemoryProtectionKey::SetPermissionsAndKey(
      GetPlatformPageAllocator(),
      {reinterpret_cast<Address>(&trusted_data_), sizeof(trusted_data_)},
      v8::PageAllocator::Permission::kRead, trusted_data_.pkey);
#endif
}

}  // namespace internal
}  // namespace v8
