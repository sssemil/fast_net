#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#define N 32

void handle_client(const int client_socket) {
  int32_t num;
  int32_t data[N];

  while (true) {
    // Attempt to read a single int32_t from the client
    ssize_t bytes_read = read(client_socket, &num, sizeof(num));
    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        std::cout << "Client disconnected." << std::endl;
      } else {
        std::cerr << "Error reading from client: " << strerror(errno) << std::endl;
      }
      break;
    }

    // Fill the data array with the read number
    std::fill_n(data, N, num);

    // Send the data array back to the client
    if (write(client_socket, data, sizeof(data)) < 0) {
      std::cerr << "Error sending data to client: " << strerror(errno) << std::endl;
      break;
    }
//    std::cout << "Data sent: filled with value " << num << std::endl;
  }

  close(client_socket);
}

int main() {
  int server_fd;
  struct sockaddr_in address {};
  int addr_len = sizeof(address);

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    std::cerr << "Socket creation failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(12345);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << "Bind failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 10) < 0) {
    std::cerr << "Listen failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Server started. Listening on port " << ntohs(address.sin_port) << std::endl;

  while (true) {
    int new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addr_len);
    if (new_socket < 0) {
      std::cerr << "Accept failed" << std::endl;
      continue;
    }
    std::cout << "Connection accepted" << std::endl;
    handle_client(new_socket);
  }
}
