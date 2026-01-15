// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_CHAT_SERVICE_H_
#define JDOCS_SERVICES_CHAT_SERVICE_H_

#include <nlohmann/json.hpp>

#include "service_handler.h"

namespace jdocs {

struct chat_message {
  uint32_t user_id;
  std::string message;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(chat_message, user_id, message);
};

struct sender_message {
  uint32_t user_id;
  std::string date;
  std::string message;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(sender_message, user_id, date, message);
};

class ChatService : public ServiceHandler {
public:
  ChatService(TcpConnection *connection);
  ~ChatService() = default;

  bool parse_parameters(
      std::unordered_map<std::string, std::vector<std::string>> args) override;

  std::string handle(std::string data) override;

private:
  nlohmann::json json_;
};

} // namespace jdocs

#endif
