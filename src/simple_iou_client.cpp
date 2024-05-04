#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#define N 32
#define PORT 12345
#define SERVER_ADDR "127.0.0.1"
#define NUM_REQUESTS 1000000
#define REPORT_INTERVAL 10000

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
  io_uring_queue_init(32, &ring, 0);

  int32_t send_value;
  std::vector<int32_t> recv_buffer(N, 0);
  int correct_responses = 0;
  int incorrect_responses = 0;

  auto start = std::chrono::high_resolution_clock::now();
  auto last_report_time = start;
  int iterations = 0;

  for (send_value = 1; send_value <= NUM_REQUESTS; send_value++) {
    struct io_uring_sqe* sqe_send = io_uring_get_sqe(&ring);
    io_uring_prep_send(sqe_send, sock, &send_value, sizeof(send_value), 0);

    struct io_uring_sqe* sqe_recv = io_uring_get_sqe(&ring);
    io_uring_prep_recv(sqe_recv, sock, recv_buffer.data(),
                       recv_buffer.size() * sizeof(int32_t), 0);

    io_uring_submit(&ring);

    struct io_uring_cqe* cqe;
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
      std::cout << "Send failed: " << strerror(-cqe->res) << std::endl;
    }
    io_uring_cqe_seen(&ring, cqe);

    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
      std::cout << "Receive failed: " << strerror(-cqe->res) << std::endl;
    } else {
      bool valid = true;
      for (int i = 0; i < N; ++i) {
        if (recv_buffer[i] != send_value) {
          valid = false;
          break;
        }
      }
      if (valid) {
        correct_responses++;
      } else {
        incorrect_responses++;
      }
    }
    io_uring_cqe_seen(&ring, cqe);

    // Increment iterations and report if interval is reached
    iterations++;
    if (iterations % REPORT_INTERVAL == 0) {
      auto now = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> elapsed = now - last_report_time;
      double it_per_second = REPORT_INTERVAL / elapsed.count();
      std::cout << iterations
                << " requests processed. Current speed: " << it_per_second
                << " it/s" << std::endl;
      last_report_time = now;
    }
  }

  std::chrono::duration<double> elapsed =
      std::chrono::high_resolution_clock::now() - start;
  double it_per_second = NUM_REQUESTS / elapsed.count();
  std::cout << "Total time: " << elapsed.count() << " s" << std::endl;
  std::cout << "Average speed: " << it_per_second * N * sizeof(int32_t) * 8 / 1e9
            << " Gbps" << std::endl;

  io_uring_queue_exit(&ring);
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
