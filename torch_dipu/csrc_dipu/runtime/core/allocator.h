// Copyright (c) 2023, DeepLink.
#pragma once

#include <c10/core/Allocator.h>
#include <c10/core/Device.h>

#include <csrc_dipu/common.h>
#include <csrc_dipu/runtime/device/deviceapis.h>
#include <csrc_dipu/runtime/core/MemChecker.h>
#include <iostream>

namespace dipu {


#define DIPU_DEBUG_ALLOCATOR(x)                                                                 \
  {                                                                                             \
    static bool enable = std::getenv("DIPU_DEBUG_ALLOCATOR") != nullptr;                        \
    if (enable)                                                                                 \
    {                                                                                           \
      std::cout << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " " << x << std::endl; \
    }                                                                                           \
  }


static void DIPUDeleter(void* ptr) {
  if (ptr) {
    MemChecker::instance().erase(ptr);
    DIPU_DEBUG_ALLOCATOR("devapis::freeDevice: free " << ptr);
    devapis::freeDevice(ptr);
    ptr = nullptr;
  }
}


class DIPU_API DIPUAllocator: public c10::Allocator {
public:
  DIPUAllocator() {
    auto device = devapis::current_device();
    devapis::setDevice(device);
  }

  inline virtual c10::DataPtr allocate(size_t size) const {
    auto idx = devapis::current_device();
    return this->allocate(size, idx);
  }
  c10::DeleterFnPtr raw_deleter() const override {
    return &DIPUDeleter;
  }

private:
  static std::mutex mutex_;
  c10::DataPtr allocate(size_t nbytes, c10::DeviceIndex device_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    void* data = nullptr;
    devapis::mallocDevice(&data, nbytes);
    DIPU_DEBUG_ALLOCATOR("devapis::mallocDevice: malloc " << nbytes << " nbytes, ptr:" << data);
    MemChecker::instance().insert(data, nbytes);
    return {data, data, &DIPUDeleter, c10::Device(dipu::DIPU_DEVICE_TYPE, device_index)};
  }
};


void setAllocator(const std::string name, c10::DeviceType device_type, c10::Allocator* allocator, uint8_t priority = 0);
c10::Allocator* getAllocator(c10::DeviceType device_type);

struct AllocatorRegisterer {
  explicit AllocatorRegisterer(const std::string name, c10::DeviceType device_type, c10::Allocator* allocator, uint8_t priority = 0) {
    setAllocator(name, device_type, allocator, priority);
  }
};

#define DIPU_REGISTER_ALLOCATOR(name, device_type, RawAllocator, CachingAllocator, priority)        \
  namespace {                                                                                       \
  static RawAllocator raw_allocator;                                                                \
  static CachingAllocator cache_allocator(&raw_allocator);                                          \
  static AllocatorRegisterer g_allocator_d(name, device_type, &cache_allocator, priority);          \
  }

}  // namespace dipu
