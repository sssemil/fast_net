#include <arpa/inet.h>
#include <fmt/format.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "simple_consts.hpp"

enum EventType { SEND_EVENT, RECV_EVENT };

struct RequestData {
  EventType event_type;
  int32_t* buffer;
};

void debug_print_array(uint8_t* arr, uint32_t size) {
  std::string debug_data_first;
  std::string debug_data_last;
  debug_data_first.reserve(100);
  debug_data_last.reserve(100);
  auto* iov_base_data = static_cast<uint8_t*>(arr);
  for (int j = 0; j < 24 && j < size; ++j) {
    debug_data_first += fmt::format("{:02X} ", iov_base_data[j]);
  }
  for (int j = size - 24; j < size; ++j) {
    debug_data_last += fmt::format("{:02X} ", iov_base_data[j]);
  }
  std::cout << "First 30 and last 30 bytes: " << debug_data_first << " ... "
            << debug_data_last << std::endl;
}

int setup_socket() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    std::cout << "Socket creation failed" << std::endl;
    return -1;
  }

  struct sockaddr_in serv_addr {};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr);

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
    std::cout << "Connection failed" << std::endl;
    close(sock);
    return -1;
  }

  return sock;
}

void send_receive_data(size_t start_index, size_t end_index,
                       size_t thread_index) {
  std::cout << "[" << thread_index << "] start_index: " << start_index
            << ", end_index: " << end_index << std::endl;
  int sock = setup_socket();
  if (sock < 0) {
    std::cout << "[" << thread_index << "] Failed to connect to server"
              << std::endl;
    exit(EXIT_FAILURE);
  }
  size_t num_requests = end_index - start_index;
  struct io_uring ring {};
  int r = io_uring_queue_init(RING_SIZE, &ring, IORING_SETUP_SINGLE_ISSUER);
  if (r < 0) {
    std::cout << "[" << thread_index
              << "] io_uring_queue_init failed: " << strerror(-r) << std::endl;
    exit(EXIT_FAILURE);
  }

#if VERIFY
  int correct_responses = 0;
  int incorrect_responses = 0;
  bool* received = new bool[num_requests]();
#endif

  int iterations_received = 0;

  auto start_time = std::chrono::high_resolution_clock::now();

  int send_index = 0;
  int recv_index = 0;

  while (send_index < num_requests || recv_index < num_requests ||
         iterations_received < num_requests) {
    // Submit send requests
    auto send_index_pre = send_index;
    while (send_index < num_requests &&
           send_index - recv_index < RING_SIZE / 2) {
#if VERBOSE
      std::cout << "[" << thread_index << "] send_index: " << send_index
                << std::endl;
#endif
      auto* send_buffer = new int32_t(start_index + send_index);
      struct io_uring_sqe* sqe_send = io_uring_get_sqe(&ring);
      io_uring_prep_send(sqe_send, sock, send_buffer, sizeof(int32_t), 0);

      auto* request_data_send = new RequestData{SEND_EVENT, send_buffer};
      io_uring_sqe_set_data(sqe_send, request_data_send);

      send_index++;
    }
    auto send_index_diff = send_index - send_index_pre;

    // Submit receive requests
    auto recv_index_pre = recv_index;
    while (recv_index < send_index &&
           recv_index - iterations_received < RING_SIZE / 2) {
#if VERBOSE
      std::cout << "[" << thread_index << "] recv_index: " << recv_index
                << std::endl;
#endif
      auto* recv_buffer = new int32_t[PAGE_SIZE];
      struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
      io_uring_prep_recv(sqe_recv, sock, recv_buffer,
                         PAGE_SIZE * sizeof(int32_t), MSG_WAITALL);

      auto* request_data_recv = new RequestData{RECV_EVENT, recv_buffer};
      io_uring_sqe_set_data(sqe_recv, request_data_recv);

      recv_index++;
    }
    auto recv_index_diff = recv_index - recv_index_pre;

    if (send_index_diff != 0 || recv_index_diff != 0) {
#if VERBOSE
      std::cout << "[" << thread_index
                << "] Ring space left: " << io_uring_sq_space_left(&ring)
                << std::endl;
#endif
      io_uring_submit(&ring);
    }

    // Process completed requests
    struct io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;

    io_uring_for_each_cqe(&ring, head, cqe) {
      auto* data = static_cast<RequestData*>(io_uring_cqe_get_data(cqe));

      if (cqe->res < 0) {
        if (data->event_type == SEND_EVENT) {
          std::cout << "[" << thread_index
                    << "] Send failed: " << strerror(-cqe->res) << std::endl;
        } else {
          std::cout << "[" << thread_index
                    << "] Receive failed: " << strerror(-cqe->res) << std::endl;
        }
      } else if (data->event_type == RECV_EVENT) {
        iterations_received++;
        if (iterations_received % 10000 == 0) {
          auto iter_per_second =
              iterations_received /
              std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - start_time)
                  .count();
          std::cout << "[" << thread_index
                    << "] Iterations received: " << iterations_received << " ["
                    << iter_per_second << " it/s]" << std::endl;
        }

        if (cqe->res != PAGE_SIZE * sizeof(int32_t)) {
          std::cout << "[" << thread_index
                    << "] Received incorrect number of bytes: " << cqe->res
                    << " (first uint32 hex: " << std::hex << data->buffer[0]
                    << ")" << std::endl;
          exit(EXIT_FAILURE);
        }

#if VERIFY
        // Validate response
        bool correct = true;
        int32_t first = data->buffer[0];
        for (int j = 0; j < PAGE_SIZE; ++j) {
          if (data->buffer[j] != first) {
            std::cout << "[" << thread_index << "] Incorrect response at index "
                      << iterations_received - 1 << ", value "
                      << data->buffer[j] << std::endl;
            correct = false;
            break;
          }
        }
        if (first < start_index || first >= end_index) {
          std::cout << "[" << thread_index
                    << "] Received out-of-range response at index " << first
                    << std::endl;
          correct = false;
        } else if (received[first - start_index]) {
          std::cout << "[" << thread_index
                    << "] Received duplicate response at index " << first
                    << std::endl;
          correct = false;
        }
        if (correct) {
          correct_responses++;
          received[first - start_index] = true;
        } else {
          incorrect_responses++;
          debug_print_array(reinterpret_cast<uint8_t*>(data->buffer),
                            PAGE_SIZE * sizeof(int32_t));
        }
#endif
      }

      delete[] data->buffer;
      delete data;

      count++;
    }

    io_uring_cq_advance(&ring, count);
  }

  std::chrono::duration<double> elapsed =
      std::chrono::high_resolution_clock::now() - start_time;
  double it_per_second = (double)num_requests / elapsed.count();
  std::cout << "[" << thread_index << "] Total time: " << elapsed.count()
            << " s" << std::endl;
  std::cout << "[" << thread_index << "] Average speed: "
            << it_per_second * PAGE_SIZE * sizeof(int32_t) * 8 / 1e9 << " Gbps"
            << std::endl;

  io_uring_queue_exit(&ring);
  std::cout << "[" << thread_index
            << "] Iterations received: " << iterations_received << std::endl;
#if VERIFY
  for (int i = 0; i < num_requests; ++i) {
    if (!received[i]) {
      std::cout << "[" << thread_index << "] Missing response at index "
                << start_index + i << std::endl;
    }
  }
  std::cerr << "[" << thread_index << "] Total iterations: " << num_requests
            << std::endl;
  std::cerr << "[" << thread_index
            << "] Correct responses: " << correct_responses << std::endl;
  std::cerr << "[" << thread_index
            << "] Incorrect responses: " << incorrect_responses << std::endl;

  delete[] received;
#endif

  close(sock);
}

int main() {
  size_t client_threads = CLIENT_THREADS;
  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  size_t requests_per_thread = NUM_REQUESTS / client_threads;
  for (size_t i = 0; i < client_threads; i++) {
    size_t start_index = i * requests_per_thread;
    size_t end_index = (i == client_threads - 1)
                           ? NUM_REQUESTS
                           : (i + 1) * requests_per_thread;
    std::cout << "Starting thread " << i << " for range " << start_index << " "
              << end_index << std::endl;
    threads.emplace_back(send_receive_data, start_index, end_index, i);
  }

  for (auto& thread : threads) {
    thread.join();
  }

  double total_time =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now() - start_time)
          .count() /
      1e9;
  double avg_rate = (double)NUM_REQUESTS / total_time;
  double avg_gbps = avg_rate * PAGE_SIZE * sizeof(int32_t) * 8 / 1e9;
  std::cout << "Total time for " << NUM_REQUESTS << " requests: " << total_time
            << " s" << std::endl;
  std::cout << "Average rate: " << std::fixed << std::setprecision(2)
            << avg_rate << " it/s" << std::endl;
  std::cout << "Average Gbps: " << avg_gbps << std::endl;
  return 0;
}
