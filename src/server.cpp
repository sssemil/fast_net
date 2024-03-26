#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

#include "consts.h"
#include "memory_block.h"
#include "models/get_page.h"
#include "static_config.h"

MemoryBlock memory_block(PAGE_SIZE, PAGE_COUNT,
                         new PseudoRandomFillingStrategy());

void handle_client(const int client_socket) {
  while (true) {
    GetPageRequest request{};
    if (const long val_read = read(client_socket, &request, sizeof(request));
        val_read <= 0) {
      std::cout << "Client disconnected or error" << std::endl;
      break;
    }

    if (request.page_number >= PAGE_COUNT) {
      std::cout << "Invalid page number: " << request.page_number << std::endl;
      // TODO: Define a better response struct for errors
      GetPageResponse response{};
      response.status = INVALID_PAGE_NUMBER;
      send(client_socket, &response, sizeof(response), 0);
      std::cout << "Error sent." << std::endl;
    } else {
      GetPageResponse response{};
      response.status = SUCCESS;
      // TODO: Optimize this copy operation, we can just read and send the data
      // from the page memblock directly
      std::memcpy((void *)response.content.data(),
                  memory_block.data.data() + request.page_number * PAGE_SIZE,
                  PAGE_SIZE);
      send(client_socket, &response, sizeof(response), 0);
      // std::cout << "Data sent." << std::endl;
    }
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

  if (listen(server_fd, MAX_QUEUE) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  std::cout << "Server started. Listening on port " << Config::port
            << std::endl;

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
