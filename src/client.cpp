#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>

#include "consts.hpp"
#include "memory_block.hpp"
#include "models/get_page.hpp"
#include "spdlog/spdlog.h"
#include "static_config.hpp"

int main() {
  Config::load_config();

  const MemoryBlockVerifier<PAGE_SIZE> memory_block_verifier(
      Config::page_count, new PseudoRandomFillingStrategy());

  spdlog::info("Port: {}", Config::port);
  spdlog::info("Number of Requests: {}", Config::num_requests);

  sockaddr_in serv_addr{};
  int sock;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    spdlog::error("Socket creation error");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(Config::port);

  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    spdlog::error("Invalid address / Address not supported");
    return -1;
  }

  if (connect(sock, reinterpret_cast<sockaddr*>(&serv_addr),
              sizeof(serv_addr)) < 0) {
    spdlog::error("Connection failed");
    return -1;
  }

  // TODO: Consider using a constant seed for reproducibility
  srand(time(nullptr));  // NOLINT(*-msc51-cpp)
  std::chrono::duration<double, std::milli> total_time(0);

  GetPageRequest request{};
  GetPageResponse response{};
  for (int i = 0; i < Config::num_requests; ++i) {
    auto start = std::chrono::high_resolution_clock::now();

    const uint32_t page_number =
        rand() % Config::page_count;  // NOLINT(*-msc50-cpp)

    request.page_number = page_number;
    request.to_network_order();
    if (send(sock, &request, sizeof(request), 0) != sizeof(request)) {
      throw std::runtime_error("Error sending request to server");
    }

    const auto response_ptr = reinterpret_cast<uint8_t*>(&response);
    ssize_t read_count = 0;
    size_t read_pass = 0;
    while (read_count < sizeof(response)) {
      spdlog::debug("read_count: {} (read pass: {})", read_count, read_pass);
      read_pass++;
      const ssize_t curr_read_count =
          read(sock, response_ptr + read_count, sizeof(response) - read_count);
      if (curr_read_count < 0) {
        throw std::runtime_error("Error reading response from server");
      }
      if (curr_read_count == 0) {
        throw std::runtime_error("Connection closed by server");
      }
      read_count += curr_read_count;
    }

    if (read_count != sizeof(response)) {
      throw std::runtime_error("Incomplete response received");
    }

    response.to_host_order();

    if (response.get_status() != SUCCESS) {
      const auto error_string = std::string("Server reported error ") +
                                std::to_string(response.get_status()) +
                                " for page " + std::to_string(page_number);
      throw std::runtime_error(error_string);
    }

    auto end = std::chrono::high_resolution_clock::now();
    total_time += end - start;

    if (!memory_block_verifier.verify(response.content, page_number)) {
      throw std::runtime_error("Data verification failed for page " +
                               std::to_string(page_number));
    }

    if (i % 1000 == 999) {
      const double millis = total_time.count();
      const double rate = 1000 * ((i + 1) / millis);
      const double mbps = rate * sizeof(GetPageResponse) * 8 / (1000 * 1000 * 1000);
      spdlog::info("Processed {}/{} requests [{:03.2f} req/s][{:03.2f} Gb/s]", i + 1,
                   Config::num_requests, rate, mbps);
    }
  }

  const double total_time_seconds = total_time.count() / 1000;
  const double avg_time =
      total_time_seconds / static_cast<double>(Config::num_requests);
  const double avg_rate =
      static_cast<double>(Config::num_requests) / total_time_seconds;
  // TODO: Account for the full packet size?
  const double avg_gbps =
      avg_rate * sizeof(GetPageResponse) * 8 / (1000 * 1000 * 1000);
  spdlog::info("Average response time for {} requests: {} s",
               Config::num_requests, avg_time);
  spdlog::info("Average rate: {:03.2f} req/s", avg_rate);
  spdlog::info("Average throughput: {:03.2f} Gb/s", avg_gbps);

  close(sock);
  return 0;
}
