#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>

#include "commands_generated.h"

constexpr std::string_view IP = "127.0.0.1";
constexpr uint16_t PORT = 8085;

int main() {
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    std::cerr << "Socket creation failed: " << std::strerror(errno) << std::endl;
    close(sock);
    return -1;
  }

  struct sockaddr_in serv_addr {
    .sin_family = AF_INET,
    .sin_port = htons(PORT)
  };

  if (inet_pton(AF_INET, IP.data(), &serv_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    close(sock);
    return -1;
  }

  if (connect(sock, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
    std::cerr << "Connection Failed" << std::endl;
    close(sock);
    return -1;
  }

  flatbuffers::FlatBufferBuilder builder(1024);

  robot::common::Vector3D position(1.0, 2.0, 3.0);
  robot::common::Vector3D orientation(0.0, 1.0, 0.0);
  auto c_space = robot::command::C_Space(position, orientation);

  auto c_space_offset = builder.CreateStruct(c_space);

  robot::command::MoveCommandBuilder move_command_builder(builder);
  move_command_builder.add_move_type(robot::command::MoveData_C_Space);
  move_command_builder.add_move(c_space_offset.Union());
  auto move_command = move_command_builder.Finish();

  robot::command::CommandBuilder command_builder(builder);
  command_builder.add_timestamp(0UL);
  command_builder.add_data_type(robot::command::CommandData_MoveCommand);
  command_builder.add_data(move_command.Union());
  auto command = command_builder.Finish();

  builder.Finish(command);

  send(sock, builder.GetBufferPointer(), builder.GetSize(), MSG_CONFIRM);

  std::cout << "Message sent" << std::endl;

  close(sock);

  return EXIT_SUCCESS;
}