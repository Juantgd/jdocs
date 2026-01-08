// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_DATABASE_MYSQL_CONNECTION_H_
#define JDOCS_DATABASE_MYSQL_CONNECTION_H_

#include <mysql/mysql.h>
#include <nlohmann/json.hpp>

namespace jdocs {

using json = nlohmann::json;

namespace {
constexpr static const char *kHost = "127.0.0.1";
constexpr static uint16_t kPort = 3306;
constexpr static const char *kUser = "root";
constexpr static const char *kPassword = "123abcABC@";
constexpr static const char *kDatabase = "my_app_db";
// constexpr static uint32_t kMaxConnections = 1024;
// constexpr static uint32_t kMinConnections = 8;
} // namespace

class MysqlConnection {
public:
  ~MysqlConnection();

  static MysqlConnection *Instance() {
    static thread_local MysqlConnection conn(kHost, kPort, kUser, kPassword,
                                             kDatabase);
    return &conn;
  }

  MysqlConnection(const MysqlConnection &) = delete;
  MysqlConnection &operator=(const MysqlConnection &) = delete;
  MysqlConnection(MysqlConnection &&) = default;
  MysqlConnection &operator=(MysqlConnection &&) = default;

  json::array_t query(const char *sql);

  // 是否启用事务提交
  bool transaction(bool auto_mode);

  bool commit();

  bool rollback();

  uint64_t affected_rows() const { return mysql_affected_rows_; }

private:
  MysqlConnection(const char *host, uint16_t port, const char *username,
                  const char *password, const char *db_name);

  bool __connect(const char *host, uint16_t port, const char *username,
                 const char *password, const char *db_name);

  json::array_t __result_handle();
  inline void __free_mysql_result() {
    if (mysql_result_) {
      mysql_free_result(mysql_result_);
      mysql_result_ = NULL;
    }
  }
  MYSQL *mysql_conn_;
  MYSQL_RES *mysql_result_{NULL};
  MYSQL_ROW mysql_row_{NULL};
  uint64_t mysql_affected_rows_{0};
};

} // namespace jdocs

#endif
