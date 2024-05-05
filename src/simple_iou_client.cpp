#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "simple_consts.hpp"

enum EventType { SEND_EVENT, RECV_EVENT };

struct RequestData {
  EventType event_type;
  int32_t* buffer;
};

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

void send_receive_data(int sock) {
  struct io_uring ring {};
  int r = io_uring_queue_init(RING_SIZE, &ring, IORING_SETUP_SINGLE_ISSUER);
  if (r < 0) {
    std::cout << "io_uring_queue_init failed: " << strerror(-r) << std::endl;
    exit(EXIT_FAILURE);
  }

#if VERIFY
  int correct_responses = 0;
  int incorrect_responses = 0;
  bool received[NUM_REQUESTS] = {false};
#endif

  int iterations_received = 0;

  auto start = std::chrono::high_resolution_clock::now();

  int send_index = 0;
  int recv_index = 0;

  while (send_index < NUM_REQUESTS || recv_index < NUM_REQUESTS ||
         iterations_received < NUM_REQUESTS) {
    //    std::cout << "Send index: " << send_index << ", recv index: " <<
    //    recv_index
    //              << ", iterations received: " << iterations_received <<
    //              std::endl;
    // Submit send requests
    auto send_index_pre = send_index;
    while (send_index < NUM_REQUESTS &&
           send_index - recv_index < RING_SIZE / 4) {
      auto* send_buffer = new int32_t(send_index);
      struct io_uring_sqe* sqe_send = io_uring_get_sqe(&ring);
      io_uring_prep_send(sqe_send, sock, send_buffer, sizeof(int32_t), 0);

      auto* request_data_send = new RequestData{SEND_EVENT, send_buffer};
      io_uring_sqe_set_data(sqe_send, request_data_send);

      send_index++;
    }
    auto send_index_diff = send_index - send_index_pre;
    //    std::cout << "Send index: " << send_index << " (diff: " <<
    //    send_index_diff
    //              << ")" << std::endl;

    // Submit receive requests
    auto recv_index_pre = recv_index;
    while (recv_index < send_index &&
           recv_index - iterations_received < RING_SIZE / 4) {
      auto* recv_buffer = new int32_t[PAGE_SIZE];
      struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
      io_uring_prep_recv(sqe_recv, sock, recv_buffer,
                         PAGE_SIZE * sizeof(int32_t), MSG_WAITALL);

      auto* request_data_recv = new RequestData{RECV_EVENT, recv_buffer};
      io_uring_sqe_set_data(sqe_recv, request_data_recv);

      recv_index++;
    }
    auto recv_index_diff = recv_index - recv_index_pre;
    //    std::cout << "Recv index: " << recv_index << " (diff: " <<
    //    recv_index_diff
    //              << ")" << std::endl;

    if (send_index_diff != 0 || recv_index_diff != 0) {
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
          std::cout << "Send failed: " << strerror(-cqe->res) << std::endl;
        } else {
          std::cout << "Receive failed: " << strerror(-cqe->res) << std::endl;
        }
      } else if (data->event_type == RECV_EVENT) {
        iterations_received++;
        if (iterations_received % 10000 == 0) {
          auto iter_per_second =
              iterations_received /
              std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - start)
                  .count();
          std::cout << "Iterations received: " << iterations_received << " ["
                    << iter_per_second << " it/s]" << std::endl;
        }

        if (cqe->res != PAGE_SIZE * sizeof(int32_t)) {
          std::cout << "Received incorrect number of bytes: " << cqe->res
                    << " (first uint32 hex: " << std::hex << data->buffer[0]
                    << ")" << std::endl;
          //          exit(EXIT_FAILURE);
        } else {
          //          std::cout << "Received " << cqe->res << " bytes" <<
          //          std::endl;
        }

#if VERIFY
        // Validate response
        bool correct = true;
        int32_t first = data->buffer[0];
        for (int j = 0; j < PAGE_SIZE; ++j) {
          if (data->buffer[j] != first) {
            std::cout << "Incorrect response at index "
                      << iterations_received - 1 << ", value "
                      << data->buffer[j] << std::endl;
            correct = false;
            break;
          }
        }
        if (first >= NUM_REQUESTS) {
          std::cout << "Received out-of-range response at index " << first
                    << std::endl;
          correct = false;
        } else if (received[first]) {
          std::cout << "Received duplicate response at index " << first
                    << std::endl;
          correct = false;
        }
        if (correct) {
          correct_responses++;
          received[first] = true;
        } else {
          incorrect_responses++;
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
      std::chrono::high_resolution_clock::now() - start;
  double it_per_second = NUM_REQUESTS / elapsed.count();
  std::cout << "Total time: " << elapsed.count() << " s" << std::endl;
  std::cout << "Average speed: "
            << it_per_second * PAGE_SIZE * sizeof(int32_t) * 8 / 1e9 << " Gbps"
            << std::endl;

  io_uring_queue_exit(&ring);
  std::cout << "Iterations received: " << iterations_received << std::endl;
#if VERIFY
  for (int i = 0; i < NUM_REQUESTS; ++i) {
    if (!received[i]) {
      std::cout << "Missing response at index " << i << std::endl;
    }
  }
  std::cout << "Total iterations: " << NUM_REQUESTS << std::endl;
  std::cout << "Correct responses: " << correct_responses << std::endl;
  std::cout << "Incorrect responses: " << incorrect_responses << std::endl;
#endif
}

int main() {
  int sock = setup_socket();
  if (sock >= 0) {
    send_receive_data(sock);
    close(sock);
  }
  return 0;
}
