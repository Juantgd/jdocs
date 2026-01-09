// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include <spdlog/spdlog.h>

#include "chat_service.h"
#include "core/server.h"
#include "net/tcp_connection.h"
#include "service_handler.h"
#include "utils/helpers.h"

namespace jdocs {

ChatService::ChatService(TcpConnection *connection)
    : ServiceHandler(connection) {}

bool ChatService::parse_parameters(
    std::unordered_map<std::string, std::vector<std::string>> args) {
  args.clear();
  return true;
}

std::string ChatService::handle(std::string data) {
  spdlog::info("Chat Service: user_id: {}", connection_->user_id());
  try {
    json_ = nlohmann::json::parse(data);
    chat_message msg = json_.get<chat_message>();
    spdlog::info("receive message from user_id: {}", connection_->user_id());
    sender_message send_msg = {connection_->user_id(), get_datetime(),
                               std::move(msg.message)};
    json_ = send_msg;
    uint32_t conn_id = JdocsServer::GetConnectionId(msg.user_id);
    // 发送消息给对应的连接，如果该连接存在
    if (conn_id) {
      spdlog::info("send message to conn_id: {}", conn_id);
      CTContext *ctx = new CTContext(1, connection_->conn_id(), json_.dump());
      connection_->GetEventLoop()->prep_cross_thread_msg(conn_id, ctx);
    }
    return R"({"success":true,"message":"sent successfully!"})";
  } catch (const nlohmann::json::exception &e) {
    spdlog::error("json parser failed. error: {}", e.what());
    return R"({"success":false,"message":"bad request!"})";
  }
}

} // namespace jdocs
