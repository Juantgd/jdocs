// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "mysql_connection.h"

#include <spdlog/spdlog.h>

namespace jdocs {

MysqlConnection::MysqlConnection(const char *host, uint16_t port,
                                 const char *username, const char *password,
                                 const char *db_name) {
  mysql_conn_ = mysql_init(NULL);
  if (!mysql_conn_) {
    spdlog::error("mysql_init failed. error: {}", mysql_error(mysql_conn_));
    exit(EXIT_FAILURE);
  }
  mysql_set_character_set(mysql_conn_, "utf8");
  if (!__connect(host, port, username, password, db_name)) {
    exit(EXIT_FAILURE);
  }
}

MysqlConnection::~MysqlConnection() {
  __free_mysql_result();
  if (mysql_conn_) {
    mysql_close(mysql_conn_);
  }
}

bool MysqlConnection::__connect(const char *host, uint16_t port,
                                const char *username, const char *password,
                                const char *db_name) {
  if (mysql_real_connect(mysql_conn_, host, username, password, db_name, port,
                         NULL, 0)) {
    return true;
  }
  spdlog::error("mysql_real_connect failed. error:{}",
                mysql_error(mysql_conn_));
  return false;
}

json::array_t MysqlConnection::query(const char *sql) {
  if (mysql_query(mysql_conn_, sql)) {
    spdlog::error("mysql_query failed. error: {}", mysql_error(mysql_conn_));
    return {};
  }
  mysql_result_ = mysql_store_result(mysql_conn_);
  if (!mysql_result_) {
    if (mysql_field_count(mysql_conn_) == 0) {
      mysql_affected_rows_ = mysql_affected_rows(mysql_conn_);
    } else {
      spdlog::error("mysql_store_result failed. error: {}",
                    mysql_error(mysql_conn_));
    }
    return {};
  }
  return __result_handle();
}

json::array_t MysqlConnection::__result_handle() {
  // 获取字段列表
  MYSQL_FIELD *fields = mysql_fetch_fields(mysql_result_);
  unsigned int fields_count = mysql_num_fields(mysql_result_);

  json json_array = json::array();
  while ((mysql_row_ = mysql_fetch_row(mysql_result_))) {
    json json_object = json::object();
    unsigned long *lengths = mysql_fetch_lengths(mysql_result_);
    for (unsigned i = 0; i < fields_count; ++i) {
      json_object.push_back({std::string(fields[i].name, fields[i].name_length),
                             std::string(mysql_row_[i], lengths[i])});
    }
    json_array.emplace_back(std::move(json_object));
  }
  __free_mysql_result();

  return json_array;
}

// 是否启用事务提交
bool MysqlConnection::transaction(bool auto_mode) {
  if (!mysql_autocommit(mysql_conn_, auto_mode)) {
    spdlog::error("mysql_autocommit failed. error: {}",
                  mysql_error(mysql_conn_));
    return false;
  }
  return true;
}

bool MysqlConnection::commit() {
  if (!mysql_commit(mysql_conn_)) {
    spdlog::error("mysql_commit failed. error: {}", mysql_error(mysql_conn_));
    return false;
  }
  return true;
}

bool MysqlConnection::rollback() {
  if (!mysql_rollback(mysql_conn_)) {
    spdlog::error("mysql_rollback failed. error: {}", mysql_error(mysql_conn_));
    return false;
  }
  return true;
}

} // namespace jdocs
