#include <arpa/inet.h>
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

#include "buffer_pool.hpp"
#include "simple_consts.hpp"

void debug_print_array(uint8_t* arr, uint32_t size) {
  std::ostringstream debug_data_first;
  std::ostringstream debug_data_last;

  auto* iov_base_data = static_cast<uint8_t*>(arr);

  for (int j = 0; j < 24 && j < size; ++j) {
    debug_data_first << std::uppercase << std::setw(2) << std::setfill('0')
                     << std::hex << static_cast<int>(iov_base_data[j]) << " ";
  }

  if (size > 24) {
    for (int j = size - 24; j < size; ++j) {
      debug_data_last << std::uppercase << std::setw(2) << std::setfill('0')
                      << std::hex << static_cast<int>(iov_base_data[j]) << " ";
    }
  }

  std::cout << "[" << static_cast<void*>(arr) << "] "
            << "First 24 and last 24 bytes: " << debug_data_first.str()
            << " ... " << debug_data_last.str() << std::endl;
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
                       size_t thread_index, uint64_t* _total_received) {
  std::cout << "[" << thread_index << "] start_index: " << start_index
            << ", end_index: " << end_index << std::endl;

  std::vector buffer_sizes = {sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t),
                              sizeof(RequestData) + sizeof(int32_t)};
  BufferPool buffer_pool(buffer_sizes, BUFFER_POOL_INITIAL_POOL_SIZE);

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

  int iterations_received = 0;

  auto start_time = std::chrono::high_resolution_clock::now();

  int send_index = 0;
  int recv_index = 0;

  size_t total_received = 0;
  size_t total_expected_received = num_requests * PAGE_SIZE * sizeof(int32_t);
#if VERIFY
  std::vector<RequestData> recvs;
#endif
  size_t recv_req_num = 0;
  size_t send_req_num = 0;

  while (send_index < num_requests || recv_index < num_requests
         //         || iterations_received < num_requests || total_received <
         //         total_expected_received
  ) {
    // Submit send requests
    auto send_index_pre = send_index;
    while (send_index < num_requests &&
           send_index - recv_index < RING_SIZE / 4) {
#if VERBOSE
      std::cout << "[" << thread_index << "] send_index: " << send_index
                << std::endl;
#endif

      auto* request_data_send = (RequestData*)buffer_pool.allocate(
          sizeof(RequestData) + sizeof(int32_t));
      request_data_send->buffer[0] = start_index + send_index;
      request_data_send->seq[0] = thread_index;
      request_data_send->seq[1] = send_req_num++;
      request_data_send->event_type = SEND_EVENT;
      request_data_send->buffer_offset = 0;

      struct io_uring_sqe* sqe_send = io_uring_get_sqe(&ring);
      io_uring_prep_send(sqe_send, sock, request_data_send->buffer,
                         sizeof(int32_t), 0);
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
      auto* request_data_recv = (RequestData*)buffer_pool.allocate(
          sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t));
      request_data_recv->seq[0] = thread_index;
      request_data_recv->seq[1] = recv_req_num++;
      request_data_recv->event_type = RECV_EVENT;
      request_data_recv->buffer_offset = 0;
      struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
      io_uring_prep_recv(sqe_recv, sock, request_data_recv->buffer,
                         PAGE_SIZE * sizeof(int32_t), MSG_WAITALL);
      io_uring_sqe_set_data(sqe_recv, request_data_recv);

      recv_index++;
    }
    auto recv_index_diff = recv_index - recv_index_pre;

    // here we handle recvs for the remaining bytes
    //    if (send_index >= num_requests && recv_index >= num_requests) {
    //      size_t remaining_bytes = total_expected_received - total_received;
    //      size_t recvs_to_submit = std::min(remaining_bytes / PAGE_SIZE,
    //                                        (size_t)io_uring_sq_space_left(&ring));
    //      std::cout << "[" << thread_index
    //                << "] Remaining bytes: " << remaining_bytes
    //                << ". Recvs to submit: " << recvs_to_submit
    //                << ". Space in ring: " << io_uring_sq_space_left(&ring)
    //                << std::endl;
    //      for (size_t i = 0; i < recvs_to_submit; i++) {
    // #if VERBOSE
    //        std::cout << "[" << thread_index << "] recv_index: " << recv_index
    //                  << std::endl;
    // #endif
    //        auto* recv_buffer = new int32_t[PAGE_SIZE];
    //        struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
    //        io_uring_prep_recv(sqe_recv, sock, recv_buffer,
    //                           PAGE_SIZE * sizeof(int32_t), MSG_WAITALL);
    //
    //        auto* request_data_recv = new RequestData{
    //            {thread_index, recv_req_num++}, RECV_EVENT, recv_buffer, 0};
    //        io_uring_sqe_set_data(sqe_recv, request_data_recv);
    //
    //        recv_index++;
    //      }
    //      io_uring_submit(&ring);
    //    }
    //
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

#if VERBOSE
      size_t seq = data->seq[1];
      std::cout << "[" << thread_index << "] type: " << data->event_type
                << "; seq: " << seq << std::endl;
#endif

      if (cqe->res < 0) {
        if (data->event_type == SEND_EVENT) {
          std::cout << "[" << thread_index
                    << "] Send failed: " << strerror(-cqe->res) << std::endl;
        } else if (data->event_type == RECV_EVENT) {
          std::cout << "[" << thread_index
                    << "] Receive failed: " << strerror(-cqe->res) << std::endl;
        } else {
          std::cout << "[" << thread_index
                    << "] Unknown event type: " << data->event_type
                    << std::endl;
        }
        exit(EXIT_FAILURE);
      }

      // we know the call was processed without errors, so, res has the size of
      // bytes read now
      data->buffer_offset = cqe->res;
      if (data->event_type == RECV_EVENT) {
        total_received += cqe->res;
#if VERIFY
        recvs.push_back(*data);
#endif
#if VERBOSE
        std::cout << "[" << thread_index << "] Received " << cqe->res
                  << " bytes. Total received: " << total_received
                  << ". Total expected: " << total_expected_received
                  << std::endl;
#endif
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
      }

#if !VERIFY
      if (data->event_type == SEND_EVENT) {
        buffer_pool.deallocate((char*)data,
                               sizeof(RequestData) + sizeof(int32_t));
      } else if (data->event_type == RECV_EVENT) {
        buffer_pool.deallocate(
            (char*)data, sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t));
      }
      // delete[] data->buffer;
      // delete data;
#endif
      count++;
    }

    io_uring_cq_advance(&ring, count);

    if (total_received >= total_expected_received &&
        send_index >= num_requests) {
      std::cout << "[" << thread_index
                << "] !!! Total received: " << total_received
                << ". Total expected: " << total_expected_received << std::endl;
      break;
    }
  }

  std::chrono::duration<double> elapsed =
      std::chrono::high_resolution_clock::now() - start_time;
  double it_per_second = (double)num_requests / elapsed.count();

#if VERIFY
  std::sort(recvs.begin(), recvs.end(),
            [](const RequestData& a, const RequestData& b) {
              return a.seq[1] < b.seq[1];
            });
  for (auto& recv : recvs) {
    auto* data = recv.buffer;
#if VERBOSE
    std::cout << "[" << thread_index << "] Received data ("
              << recv.buffer_offset << " bytes) for seq: " << recv.seq[1]
              << std::endl;
    debug_print_array(
        reinterpret_cast<uint8_t*>(data),
        std::min((ssize_t)(PAGE_SIZE * sizeof(int32_t)), recv.buffer_offset));
#endif
    delete[] data;
  }
#endif

  std::cout << "[" << thread_index << "] Diff: "
            << 1.0 - (double)total_received / (double)total_expected_received
            << " (Total received: " << total_received
            << ". Total expected: " << total_expected_received << ")"
            << std::endl;
  std::cout << "[" << thread_index << "] Total time: " << elapsed.count()
            << " s" << std::endl;
  std::cout << "[" << thread_index << "] Average speed: "
            << it_per_second * PAGE_SIZE * sizeof(int32_t) * 8 / 1e9 << " Gbps"
            << std::endl;

  io_uring_queue_exit(&ring);
  std::cout << "[" << thread_index
            << "] Iterations received: " << iterations_received << std::endl;
  close(sock);

  if (_total_received) {
    *_total_received = total_received;
  }
}

int main() {
  size_t client_threads = CLIENT_THREADS;
  std::cout << "Starting " << client_threads << " client threads" << std::endl;
  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<uint64_t> total_received(client_threads, 0);
  std::vector<std::thread> threads;
  size_t requests_per_thread = NUM_REQUESTS / client_threads;
  for (size_t i = 0; i < client_threads; i++) {
    size_t start_index = i * requests_per_thread;
    size_t end_index = (i == client_threads - 1)
                           ? NUM_REQUESTS
                           : (i + 1) * requests_per_thread;
    std::cout << "Starting thread " << i << " for range " << start_index << " "
              << end_index << std::endl;
    threads.emplace_back(send_receive_data, start_index, end_index, i,
                         &total_received[i]);
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
  //  std::cout << "Average Gbps: " << avg_gbps << std::endl;

  uint64_t total_received_bytes = 0;
  for (size_t i = 0; i < client_threads; i++) {
    total_received_bytes += total_received[i];
  }
  double total_received_gbps = total_received_bytes * 8 / 1e9 / total_time;
  uint64_t expected_total_received_bytes =
      1u * NUM_REQUESTS * PAGE_SIZE * sizeof(int32_t);
  std::cout << "Total received: " << total_received_bytes << " bytes"
            << std::endl;
  std::cout << "Expected total received: " << expected_total_received_bytes
            << " bytes" << std::endl;
  //  std::cout << "Total received Gbps: " << total_received_gbps << std::endl;
  std::cout << "Average Gbps: " << total_received_gbps << std::endl;

  return 0;
}
