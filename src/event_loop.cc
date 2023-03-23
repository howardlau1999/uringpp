#include "uringpp/event_loop.h"

#include <memory>

namespace uringpp {

std::shared_ptr<event_loop> event_loop::create(unsigned int entries,
                                               uint32_t flags, int wq_fd) {
  return std::make_shared<event_loop>(entries, flags, wq_fd);
}

event_loop::event_loop(unsigned int entries, uint32_t flags, int wq_fd,
                       int sq_thread_cpu, int sq_thread_idle)
    : cqe_count_(0) {
  struct io_uring_params params = {};
  params.wq_fd = wq_fd > 0 ? wq_fd : 0;
  params.flags = flags;
  params.sq_thread_cpu = sq_thread_cpu;
  params.sq_thread_idle = sq_thread_idle;
  check_nerrno(::io_uring_queue_init_params(entries, &ring_, &params),
               "failed to init io uring");
  try {
    probe_ring probe(&ring_);
    supported_ops_ = probe.supported_ops();
  } catch (std::exception const &e) {
    ::io_uring_queue_exit(&ring_);
    throw;
  }
  init_supported_features(params);
}

event_loop::~event_loop() { ::io_uring_queue_exit(&ring_); }

event_loop::probe_ring::probe_ring(struct io_uring *ring) {
  probe_ = ::io_uring_get_probe_ring(ring);
  check_ptr(probe_, "failed to get probe ring");
}

std::bitset<IORING_OP_LAST> event_loop::probe_ring::supported_ops() {
  std::bitset<IORING_OP_LAST> ops;
  for (uint8_t i = 0; i < probe_->ops_len; ++i) {
    if (probe_->ops[i].flags & IO_URING_OP_SUPPORTED) {
      ops.set(probe_->ops[i].op);
    }
  }
  return ops;
}

event_loop::probe_ring::~probe_ring() { ::io_uring_free_probe(probe_); }

void event_loop::check_feature(uint32_t features, uint32_t test_bit,
                               enum feature efeat) {
  if (features & test_bit) {
    supported_features_.insert(efeat);
  }
}

void event_loop::init_supported_features(struct io_uring_params const &params) {
  check_feature(params.features, IORING_FEAT_SINGLE_MMAP, feature::SINGLE_MMAP);
  check_feature(params.features, IORING_FEAT_NODROP, feature::NODROP);
  check_feature(params.features, IORING_FEAT_SUBMIT_STABLE,
                feature::SUBMIT_STABLE);
  check_feature(params.features, IORING_FEAT_RW_CUR_POS, feature::RW_CUR_POS);
  check_feature(params.features, IORING_FEAT_CUR_PERSONALITY,
                feature::CUR_PERSONALITY);
  check_feature(params.features, IORING_FEAT_FAST_POLL, feature::FAST_POLL);
  check_feature(params.features, IORING_FEAT_POLL_32BITS, feature::POLL_32BITS);
  check_feature(params.features, IORING_FEAT_SQPOLL_NONFIXED,
                feature::SQPOLL_NONFIXED);
  check_feature(params.features, IORING_FEAT_EXT_ARG, feature::EXT_ARG);
  check_feature(params.features, IORING_FEAT_NATIVE_WORKERS,
                feature::NATIVE_WORKERS);
  check_feature(params.features, IORING_FEAT_RSRC_TAGS, feature::RSRC_TAGS);
}

} // namespace uringpp