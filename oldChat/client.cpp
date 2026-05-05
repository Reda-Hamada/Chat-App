#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// thread function for receiving messages
void receive_messages(int clientSocket) {
  char buffer[1024] = {0};

  while (true) {
    // std::fill(std::begin(buffer), std::end(buffer), 0);

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
      std::cout << "\n" << buffer << "\n> " << std::flush;
    } else {
      std::cout << "\nDisconnected from server\n";
      exit(0);
    }
  }
}

int main(int argc, char *argv[]) {

  std::string server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0) {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080);

  if (inet_pton(AF_INET, server_ip.c_str(), &serverAddress.sin_addr) <= 0) {
    std::cerr << "Invalid address\n";
    close(clientSocket);
    return 1;
  }

  if (connect(clientSocket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    std::cerr << "Connection failed\n";
    close(clientSocket);
    return 1;
  }

  std::cout << "Connected to server at " << server_ip << ":" << 8080 << "\n";

  // receive prompt
  char buffer[1024] = {};
  int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

  if (bytesReceived <= 0) {
    std::cout << "Server disconnected\n";
    return -1;
  }

  std::cout << buffer;

  std::string username;
  std::getline(std::cin, username);

  send(clientSocket, username.c_str(), username.size(), 0);

  std::thread receiver(receive_messages, clientSocket);
  receiver.detach();

  // main thread = sending
  while (true) {
    std::string message;
    std::cout << "> ";

    std::getline(std::cin, message);

    if (message.empty())
      continue;

    if (message == "exit")
      break;

    if (send(clientSocket, message.c_str(), message.size(), 0) < 0) {
      std::cerr << "Send failed\n";
      break;
    }
  }

  close(clientSocket);
  return 0;
}
