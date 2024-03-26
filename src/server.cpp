#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

#include "memory_block.h"
#include "static_config.h"

#define BUFFER_SIZE 4096  // 4KB

MemoryBlock memory_block;

void handle_client(const int client_socket) {
  char buffer[1024] = {};
  while (true) {
    if (const long val_read = read(client_socket, buffer, 1024);
        val_read <= 0) {
      std::cout << "Client disconnected or error" << std::endl;
      break;
    }
    if (const int start_position = atoi(buffer);
        start_position < 0 ||
        start_position >= MemoryBlock::size - BUFFER_SIZE) {
      std::cout << "Invalid start position." << std::endl;
    } else {
      // Send 4KB of data starting from the requested position
      send(client_socket, memory_block.data + start_position, BUFFER_SIZE, 0);
      // std::cout << "Data sent." << std::endl;
    }
    memset(buffer, 0, 1024);
  }
  close(client_socket);
}

int main() {
  Config::load_config();
  std::cout << "Port: " << Config::port << std::endl;

  int server_fd;
  sockaddr_in address{};
  int addr_len = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(Config::port);

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
      0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 1024) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  std::cout << "Server started. Listening on port " << Config::port << std::endl;

  while (true) {
    int new_socket;
    if ((new_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&address),
                             reinterpret_cast<socklen_t *>(&addr_len))) < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }

    std::thread client_thread(handle_client, new_socket);
    client_thread.detach();
  }
}
