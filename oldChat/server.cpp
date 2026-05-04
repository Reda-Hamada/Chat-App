#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

std::map<std::string, int> users;
std::mutex users_mutex;

// parse: message|to
bool parse_message(const std::string &input, std::string &msg,
                   std::string &to) {
  size_t pos = input.find('|');
  if (pos == std::string::npos)
    return false;

  msg = input.substr(0, pos);
  to = input.substr(pos + 1);
  return true;
}

void remove_user(const std::string &username) {
  std::lock_guard<std::mutex> lock(users_mutex);
  users.erase(username);
}

int get_user_socket(const std::string &username) {
  std::lock_guard<std::mutex> lock(users_mutex);
  if (users.find(username) == users.end())
    return -1;
  return users[username];
}

// runs in thread
static void handle_client(int clientSocket) {
  char buffer[1024];

  std::string prompt = "Enter username: ";
  send(clientSocket, prompt.c_str(), prompt.size(), 0);
  int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

  std::string username;
  username = std::string(buffer, bytesReceived);

  {
    std::lock_guard<std::mutex> lock(users_mutex);
    users[username] = clientSocket;
  }

  std::cout << username << " connected\n";

  while (true) {
    std::fill(std::begin(buffer), std::end(buffer), 0);

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived <= 0) {
      std::cout << username << " disconnected\n";
      break;
    }

    std::string input(buffer, bytesReceived);

    if (input == "exit")
      break;

    std::string msg, to;
    if (!parse_message(input, msg, to)) {
      std::string err = "Invalid format. Use: message|username";
      send(clientSocket, err.c_str(), err.size(), 0);
      continue;
    }

    int targetSocket = get_user_socket(to);
    if (targetSocket == -1) {
      std::string err = "User not found";
      send(clientSocket, err.c_str(), err.size(), 0);
      continue;
    }

    std::string finalMsg = username + ": " + msg;

    if (send(targetSocket, finalMsg.c_str(), finalMsg.size(), 0) < 0) {
      std::cerr << "Send failed\n";
      break;
    }

    std::string ok = "Sent ✔";
    send(clientSocket, ok.c_str(), ok.size(), 0);
  }

  remove_user(username);
  close(clientSocket);
}

// void _send(int clientSocket, std::string &username) {
//
//   std::string prompt = "Enter username: ";
//   send(clientSocket, prompt.c_str(), prompt.size(), 0);
//
//   char buffer[1024] = {};
//   int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
//
//   username = std::string(buffer, bytesReceived);
// }
//
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
    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);

    int clientSocket =
        accept(serverSocket, (struct sockaddr *)&clientAddress, &clientSize);

    if (clientSocket < 0) {
      std::cerr << "Accept failed\n";
      continue;
    }
    //
    // std::string prompt = "Enter username: ";
    // send(clientSocket, prompt.c_str(), prompt.size(), 0);
    //
    // char buffer[1024] = {};
    // int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    //
    //
    //
    // if (bytesReceived <= 0) {
    //   close(clientSocket);
    //   continue;
    // }

    // std::string username;
    // std::thread sender(_send, clientSocket, std::ref(username));
    // sender.detach();
    //
    std::thread(handle_client, clientSocket).detach();
  }

  close(serverSocket);
  return 0;
}
