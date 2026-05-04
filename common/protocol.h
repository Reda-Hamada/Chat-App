#pragma once
#include <sstream>
#include <string>
#include <vector>

// connection settings
const int PORT = 8080;
const int BUF_SIZE = 4096;
const int HISTORY = 10;

// message types (client -> server)
const std::string JOIN = "JOIN";     // JOIN|roomname
const std::string MSG = "MSG";       // MSG|username|message
const std::string ROOM_MSG = "ROOM"; // ROOM|message
const std::string EXIT_CMD = "EXIT"; // EXIT

// message types (server -> client)
const std::string OK = "OK";     // OK|info
const std::string ERR = "ERR";   // ERR|reason
const std::string INFO = "INFO"; // INFO|message

// delimiter between parts
const char DELIM = '|';

// build("MSG", {"ahmed", "hello"}) -> "MSG|ahmed|hello"
inline std::string build(const std::string &type,
                         const std::vector<std::string> &parts = {}) {
  std::string result = type;
  for (const auto &p : parts)
    result += DELIM + p;
  return result;
}

// parse("MSG|ahmed|hello") -> ["MSG", "ahmed", "hello"]
inline std::vector<std::string> parse(const std::string &raw) {
  std::vector<std::string> parts;
  std::stringstream ss(raw);
  std::string token;
  while (std::getline(ss, token, DELIM))
    parts.push_back(token);
  return parts;
}
