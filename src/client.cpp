#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>

#include "memory_block.h"
#include "static_config.h"

#define BUFFER_SIZE 4096

int main() {
  Config::load_config();
  std::cout << "Port: " << Config::port << std::endl;
  std::cout << "Number of Requests: " << Config::num_requests << std::endl;

  sockaddr_in serv_addr{};
  int sock;
  char buffer[BUFFER_SIZE] = {};

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Socket creation error" << std::endl;
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(Config::port);

  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address / Address not supported" << std::endl;
    return -1;
  }

  if (connect(sock, reinterpret_cast<sockaddr *>(&serv_addr),
              sizeof(serv_addr)) < 0) {
    std::cerr << "Connection failed" << std::endl;
    return -1;
  }

  srand(time(nullptr));  // Seed the random number generator
  std::chrono::duration<double, std::milli> total_time(0);
  int verified_count = 0;

  for (int i = 0; i < Config::num_requests; ++i) {
    const int start_pos = rand() % (MemoryBlock::size - BUFFER_SIZE);
    // Random start position within the server's memory block
    std::string request = std::to_string(start_pos);

    auto start = std::chrono::high_resolution_clock::now();
    send(sock, request.c_str(), request.length(), 0);
    if (const long valread = read(sock, buffer, BUFFER_SIZE); valread <= 0) {
      std::cerr << "Error reading from server" << std::endl;
      break;
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (MemoryBlock::verify(buffer, start_pos, BUFFER_SIZE)) {
      verified_count++;
    } else {
      std::cerr << "Data verification failed." << std::endl;
    }

    total_time += end - start;
    // std::cout << "Received chunk starting with: " << std::string(buffer,
    // buffer + 10) << std::endl;
  }

  const double avg_time =
      total_time.count() / static_cast<double>(Config::num_requests);
  std::cout << "Average response time for " << Config::num_requests
            << " requests: " << avg_time << " ms" << std::endl;
  std::cout << "Verified " << verified_count << " out of "
            << Config::num_requests << " requests." << std::endl;

  close(sock);
  return 0;
}
