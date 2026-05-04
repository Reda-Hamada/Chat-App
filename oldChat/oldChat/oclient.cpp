#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// no threading needed on the client — it's just one user talking to one server
int main() {
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0) {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080);

  if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
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

  std::cout << "Connected to server\n";

  char buffer[1024] = {};
  int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

  if (bytesReceived > 0) {
    std::cout << "Server-" << std::string(buffer, bytesReceived) << "\n";
  } else {
    std::cout << "Server disconnected\n";
    return -1;
  }

  std::string usrnameclient;
  std::cin >> usrnameclient;
  if (send(clientSocket, usrnameclient.c_str(), usrnameclient.size(), 0) < 0) {
    std::cerr << "Send failed\n";
  }
  while (true) {
    std::string message;
    std::cout << "meg|username: ";

    if (!std::getline(std::cin, message))
      break; // EOF / Ctrl+D
    if (message.empty())
      continue;
    if (message == "exit")
      break;

    if (send(clientSocket, message.c_str(), message.size(), 0) < 0) {
      std::cerr << "Client Send failed\n";
      break;
    }

    // wait for the server's reply
    char buffer[1024] = {};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
      std::cout << "Server: " << std::string(buffer, bytesReceived) << "\n";
    } else {
      std::cout << "Server disconnected\n";
      break;
    }
  }

  close(clientSocket);
  return 0;
}
