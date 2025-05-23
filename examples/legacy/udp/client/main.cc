#include <arpa/inet.h>
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

  const char* message = "waiting for sychronization with server";
  if (sendto(sockfd, message, std::strlen(message), MSG_CONFIRM, reinterpret_cast<const struct sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
    std::cerr << "Sendto failed: " << std::strerror(errno) << std::endl;
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  socklen_t len = sizeof(servaddr);
  std::array<std::byte, 1024> buffer;
  int n = recvfrom(sockfd, buffer.data(), buffer.size(), MSG_WAITALL, reinterpret_cast<struct sockaddr*>(&servaddr), &len);
  if (n < 0) {
    std::cerr << "Recvfrom failed: " << std::strerror(errno) << std::endl;
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  auto rtd = flatbuffers::GetRoot<robot::rtd::RTD>(buffer.data());

  std::cout << "Version: " << rtd->version() << std::endl;
  std::cout << "Timestamp: " << rtd->timestamp() << std::endl;

  auto motors = rtd->motors();
  for (auto motor : *motors) {
    std::cout << "Motor ID: " << motor->id()
              << ", Temperature: " << motor->temperature()
              << ", Angle: " << motor->angle()
              << ", Speed: " << motor->speed()
              << ", Torque: " << motor->torque()
              << ", Status: " << (motor->status() ? "On" : "Off") << std::endl;
  }

  close(sockfd);

  return EXIT_SUCCESS;
}