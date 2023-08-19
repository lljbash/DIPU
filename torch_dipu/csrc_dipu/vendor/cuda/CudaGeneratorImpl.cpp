// Copyright (c) 2023, DeepLink.
#include <ATen/Utils.h>

#include <csrc_dipu/runtime/core/DIPUGeneratorImpl.h>

#include <iostream>
namespace dipu {

static const size_t states_size = 200 * sizeof(4120);
static const size_t seed_size = sizeof(uint64_t);
static const size_t offset_size = sizeof(int64_t);
static const size_t total_size = states_size + seed_size + offset_size;

class CUDAGeneratorImpl : public dipu::DIPUGeneratorImpl {
public:
  CUDAGeneratorImpl(at::DeviceIndex device_index): dipu::DIPUGeneratorImpl(device_index) {
  }

  // void init_state() const override {
  //   std::cout << "enter into " << __FILE__ << ":" << __FUNCTION__ << std::endl;
  //   // resize and set the state tensor.
  //   std::call_once(init_state_flag, [&] {
  //     init_state_from_seed();
  //     std::cout << "init state, state size=" << total_size << std::endl;
  //   });
  // }

  void set_state(const c10::TensorImpl& state) override {
    std::cout << "enter into " << __FILE__ << ":" << __FUNCTION__ << std::endl;
    at::detail::check_rng_state(state);
    at::Tensor state_tmp(state.shallow_copy_and_detach(state.version_counter(), true));
    state_ = state_tmp;
    state_need_reset_ = false;
  }

  void update_state() const override {
    std::cout << "enter into " << __FILE__ << ":" << __FUNCTION__ << std::endl;
    if (state_need_reset_) {
      state_ = at::detail::empty_cpu({(int64_t)total_size}, c10::ScalarType::Byte, c10::nullopt, c10::nullopt, c10::nullopt, c10::nullopt);
      auto rng_state = state_.data_ptr<uint8_t>();
      // since curandStateMTGP is not used anymore, fill gen_states of THCGenerator with deterministic garbage value of -1
      // gen_states in THCGenerator struct was an array of curandStateMtgp32s.
      memset(rng_state, -1, states_size);
      uint64_t current_seed = this->current_seed();
      int64_t offset = 0;
      memcpy(rng_state + states_size, &current_seed, seed_size);
      memcpy(rng_state + states_size + seed_size, &offset, offset_size);
      std::cout << "finish update state, state_.scalar_type() = " << state_.scalar_type() << std::endl;
    }
  }
};

const at::Generator vendorMakeGenerator(at::DeviceIndex device_index) {
  std::cout << "enter into " << __FILE__ << ":" << __FUNCTION__ << std::endl;
  return at::make_generator<CUDAGeneratorImpl>(device_index);
}

}  // namespace torch_dipu