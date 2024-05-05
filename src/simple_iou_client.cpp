#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#define PAGE_SIZE 32
#define PORT 12345
#define SERVER_ADDR "127.0.0.1"
#define BATCH_SIZE (64*8)
#define N (64 * 1024)
#define NUM_REQUESTS (N - (N % BATCH_SIZE))
#define RING_SIZE 8192
#define VERIFY 1

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
  int r = io_uring_queue_init(RING_SIZE, &ring, 0);
  if (r < 0) {
    std::cout << "io_uring_queue_init failed: " << strerror(-r) << std::endl;
    exit(EXIT_FAILURE);
  }

  int correct_responses = 0;
  int incorrect_responses = 0;

  int iterations_received = 0;

  auto start = std::chrono::high_resolution_clock::now();

  for (int send_index = 0; send_index < NUM_REQUESTS;
       send_index += BATCH_SIZE) {
    for (int batch_index = 0; batch_index < BATCH_SIZE; ++batch_index) {
      auto* send_buffer = new int32_t(send_index + batch_index);
      struct io_uring_sqe* sqe_send = io_uring_get_sqe(&ring);
      io_uring_prep_send(sqe_send, sock, send_buffer, sizeof(int32_t), 0);

      auto* request_data_send = new RequestData{SEND_EVENT, send_buffer};
      io_uring_sqe_set_data(sqe_send, request_data_send);
    }

    for (int batch_index = 0; batch_index < BATCH_SIZE; ++batch_index) {
      auto* recv_buffer = new int32_t[PAGE_SIZE];
      struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
      io_uring_prep_recv(sqe_recv, sock, recv_buffer,
                         PAGE_SIZE * sizeof(int32_t), 0);

      auto* request_data_recv = new RequestData{RECV_EVENT, recv_buffer};
      io_uring_sqe_set_data(sqe_recv, request_data_recv);
    }

    io_uring_submit(&ring);

    for (int i = 0; i < BATCH_SIZE * 2; ++i) {
      struct io_uring_cqe* cqe;
      io_uring_wait_cqe(&ring, &cqe);
      auto* data = static_cast<RequestData*>(io_uring_cqe_get_data(cqe));

      if (cqe->res < 0) {
        if (data->event_type == SEND_EVENT) {
          std::cout << "Send failed: " << strerror(-cqe->res) << std::endl;
        } else {
          std::cout << "Receive failed: " << strerror(-cqe->res) << std::endl;
        }
      } else if (data->event_type == RECV_EVENT) {
        iterations_received++;
        if (iterations_received % 1000 == 0) {
          auto iter_per_second =
              iterations_received /
              std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - start)
                  .count();
          std::cout << "Iterations received: " << iterations_received << " ["
                    << iter_per_second << " it/s]" << std::endl;
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
        if (correct) {
          correct_responses++;
        } else {
          incorrect_responses++;
        }
#endif
      }

      delete[] data->buffer;
      delete data;

      io_uring_cqe_seen(&ring, cqe);
    }
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
  std::cout << "Correct responses: " << correct_responses << std::endl;
  std::cout << "Incorrect responses: " << incorrect_responses << std::endl;
}

int main() {
  int sock = setup_socket();
  if (sock >= 0) {
    send_receive_data(sock);
    close(sock);
  }
  return 0;
}
