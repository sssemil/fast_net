#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#define _Atomic(X) std::atomic<X>
#endif

#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "buffer_pool.hpp"
#include "simple_consts.hpp"

void add_read_request(struct io_uring& ring, int client_socket, size_t seq1,
                      size_t seq2, RequestData* req) {
  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
  // auto* req = new RequestData{{seq1, seq2}, READ_EVENT, new int32_t, 0};
  req->seq[0] = seq1;
  req->seq[1] = seq2;
  req->event_type = READ_EVENT;
  req->buffer_offset = 0;

  io_uring_prep_read(sqe, client_socket, req->buffer, sizeof(int32_t), 0);
  io_uring_sqe_set_data(sqe, req);
}

void add_write_request(struct io_uring& ring, int client_socket, size_t seq1,
                       size_t seq2, RequestData* req) {
  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
  //  auto* req = new RequestData{{seq1, seq2}, WRITE_EVENT, data, 0};
  req->seq[0] = seq1;
  req->seq[1] = seq2;
  req->event_type = WRITE_EVENT;
  req->buffer_offset = 0;

  io_uring_prep_send(sqe, client_socket, req->buffer,
                     PAGE_SIZE * sizeof(int32_t), 0);
  io_uring_sqe_set_data(sqe, req);
}

bool event_loop(struct io_uring& ring, int client_socket, size_t client_num) {
  std::vector buffer_sizes = {sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t),
                              sizeof(RequestData) + sizeof(int32_t)};
  BufferPool buffer_pool(buffer_sizes, BUFFER_POOL_INITIAL_POOL_SIZE);
  size_t read_req_num = 0;
  size_t write_req_num = 0;
  for (int i = 0; i < RING_SIZE / 4; i++) {
    auto* response = (RequestData*)buffer_pool.allocate(
        sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t));
    add_read_request(ring, client_socket, client_num, read_req_num++, response);
  }
  io_uring_submit(&ring);

  while (true) {
    struct io_uring_cqe* cqe;
    io_uring_wait_cqe(&ring, &cqe);
    auto* req = (RequestData*)io_uring_cqe_get_data(cqe);
    if (!req) {
      std::cout << "Request is null" << std::endl;
      return false;
    }

    switch (req->event_type) {
      case READ_EVENT: {
        if (cqe->res == 0) {
          std::cout << "Client closed connection" << std::endl;
          close(client_socket);
          return true;
        }

        int32_t page_number;
        memcpy(&page_number, req->buffer, sizeof(int32_t));
#if VERIFY
        if (page_number > NUM_REQUESTS) {
          std::cout << "Requested invalid page number: " << page_number
                    << std::endl;
          return false;
        }
#endif

#if VERBOSE
        std::cout << "Requested page number: " << page_number << std::endl;
#endif

        auto* response = (RequestData*)buffer_pool.allocate(
            sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t));
        for (int i = 0; i < PAGE_SIZE; i++) {
          response->buffer[i] = page_number;
        }
        add_write_request(ring, client_socket, client_num, write_req_num++,
                          response);
        add_read_request(ring, client_socket, client_num, read_req_num++, req);
        io_uring_submit(&ring);
        break;
      }
      case WRITE_EVENT:
#if VERBOSE
        std::cout << "Write complete, keeping connection open" << std::endl;
#endif
        // free(req->buffer);
        buffer_pool.deallocate(
            (char*)req, sizeof(RequestData) + PAGE_SIZE * sizeof(int32_t));
        break;
      default:
        std::cout << "Unknown event type: " << req->event_type << std::endl;
        break;
    }

    io_uring_cqe_seen(&ring, cqe);
  }
}

void handle_client(const int client_socket, size_t client_num,
                   std::atomic<int>& finished_threads) {
  std::cout << "Handling a new client" << std::endl;
  struct io_uring ring {};
  int r = io_uring_queue_init(RING_SIZE, &ring, IORING_SETUP_SINGLE_ISSUER);
  if (r < 0) {
    std::cout << "io_uring_queue_init failed: " << strerror(-r) << std::endl;
    exit(EXIT_FAILURE);
  }
  if (!event_loop(ring, client_socket, client_num)) {
    exit(EXIT_FAILURE);
  }
  io_uring_queue_exit(&ring);
  finished_threads++;
}

int main() {
  int server_fd;
  sockaddr_in address{};
  int addr_len = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    std::cout << "socket failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) <
      0) {
    std::cout << "bind failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 1024) < 0) {
    std::cout << "listen" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Server started. Listening on port " << PORT << std::endl;

  size_t client_num = 0;
  std::atomic<int> finished_threads = 0;
  while (true) {
    if (client_num >= CLIENT_THREADS) {
      std::cout << "Max number of clients reached: " << client_num << std::endl;
      break;
    }

    int new_socket;
    if ((new_socket = accept(server_fd, reinterpret_cast<sockaddr*>(&address),
                             reinterpret_cast<socklen_t*>(&addr_len))) < 0) {
      std::cout << "accept" << std::endl;
      exit(EXIT_FAILURE);
    }

    std::thread client_thread(handle_client, new_socket, client_num++,
                              std::ref(finished_threads));
    client_thread.detach();
  }

  std::cout << "Waiting for clients to finish" << std::endl;
  while (finished_threads < client_num) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    printf("Finished threads: %d\n", finished_threads.load());
  }
  std::cout << "Server shutting down" << std::endl;
}
