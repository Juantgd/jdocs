// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include <spdlog/spdlog.h>

#include "core/server.h"

int main() {
  jdocs::JdocsServer server(7788);
  server.Run();
  // jdocs::MysqlConnection *conn = jdocs::MysqlConnection::Instance();
  // jdocs::json j = conn->query("select * from tb_users");
  // if (j.empty()) {
  //   spdlog::error("查询失败");
  // } else {
  //   spdlog::info("result: \n{}", j.dump(4));
  // }
  return 0;
}
