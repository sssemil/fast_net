#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>

#include "consts.h"
#include "memory_block.h"
#include "models/get_page.h"
#include "spdlog/spdlog.h"
#include "static_config.h"

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
  const auto start_total = std::chrono::high_resolution_clock::now();
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
                          std::to_string(response.get_status()) + " for page " +
                          std::to_string(page_number);
      throw std::runtime_error(error_string);
    }

    auto end = std::chrono::high_resolution_clock::now();
    total_time += end - start;

    if (!memory_block_verifier.verify(response.content, page_number)) {
      spdlog::error("Data verification failed for page {}", page_number);
    }

    if (i % 1000 == 0) {
      auto now = std::chrono::high_resolution_clock::now();
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                start_total)
              .count();
      const double seconds = static_cast<double>(elapsed) / 1000.0;
      const double rate = (i + 1) / seconds;
      spdlog::info("Processed {}/{} requests [{} req/s]", i,
                   Config::num_requests, rate);
    }
  }
  const auto end_total = std::chrono::high_resolution_clock::now();

  const double avg_time =
      total_time.count() / static_cast<double>(Config::num_requests);
  const double avg_loop_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_total -
                                                            start_total)
          .count() /
      static_cast<double>(Config::num_requests);
  spdlog::info("Average response time for {} requests: {} ms",
               Config::num_requests, avg_time);
  spdlog::info("Average loop time for {} loops: {} ms", Config::num_requests,
               avg_loop_time);

  close(sock);
  return 0;
}
