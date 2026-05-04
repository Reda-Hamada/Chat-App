#include "../common/protocol.h"
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// ─── receive loop
// ───────────────────────────────────────────────────────────── runs in a
// background thread — prints any incoming message from the server

void receive_loop(int sock) {
  char buffer[BUF_SIZE] = {};

  while (true) {
    std::fill(std::begin(buffer), std::end(buffer), 0);
    int n = recv(sock, buffer, BUF_SIZE - 1, 0);

    if (n <= 0) {
      std::cout << "\n[disconnected from server]\n";
      exit(0);
    }

    std::string raw(buffer, n);
    auto parts = parse(raw);
    if (parts.empty())
      continue;

    std::string type = parts[0];

    if (type == INFO && parts.size() > 1)
      std::cout << "\n" << parts[1] << "\n> ";

    else if (type == OK && parts.size() > 1)
      std::cout << "\n[OK] " << parts[1] << "\n> ";

    else if (type == ERR && parts.size() > 1)
      std::cout << "\n[ERR] " << parts[1] << "\n> ";

    else
      std::cout << "\n" << raw << "\n> ";

    std::cout.flush();
  }
}

// ─── main
// ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {

  // server IP can be passed as argument: ./client 192.168.1.5
  // defaults to localhost if not provided
  std::string server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0) {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);

  if (inet_pton(AF_INET, server_ip.c_str(), &serverAddress.sin_addr) <= 0) {
    std::cerr << "Invalid server IP: " << server_ip << "\n";
    close(clientSocket);
    return 1;
  }

  if (connect(clientSocket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    std::cerr << "Connection failed — is the server running?\n";
    close(clientSocket);
    return 1;
  }

  std::cout << "Connected to server at " << server_ip << ":" << PORT << "\n";

  // ── handshake: username ──────────────────────────────────────────────────

  // receive "Enter username: " prompt
  char buffer[BUF_SIZE] = {};
  int n = recv(clientSocket, buffer, BUF_SIZE - 1, 0);
  if (n <= 0) {
    std::cout << "Server disconnected\n";
    return 1;
  }

  auto prompt_parts = parse(std::string(buffer, n));
  if (prompt_parts.size() > 1)
    std::cout << prompt_parts[1];

  std::string username;
  std::getline(std::cin, username);
  send(clientSocket, username.c_str(), username.size(), 0);

  // ── handshake: room ──────────────────────────────────────────────────────

  std::fill(std::begin(buffer), std::end(buffer), 0);
  n = recv(clientSocket, buffer, BUF_SIZE - 1, 0);
  if (n <= 0) {
    std::cout << "Server disconnected\n";
    return 1;
  }

  auto room_parts = parse(std::string(buffer, n));
  if (room_parts.size() > 1)
    std::cout << room_parts[1];

  std::string room;
  std::getline(std::cin, room);
  send(clientSocket, room.c_str(), room.size(), 0);

  // ── start background receive thread ──────────────────────────────────────
  std::thread(receive_loop, clientSocket).detach();

  // ── send loop ────────────────────────────────────────────────────────────
  std::cout << "\nCommands:\n";
  std::cout << "  MSG|username|message   — send to a specific user\n";
  std::cout << "  ROOM|message           — send to everyone in the room\n";
  std::cout << "  EXIT                   — disconnect\n\n";

  while (true) {
    std::cout << "> ";
    std::string input;

    if (!std::getline(std::cin, input))
      break;
    if (input.empty())
      continue;

    // validate format before sending
    auto parts = parse(input);
    if (parts.empty())
      continue;

    std::string type = parts[0];

    if (type == EXIT_CMD) {
      send(clientSocket, input.c_str(), input.size(), 0);
      break;
    }

    if (type == MSG && parts.size() < 3) {
      std::cout << "[ERR] Format: MSG|username|message\n";
      continue;
    }

    if (type == ROOM_MSG && parts.size() < 2) {
      std::cout << "[ERR] Format: ROOM|message\n";
      continue;
    }

    if (type != MSG && type != ROOM_MSG && type != EXIT_CMD) {
      std::cout << "[ERR] Unknown command — use MSG, ROOM, or EXIT\n";
      continue;
    }

    if (send(clientSocket, input.c_str(), input.size(), 0) < 0) {
      std::cerr << "Send failed\n";
      break;
    }
  }

  close(clientSocket);
  return 0;
}
