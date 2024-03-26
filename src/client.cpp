#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>

#include "consts.h"
#include "memory_block.h"
#include "models/get_page.h"
#include "static_config.h"

MemoryBlock memory_block(PAGE_SIZE, PAGE_COUNT,
                         new PseudoRandomFillingStrategy());

int main() {
  Config::load_config();
  std::cout << "Port: " << Config::port << std::endl;
  std::cout << "Number of Requests: " << Config::num_requests << std::endl;

  sockaddr_in serv_addr{};
  int sock;
  GetPageResponse buffer = {};

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
    const uint32_t random_page = rand() % PAGE_COUNT;

    auto request = GetPageRequest{.page_number = random_page};

    auto start = std::chrono::high_resolution_clock::now();
    send(sock, &request, sizeof(request), 0);
    if (const long valread = read(sock, &buffer, sizeof(GetPageResponse));
        valread <= 0) {
      std::cerr << "Error reading from server" << std::endl;
      break;
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (memory_block.verify(buffer.content, random_page)) {
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
