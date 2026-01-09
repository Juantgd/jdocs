// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "document_service.h"
#include "service_handler.h"

namespace jdocs {

DocumentService::DocumentService(TcpConnection *connection)
    : ServiceHandler(connection) {}

bool DocumentService::parse_parameters(
    std::unordered_map<std::string, std::vector<std::string>> args) {
  args.clear();
  return true;
}

std::string DocumentService::handle(std::string data) { return data; }

} // namespace jdocs
