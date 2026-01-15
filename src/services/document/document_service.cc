// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "document_service.h"

#include <spdlog/spdlog.h>

#include "net/tcp_connection.h"

namespace jdocs {

void from_json(const nlohmann::json &j, docmsg_desc &msg) {
  DocOpType type = j.at("type").get<DocOpType>();
  switch (type) {
  case DocOpType::OPEN: {
    msg.type = DocOpType::OPEN;
    msg.doc_name = j.at("doc_name").get<std::string>();
    break;
  }
  case DocOpType::EDIT: {
    msg.type = DocOpType::EDIT;
    msg.ops = j.at("ops").get<Operation>();
    msg.version = j.at("v").get<uint64_t>();
    break;
  }
  case DocOpType::CLOSE: {
    msg.type = DocOpType::CLOSE;
    break;
  }
  default: {
    msg.type = DocOpType::UNKNOW;
  }
  }
}
void to_json(nlohmann::json &j, const docmsg_desc &msg) {
  switch (msg.type) {
  case DocOpType::OPEN: {
    j["type"] = "open";
    j["v"] = msg.version;
    j["ops"] = msg.ops;
    break;
  }
  case DocOpType::ACK: {
    j["type"] = "ack";
    j["user_id"] = msg.user_id;
    break;
  }
  case DocOpType::NOTIFY: {
    j["type"] = "notify";
    j["user_id"] = msg.user_id;
    j["message"] = msg.doc_name;
    break;
  }
  case DocOpType::OP: {
    j["type"] = "op";
    j["v"] = msg.version;
    j["user_id"] = msg.user_id;
    j["ops"] = msg.ops;
    break;
  }
  default: {
    j["success"] = false;
    j["message"] = "Server Internel Error.";
  }
  }
}

DocumentService::~DocumentService() {
  if (document_) {
    close_handle({});
  }
}

std::shared_mutex DocumentService::mutex_;
std::unordered_map<std::string, std::weak_ptr<Document>>
    DocumentService::documents_;

DocumentService::DocumentService(TcpConnection *connection)
    : ServiceHandler(connection) {}

std::shared_ptr<Document>
DocumentService::GetDocument(const std::string &doc_name) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = documents_.find(doc_name);
  if (it != documents_.end()) {
    return it->second.lock();
  }
  return nullptr;
}

std::shared_ptr<Document> DocumentService::OpenDocument(std::string doc_name) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = documents_.find(doc_name);
  if (it == documents_.end()) {
    std::shared_ptr<Document> doc = std::make_shared<Document>(doc_name);
    documents_.emplace(std::move(doc_name), doc);
    return doc;
  }
  return it->second.lock();
}

bool DocumentService::CloseDocument(const std::string &doc_name) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = documents_.find(doc_name);
  // 文档对象存在并且该文档对象的引用计数为1时，关闭文档操作直接将映射删除
  if (it != documents_.end() && it->second.use_count() == 1) {
    documents_.erase(it);
    return true;
  }
  return false;
}

bool DocumentService::parse_parameters(
    std::unordered_map<std::string, std::vector<std::string>> args) {
  args.clear();
  return true;
}

std::string DocumentService::handle(std::string data) {
  spdlog::warn("handle function");
  try {
    json_ = nlohmann::json::parse(std::move(data));
    docmsg_desc msg = json_.get<docmsg_desc>();
    std::string send_msg;
    switch (msg.type) {
    case DocOpType::OPEN: {
      send_msg = open_handle(std::move(msg));
      break;
    }
    case DocOpType::EDIT: {
      send_msg = edit_handle(std::move(msg));
      break;
    }
    case DocOpType::CLOSE: {
      send_msg = close_handle(std::move(msg));
      break;
    }
    default: {
      send_msg = R"({"success":false,"message":"bad request!"})";
    }
    }
    json_.clear();
    return send_msg;
  } catch (const nlohmann::json::exception &e) {
    spdlog::error("json parser failed. error: {}", e.what());
    return R"({"success":false,"message":"bad request!"})";
  }
}

std::string DocumentService::open_handle(docmsg_desc msg) {
  spdlog::warn("open handle function step 1");
  if (document_) {
    spdlog::warn("has document are open");
    document_->ExitUser(node_);
  }
  document_ = DocumentService::GetDocument(msg.doc_name);
  spdlog::warn("open handle function step 2");
  if (document_ == nullptr) {
    document_ = DocumentService::OpenDocument(std::move(msg.doc_name));
    spdlog::warn("open handle function step 3");
  }
  node_ = document_->JoinUser(connection_->conn_id());
  spdlog::warn("open handle function step 4");
  auto users = document_->GetUserList();
  if (users.size() > 1) {
    spdlog::warn("open handle function step 5");
    docmsg_desc notify_msg{.type = DocOpType::NOTIFY,
                           .user_id = connection_->user_id(),
                           .doc_name = "new user join."};
    json_ = notify_msg;
    CTContext *ctx =
        new CTContext(users.size() - 1, connection_->conn_id(), json_.dump());
    for (const auto conn_id : users) {
      if (conn_id != connection_->conn_id())
        connection_->GetEventLoop()->prep_cross_thread_msg(conn_id, ctx);
    }
  }
  msg.ops = document_->GetContent(msg.version);
  json_ = msg;
  return json_.dump();
}

std::string DocumentService::edit_handle(docmsg_desc msg) {
  spdlog::warn("edit handle function");
  if (!document_) {
    return R"({"success":false,"message":"no document are currently open."})";
  }
  msg.ops = document_->ApplyOp(msg.version, std::move(msg.ops));
  spdlog::warn("edit handle function step 1");
  msg.user_id = connection_->user_id();
  auto users = document_->GetUserList();
  spdlog::warn("edit handle function step 2");
  if (users.size() > 1) {
    msg.type = DocOpType::OP;
    json_ = std::move(msg);
    spdlog::warn("edit handle function step 3");
    CTContext *ctx =
        new CTContext(users.size() - 1, connection_->conn_id(), json_.dump());
    for (const auto conn_id : users) {
      if (conn_id != connection_->conn_id())
        connection_->GetEventLoop()->prep_cross_thread_msg(conn_id, ctx);
    }
  }
  spdlog::warn("edit handle function step 4");
  msg.type = DocOpType::ACK;
  msg.user_id = connection_->user_id();
  json_ = msg;
  spdlog::warn("edit handle function step 5");
  return json_.dump();
}

std::string DocumentService::close_handle(docmsg_desc msg) {
  spdlog::warn("close handle function");
  if (!document_) {
    return R"({"success":false,"message":"no document are currently open."})";
  }
  spdlog::warn("close handle function step 1");
  document_->ExitUser(node_);
  spdlog::warn("close handle function step 2");
  auto users = document_->GetUserList();
  document_.reset();
  spdlog::warn("close handle function step 3");
  if (users.size() > 0) {
    msg.type = DocOpType::NOTIFY;
    msg.user_id = connection_->user_id();
    msg.doc_name = "a user close the document.";
    json_ = std::move(msg);
    CTContext *ctx =
        new CTContext(users.size(), connection_->conn_id(), json_.dump());
    for (const auto conn_id : users) {
      if (conn_id != connection_->conn_id())
        connection_->GetEventLoop()->prep_cross_thread_msg(conn_id, ctx);
    }
  }
  return R"({"success":true,"message":"close successfully."})";
}

} // namespace jdocs
