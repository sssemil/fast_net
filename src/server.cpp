#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <thread>

#include "consts.hpp"
#include "memory_block.hpp"
#include "models/get_page.hpp"
#include "spdlog/spdlog.h"
#include "static_config.hpp"

void handle_client(const int client_socket,
                   const MemoryBlock<PAGE_SIZE> &memory_block) {
  while (true) {
    GetPageRequest request{};
    if (const long val_read = read(client_socket, &request, sizeof(request));
        val_read != sizeof(request)) {
      spdlog::info("Client disconnected or error ({})", val_read);
      break;
    }
    request.to_host_order();

    spdlog::debug("Requested page number: {}", request.page_number);

    if (request.page_number >= Config::page_count) {
      spdlog::error("Invalid page number: {}", request.page_number);
      constexpr GetPageStatus status = INVALID_PAGE_NUMBER;
      uint32_t net_status = htonl(status);
      send(client_socket, &net_status, sizeof(net_status), 0);
      spdlog::info("Error sent.");
    } else {
      constexpr GetPageStatus status = SUCCESS;
      uint32_t net_status = htonl(status);
      iovec iov[2];
      iov[0].iov_base = &net_status;
      iov[0].iov_len = sizeof(net_status);
      iov[1].iov_base = const_cast<uint8_t *>(memory_block.data.data()) +
                        request.page_number * PAGE_SIZE;
      iov[1].iov_len = PAGE_SIZE;
      if (writev(client_socket, iov, 2) < 0) {
        spdlog::error("Error sending combined response to client");
      }
      spdlog::debug("Data sent.");
    }
  }
  close(client_socket);
}

int main() {
  Config::load_config();

  MemoryBlock<PAGE_SIZE> memory_block(Config::page_count,
                                      new PseudoRandomFillingStrategy());

  spdlog::info("Port: {}", Config::port);

  int server_fd;
  sockaddr_in address{};
  int addr_len = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    spdlog::critical("socket failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(Config::port);

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
      0) {
    spdlog::critical("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, MAX_QUEUE) < 0) {
    spdlog::critical("listen");
    exit(EXIT_FAILURE);
  }

  spdlog::info("Server started. Listening on port {}", Config::port);

  while (true) {
    int new_socket;
    if ((new_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&address),
                             reinterpret_cast<socklen_t *>(&addr_len))) < 0) {
      spdlog::critical("accept");
      exit(EXIT_FAILURE);
    }

    std::thread client_thread(handle_client, new_socket,
                              std::ref(memory_block));
    client_thread.detach();
  }
}
