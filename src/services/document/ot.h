// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_DOCUMENT_OT_H_
#define JDOCS_SERVICES_DOCUMENT_OT_H_

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace jdocs {

enum class OpType : uint32_t { RETAIN, INSERT, DELETE };
NLOHMANN_JSON_SERIALIZE_ENUM(OpType, {{OpType::RETAIN, "retain"},
                                      {OpType::INSERT, "insert"},
                                      {OpType::DELETE, "delete"}});

struct op_component {
  OpType type;
  uint32_t length;
  std::string text;
  // 富文本支持
  nlohmann::json attributes;

  static op_component Retain(uint32_t n, nlohmann::json attr = nullptr) {
    return {OpType::RETAIN, n, {}, std::move(attr)};
  }
  static op_component Insert(std::string text, nlohmann::json attr = nullptr) {
    return {OpType::INSERT, static_cast<uint32_t>(text.length()),
            std::move(text), std::move(attr)};
  }
  static op_component Delete(uint32_t n) {
    return {OpType::DELETE, n, {}, nullptr};
  }
};

void from_json(const nlohmann::json &j, op_component &op_item);
void to_json(nlohmann::json &j, const op_component &op_item);

struct Operation {
  std::vector<op_component> ops;

  void OpRetain(uint32_t n, nlohmann::json attr = nullptr);
  void OpInsert(std::string text, nlohmann::json attr = nullptr);
  void OpDelete(uint32_t n);

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Operation, ops);
};

class OpIterator {
public:
  OpIterator(const Operation &op) : op_(op) {}
  inline bool HasNext() const { return index_ < op_.ops.size(); }
  op_component peek() const {
    if (!HasNext())
      return op_component::Retain(UINT32_MAX);
    const auto &op_c = op_.ops[index_];
    if (op_c.type == OpType::INSERT) {
      return op_component::Insert(op_c.text.substr(offset_), op_c.attributes);
    } else if (op_c.type == OpType::DELETE) {
      return op_component::Delete(op_c.length - offset_);
    }
    return op_component::Retain(op_c.length - offset_, op_c.attributes);
  }
  void next(int n) {
    if (!HasNext())
      return;
    offset_ += n;
    if (offset_ >= op_.ops[index_].length) {
      ++index_;
      offset_ = 0;
    }
  }

private:
  const Operation &op_;
  size_t index_{0};
  uint32_t offset_{0};
};

Operation transform(const Operation &client, const Operation &server);
// 操作合并，压缩存储空间
Operation compose(const Operation &op_old, const Operation &op_new);
// 属性合并/覆盖
nlohmann::json compose_attributes(const nlohmann::json &attr_new,
                                  const nlohmann::json &attr_old);

} // namespace jdocs

#endif
