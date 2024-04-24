#include <fmt/format.h>
#include <netinet/in.h>
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
  std::string debug_data;
  debug_data.reserve(300);
  auto* iov_base_data = static_cast<uint8_t*>(arr);
  for (int j = 0; j < 100 && j < size; ++j) {
    debug_data += fmt::format("{:02X} ", iov_base_data[j]);
  }
  spdlog::debug("First 100 bytes: {}", debug_data);
}