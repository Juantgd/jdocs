// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_DOCUMENT_SERVICE_H_
#define JDOCS_SERVICES_DOCUMENT_SERVICE_H_

#include <memory>
#include <shared_mutex>

#include "document.h"
#include "service_handler.h"

namespace jdocs {

enum class DocOpType { UNKNOW, OPEN, EDIT, CLOSE, ACK, NOTIFY, OP };
NLOHMANN_JSON_SERIALIZE_ENUM(DocOpType, {{DocOpType::OPEN, "open"},
                                         {DocOpType::EDIT, "edit"},
                                         {DocOpType::CLOSE, "close"}});

struct docmsg_desc {
  DocOpType type;
  uint64_t version{0};
  uint32_t user_id{0};
  std::string doc_name{};
  Operation ops{};
};

void from_json(const nlohmann::json &j, docmsg_desc &msg);
void to_json(nlohmann::json &j, const docmsg_desc &msg);

class DocumentService : public ServiceHandler {
public:
  DocumentService(TcpConnection *connection);
  ~DocumentService();

  bool parse_parameters(
      std::unordered_map<std::string, std::vector<std::string>> args) override;

  static std::shared_ptr<Document> GetDocument(const std::string &doc_name);

  static std::shared_ptr<Document> OpenDocument(std::string doc_name);

  static bool CloseDocument(const std::string &doc_name);

  std::string handle(std::string data) override;

  std::string open_handle(docmsg_desc msg);

  std::string edit_handle(docmsg_desc msg);

  std::string close_handle(docmsg_desc msg);

private:
  static std::shared_mutex mutex_;
  static std::unordered_map<std::string, std::weak_ptr<Document>> documents_;

  std::shared_ptr<Document> document_{nullptr};
  std::list<uint32_t>::iterator node_;
  nlohmann::json json_;
};

} // namespace jdocs

#endif
