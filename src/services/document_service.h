// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_DOCUMENT_SERVICE_H_
#define JDOCS_SERVICES_DOCUMENT_SERVICE_H_

#include "service_handler.h"

namespace jdocs {

class DocumentService : public ServiceHandler {
public:
  DocumentService(TcpConnection *connection);
  ~DocumentService() = default;

  bool parse_parameters(
      std::unordered_map<std::string, std::vector<std::string>> args) override;

  std::string handle(std::string data) override;

private:
};

} // namespace jdocs

#endif
