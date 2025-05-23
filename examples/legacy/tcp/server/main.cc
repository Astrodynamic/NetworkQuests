#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <iostream>

#include "commands_generated.h"

constexpr uint16_t PORT = 8085;

int main() {
  int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_fd == 0) {
    std::cerr << "Socket creation failed: " << std::strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    std::cerr << "Setsockopt failed: " << std::strerror(errno) << std::endl;
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in address {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    .sin_addr = {
      .s_addr = INADDR_ANY
    }
  };

  if (bind(server_fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
    std::cerr << "Bind failed: " << std::strerror(errno) << std::endl;
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    std::cerr << "Listen failed: " << std::strerror(errno) << std::endl;
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  socklen_t addrlen = sizeof(address);
  int new_socket = accept(server_fd, reinterpret_cast<struct sockaddr *>(&address), &addrlen);
  if (new_socket < 0) {
    std::cerr << "Accept failed: " << std::strerror(errno) << std::endl;
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  std::array<std::byte, 1024> buffer{};
  int n = recv(new_socket, buffer.data(), buffer.size(), MSG_WAITALL);
  if (n < 0) {
    std::cerr << "Recv failed: " << std::strerror(errno) << std::endl;
    close(new_socket);
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  auto command = flatbuffers::GetRoot<robot::command::Command>(buffer.data());
  auto timestamp = command->timestamp();

  if (command->data_type() == robot::command::CommandData_MoveCommand) {
    std::cout << "Received Move Command: " << std::endl;

    auto move_command = command->data_as<robot::command::MoveCommand>();
    if (move_command->move_type() == robot::command::MoveData_C_Space) {
      auto c_space = move_command->move_as<robot::command::C_Space>();
      auto position = c_space->position();
      auto orientation = c_space->orientation();
      std::cout << "Position: " << position.x() << ", " << position.y() << ", " << position.z() << std::endl;
      std::cout << "Orientation: " << orientation.x() << ", " << orientation.y() << ", " << orientation.z() << std::endl;
    }
  } else if (command->data_type() == robot::command::CommandData_StopCommand) {
    std::cout << "Received Stop Command: " << std::endl;
    auto stop_command = command->data_as<robot::command::StopCommand>();
  }

  close(new_socket);
  close(server_fd);

  return EXIT_SUCCESS;
}