#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "consts.hpp"
#include "memory_block.hpp"
#include "models/get_page.hpp"
#include "spdlog/spdlog.h"
#include "static_config.hpp"
#include "utils.hpp"

#define IO_URING_QUEUE_DEPTH 256

struct io_uring ring;

void setup_io_uring() {
  if (io_uring_queue_init(IO_URING_QUEUE_DEPTH, &ring, 0) < 0) {
    spdlog::critical("Failed to initialize io_uring");
    exit(1);
  }
}

int setup_socket(const char* addr, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    spdlog::error("Socket creation failed");
    return -1;
  }

  fcntl(sock, F_SETFL, O_NONBLOCK);

  struct sockaddr_in serv_addr {};
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  inet_pton(AF_INET, addr, &serv_addr.sin_addr);

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
    if (errno != EINPROGRESS) {
      spdlog::error("Connection failed");
      close(sock);
      return -1;
    }
  }

  return sock;
}

int main() {
  Config::load_config();
  MemoryBlockVerifier<PAGE_SIZE> verifier(Config::page_count,
                                          new PseudoRandomFillingStrategy());
  setup_io_uring();

  int sock = setup_socket("127.0.0.1", Config::port);
  if (sock < 0) return -1;

  srand(time(nullptr));  // NOLINT(*-msc51-cpp)
  auto start = std::chrono::high_resolution_clock::now();

  auto num_requests = Config::num_requests;
  uint32_t correct_responses = 0;
  uint32_t incorrect_responses = 0;

  for (int i = 0; i < num_requests; ++i) {
    struct io_uring_sqe * sqe_send = io_uring_get_sqe(&ring);

    // TODO: Free
    auto* request =
        static_cast<GetPageRequest*>(malloc(sizeof(GetPageRequest)));
    memset(request, 0xBB, sizeof(GetPageRequest));

    request->request_id = i;
    request->page_number = i % Config::page_count;
    // request->page_number = rand() % Config::page_count;  // NOLINT(*-msc50-cpp)
    request->to_network_order();
    io_uring_prep_send(sqe_send, sock, request, sizeof(GetPageRequest), 0);

    //////////////////////////////////////////////////////////

    struct io_uring_sqe *sqe_recv = io_uring_get_sqe(&ring);
    // TODO: Free
    auto* response =
        static_cast<GetPageResponse*>(malloc(sizeof(GetPageResponse)));
    memset(response, 0xCC, sizeof(GetPageResponse));

    io_uring_prep_recv(sqe_recv, sock, response, sizeof(GetPageResponse), 0);

    // Linking send and receive SQEs
    sqe_send->flags |= IOSQE_IO_LINK;

    io_uring_submit(&ring);

    ///////////////////////////////////////////////////////////

    struct io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
        spdlog::error("Send operation failed with error: {}", cqe->res);
    }
    io_uring_cqe_seen(&ring, cqe);

    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
        spdlog::error("Receive operation failed with error: {}", cqe->res);
    } else {
        response->to_host_order();
    }
    io_uring_cqe_seen(&ring, cqe);

    ///////////////////////////////////////////////////////////

    spdlog::debug("Response req id {0:#x}", response->header.request_id);
    spdlog::debug("Response status {0:#x}", response->header.status);
    spdlog::debug("Response page num {0:#x}", response->header.page_number);
    if (!verifier.verify(response->content, response->header.page_number)) {
      spdlog::error("Verification failed for page {}", response->header.page_number);
      debug_print_array((uint8_t*)response, response->content.size());
      incorrect_responses++;
    } else {
      spdlog::debug("Verification passed for page {}", response->header.page_number);
      correct_responses++;
    }
  }

  close(sock);
  io_uring_queue_exit(&ring);

  double total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now() - start)
                          .count();
  double avg_time = total_time / Config::num_requests;
  double req_per_sec = 1000.0 / avg_time;
  double total_time_seconds = total_time / 1000.0;
  const double avg_rate =
      static_cast<double>(Config::num_requests) / total_time_seconds;
  const double avg_gbps =
      avg_rate * sizeof(GetPageResponse) * 8 / (1000 * 1000 * 1000);

  spdlog::info("Correct responses: {}", correct_responses);
  spdlog::info("Incorrect responses: {}", incorrect_responses);
  spdlog::info("Total time for {} requests: {:.2f} ms", Config::num_requests,
               total_time);
  spdlog::info("Average time per request: {:.2f} ms", avg_time);
  spdlog::info("Requests per second: {:.2f}", req_per_sec);
  spdlog::info("Average rate: {:03.2f} req/s", avg_rate);
  spdlog::info("Average throughput: {:03.2f} Gb/s", avg_gbps);

  return 0;
}
