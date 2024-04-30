#include <fmt/format.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <thread>

#include "static_config.hpp"

void debug_print_array(uint8_t* arr, uint32_t size) {
  if (spdlog::get_level() != spdlog::level::debug) {
    return;
  }
  std::string debug_data_first;
  std::string debug_data_last;
  debug_data_first.reserve(100);
  debug_data_last.reserve(100);
  auto* iov_base_data = static_cast<uint8_t*>(arr);
  for (int j = 0; j < 30 && j < size; ++j) {
    debug_data_first += fmt::format("{:02X} ", iov_base_data[j]);
  }
  for (int j = size - 30; j < size; ++j) {
    debug_data_last += fmt::format("{:02X} ", iov_base_data[j]);
  }
  spdlog::debug("First 30 and last 30 bytes: {} ... {}", debug_data_first,
                debug_data_last);
}

void configure_socket_to_not_fragment(int socket) {
  int val = IP_PMTUDISC_DO;
  setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
  int one = 1;
  setsockopt(socket, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
}

#define IO_URING_QUEUE_DEPTH 512

void setup_io_uring(struct io_uring& ring) {
  struct io_uring_params params {};
  memset(&params, 0, sizeof(params));
  params.flags = IORING_SETUP_SQPOLL;
  params.sq_thread_idle = 10000;

  int r = io_uring_queue_init_params(IO_URING_QUEUE_DEPTH, &ring, &params);
  if (r < 0) {
    spdlog::critical("Failed to initialize io_uring: {}", r);
    exit(1);
  }
}
