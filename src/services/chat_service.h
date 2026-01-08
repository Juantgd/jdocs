// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_CHAT_SERVICE_H_
#define JDOCS_SERVICES_CHAT_SERVICE_H_

#include "service_handler.h"

namespace jdocs {

class ChatService : public ServiceHandler {
public:
  ChatService(TcpConnection *connection);
  ~ChatService() = default;

  std::string handle(std::string data) override;

private:
};

} // namespace jdocs

#endif
