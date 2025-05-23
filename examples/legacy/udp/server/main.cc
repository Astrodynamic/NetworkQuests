#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>

#include "rtd_generated.h"

constexpr uint16_t PORT = 8083;

int main() {
  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    std::cerr << "Socket creation failed: " << std::strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in servaddr {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    .sin_addr = {
      .s_addr = INADDR_ANY
    }
  };

  if (bind(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
    std::cerr << "Bind failed: " << std::strerror(errno) << std::endl;
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in cliaddr {};

  socklen_t len = sizeof(cliaddr);
  std::array<std::byte, 1024> buffer;
  int n = recvfrom(sockfd, buffer.data(), buffer.size(), MSG_WAITALL, reinterpret_cast<struct sockaddr *>(&cliaddr), &len);
  if (n < 0) {
    std::cerr << "Recvfrom failed: " << std::strerror(errno) << std::endl;
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  flatbuffers::FlatBufferBuilder builder(buffer.size());

  std::vector<robot::rtd::MotorData> motor_data;
  motor_data.push_back(robot::rtd::MotorData(1, 36.5f, 10.0f, 1500.0f, 50.0f, true));
  motor_data.push_back(robot::rtd::MotorData(2, 37.0f, 15.0f, 1600.0f, 55.0f, true));
  motor_data.push_back(robot::rtd::MotorData(3, 35.5f, 20.0f, 1400.0f, 45.0f, false));

  auto motors = builder.CreateVectorOfStructs(motor_data);

  auto version = 1;
  auto timestamp = 1623847200;

  robot::rtd::RTDBuilder rtd_builder(builder);
  rtd_builder.add_version(version);
  rtd_builder.add_timestamp(timestamp);
  rtd_builder.add_motors(motors);
  auto rtd = rtd_builder.Finish();

  builder.Finish(rtd);

  sendto(sockfd, builder.GetBufferPointer(), builder.GetSize(), MSG_CONFIRM, reinterpret_cast<const struct sockaddr *>(&cliaddr), sizeof(cliaddr));

  close(sockfd);

  return EXIT_SUCCESS;
}