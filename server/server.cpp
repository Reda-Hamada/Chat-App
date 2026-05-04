#include "../common/protocol.h"
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

// ─── data structures ────────────────────────────────────────────────────────

struct Client {
  int fd;
  std::string username;
  std::string room;
};

// username -> Client info
std::map<std::string, Client> clients;
std::mutex clients_mutex;

// room -> set of usernames
std::map<std::string, std::set<std::string>> rooms;
std::mutex rooms_mutex;

// ─── helpers ────────────────────────────────────────────────────────────────

void send_msg(int fd, const std::string &msg) {
  send(fd, msg.c_str(), msg.size(), 0);
}

std::string recv_msg(int fd) {
  char buffer[BUF_SIZE] = {};
  int n = recv(fd, buffer, BUF_SIZE - 1, 0);
  if (n <= 0)
    return "";
  return std::string(buffer, n);
}

// ─── history ────────────────────────────────────────────────────────────────

// append one line to history/roomname.txt
void save_history(const std::string &room, const std::string &line) {
  std::ofstream f("history/" + room + ".txt", std::ios::app);
  if (f.is_open())
    f << line << "\n";
}

// read last HISTORY lines from history/roomname.txt
std::vector<std::string> load_history(const std::string &room) {
  std::ifstream f("history/" + room + ".txt");
  std::vector<std::string> lines;
  if (!f.is_open())
    return lines;

  std::string line;
  while (std::getline(f, line))
    lines.push_back(line);

  // return only the last HISTORY lines
  if ((int)lines.size() > HISTORY)
    lines = std::vector<std::string>(lines.end() - HISTORY, lines.end());

  return lines;
}

// ─── offline messages ────────────────────────────────────────────────────────

// save a message for an offline user
void save_offline(const std::string &username, const std::string &msg) {
  std::ofstream f("offline/" + username + ".txt", std::ios::app);
  if (f.is_open())
    f << msg << "\n";
}

// send all offline messages to a user then delete the file
void flush_offline(const std::string &username, int fd) {
  std::ifstream f("offline/" + username + ".txt");
  if (!f.is_open())
    return;

  std::string line;
  bool had_messages = false;

  while (std::getline(f, line)) {
    had_messages = true;
    send_msg(fd, build(INFO, {"[offline] " + line}));
  }
  f.close();

  if (had_messages)
    std::remove(("offline/" + username + ".txt").c_str());
}

// ─── room broadcast ──────────────────────────────────────────────────────────

// send a message to everyone in a room except the sender
void broadcast_room(const std::string &room, const std::string &sender,
                    const std::string &msg) {
  std::lock_guard<std::mutex> lock(rooms_mutex);
  for (const auto &uname : rooms[room]) {
    if (uname == sender)
      continue;

    std::lock_guard<std::mutex> clock(clients_mutex);
    if (clients.count(uname))
      send_msg(clients[uname].fd, msg);
  }
}

// ─── handle one client ───────────────────────────────────────────────────────

static void handle_client(int fd) {

  // ── step 1: get username ─────────────────────────────────────────────────
  send_msg(fd, build(INFO, {"Enter username: "}));
  std::string username = recv_msg(fd);
  if (username.empty()) {
    close(fd);
    return;
  }

  // reject duplicate username
  {
    std::lock_guard<std::mutex> lock(clients_mutex);
    if (clients.count(username)) {
      send_msg(fd, build(ERR, {"Username already taken"}));
      close(fd);
      return;
    }
  }

  // ── step 2: get room ─────────────────────────────────────────────────────
  send_msg(fd, build(INFO, {"Enter room name: "}));
  std::string room = recv_msg(fd);
  if (room.empty()) {
    close(fd);
    return;
  }

  // ── step 3: register client ──────────────────────────────────────────────
  {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[username] = {fd, username, room};
  }
  {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    rooms[room].insert(username);
  }

  std::cout << "[+] " << username << " joined room [" << room << "]\n";

  // ── step 4: send history ─────────────────────────────────────────────────
  auto history = load_history(room);
  if (!history.empty()) {
    send_msg(fd, build(INFO, {"--- last " + std::to_string(history.size()) +
                              " messages ---"}));
    for (const auto &line : history)
      send_msg(fd, build(INFO, {line}));
    send_msg(fd, build(INFO, {"--- end of history ---"}));
  }

  // ── step 5: send offline messages ────────────────────────────────────────
  flush_offline(username, fd);

  // notify room
  broadcast_room(room, username, build(INFO, {username + " joined the room"}));
  send_msg(fd, build(OK, {"Welcome to room [" + room + "]"}));

  // ── step 6: main message loop ────────────────────────────────────────────
  while (true) {
    std::string raw = recv_msg(fd);
    if (raw.empty())
      break;

    auto parts = parse(raw);
    if (parts.empty())
      continue;

    std::string type = parts[0];

    // EXIT
    if (type == EXIT_CMD)
      break;

    // MSG|targetuser|message
    if (type == MSG) {
      if (parts.size() < 3) {
        send_msg(fd, build(ERR, {"Format: MSG|username|message"}));
        continue;
      }

      std::string to = parts[1];
      std::string msg = parts[2];

      // check target exists
      std::lock_guard<std::mutex> lock(clients_mutex);
      if (!clients.count(to) && to != username) {
        // target is offline — save for later
        std::string offline_msg = "[" + username + "]: " + msg;
        save_offline(to, offline_msg);
        send_msg(fd, build(INFO, {to + " is offline — message saved"}));
        continue;
      }

      // check same room
      if (clients.count(to) && clients[to].room != room) {
        send_msg(fd, build(ERR, {to + " is in a different room"}));
        continue;
      }

      // deliver
      std::string forward = "[" + username + "] -> [you]: " + msg;
      send_msg(clients[to].fd, build(INFO, {forward}));
      std::cout << "[>] " << username << " -> " << to << ": " << msg << "\n";
    }

    // ROOM|message  (broadcast to everyone in the room)
    else if (type == ROOM_MSG) {
      if (parts.size() < 2) {
        send_msg(fd, build(ERR, {"Format: ROOM|message"}));
        continue;
      }

      std::string msg = parts[1];
      std::string forward = "[" + username + "] [room]: " + msg;

      broadcast_room(room, username, build(INFO, {forward}));
      save_history(room, forward);
      std::cout << "[room:" << room << "] " << username << ": " << msg << "\n";
    }

    else {
      send_msg(fd, build(ERR, {"Unknown command"}));
    }
  }

  // ── cleanup ──────────────────────────────────────────────────────────────
  {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(username);
  }
  {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    rooms[room].erase(username);
  }

  broadcast_room(room, username, build(INFO, {username + " left the room"}));
  std::cout << "[-] " << username << " disconnected\n";
  close(fd);
}

// ─── main ────────────────────────────────────────────────────────────────────

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
  address.sin_port = htons(PORT);

  if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    std::cerr << "Bind failed\n";
    close(serverSocket);
    return 1;
  }

  if (listen(serverSocket, 10) < 0) {
    std::cerr << "Listen failed\n";
    close(serverSocket);
    return 1;
  }

  std::cout << "Server listening on port " << PORT << "...\n";

  while (true) {
    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);

    int clientFd =
        accept(serverSocket, (struct sockaddr *)&clientAddress, &clientSize);
    if (clientFd < 0) {
      std::cerr << "Accept failed, continuing...\n";
      continue;
    }

    std::cout << "[*] New connection from " << inet_ntoa(clientAddress.sin_addr)
              << ":" << ntohs(clientAddress.sin_port) << "\n";

    // each client gets its own thread
    std::thread(handle_client, clientFd).detach();
  }

  close(serverSocket);
  return 0;
}
