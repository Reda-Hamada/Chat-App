#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

std::map<std::string, int> users;
// runs in its own thread — one per connected client
static void handle_client(int clientSocket, std::string username) {
  char buffer[1024];

  while (true) {
    std::fill(std::begin(buffer), std::end(buffer), 0);

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    std::cout << buffer << std::endl;
    if (bytesReceived <= 0) {
      std::cout << username << " " << "disconnected\n";
      break;
    }

    std::string message(buffer, bytesReceived);
    if (message == "exit")
      break;

    std::string meg, to;

    int idxsplit = 0;
    for (int i = 0; i < message.size(); i++) {
      if (message[i] == '|') {
        idxsplit = i;
        break;
      }
      meg += message[i];
    }
    // std::cout << "Received: " << message << "\n";
    //
    for (int i = idxsplit + 1; i < message.size(); i++)
      to += message[i];
    int fd = users[to];

    std::string megsend = username + meg;
    std::cout << meg << "-->" << to << std::endl;
    if (send(fd, megsend.c_str(), megsend.size(), 0) < 0) {
      std::cerr << "Server Send failed\n";
      break;
    }
    std::cout << "the message received to " << to << "\n";
  }
  close(clientSocket);
}

int main() {

  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  int opt = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    std::cerr << "Bind failed\n";
    close(serverSocket);
    return 1;
  }

  if (listen(serverSocket, 5) < 0) {
    std::cerr << "Listen failed\n";
    close(serverSocket);
    return 1;
  }

  std::cout << "Listening on port 8080...\n";

  while (true) {
    char buffer[1024];
    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);

    int clientSocket =
        accept(serverSocket, (struct sockaddr *)&clientAddress, &clientSize);
    if (clientSocket < 0) {
      std::cerr << "Accept failed, continuing...\n";
      continue;
    }

    std::string prompt = "Enter user name: ";
    if (send(clientSocket, prompt.c_str(), prompt.size(), 0) < 0) {
      std::cerr << "Send failed\n";
      break;
    }
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
      std::cout << "Client disconnected\n";
      break;
    }
    std::string usr(buffer, bytesReceived);
    users[usr] = clientSocket;
    std::cout << usr << " " << "connected" << inet_ntoa(clientAddress.sin_addr)
              << "\n";
    // handle_client(clientSocket);
    // detach — thread cleans itself up when done, main keeps accepting
    std::thread(handle_client, clientSocket, usr).detach();
  }
  close(serverSocket);
  return 0;
}
