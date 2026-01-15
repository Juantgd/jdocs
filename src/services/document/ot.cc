// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "ot.h"

#include <spdlog/spdlog.h>

namespace jdocs {

void Operation::OpRetain(uint32_t n, nlohmann::json attr) {
  if (n == 0)
    return;
  if (!ops.empty() && ops.back().type == OpType::RETAIN) {
    if (!attr.is_null())
      ops.back().attributes = compose_attributes(attr, ops.back().attributes);
    ops.back().length += n;
    return;
  }
  ops.emplace_back(op_component::Retain(n, std::move(attr)));
}

void Operation::OpInsert(std::string text, nlohmann::json attr) {
  if (text.empty())
    return;
  if (!ops.empty() && ops.back().type == OpType::INSERT) {
    if (!attr.is_null()) {
      ops.back().attributes = compose_attributes(attr, ops.back().attributes);
    }
    ops.back().text.append(std::move(text));
    return;
  }
  ops.emplace_back(op_component::Insert(std::move(text), std::move(attr)));
}

void Operation::OpDelete(uint32_t n) {
  if (n == 0)
    return;
  if (!ops.empty() && ops.back().type == OpType::DELETE) {
    ops.back().length += n;
    return;
  }
  ops.emplace_back(op_component::Delete(n));
}

Operation transform(const Operation &client, const Operation &server) {
  Operation result;
  OpIterator iterA(client);
  OpIterator iterB(server);
  while (iterA.HasNext() || iterB.HasNext()) {
    auto op_a = iterA.peek();
    auto op_b = iterB.peek();
    // 服务端插入操作优先
    if (op_b.type == OpType::INSERT) {
      result.OpRetain(op_b.length);
      iterB.next(op_b.length);
      continue;
    }
    if (op_a.type == OpType::INSERT) {
      result.OpInsert(std::move(op_a.text), std::move(op_a.attributes));
      iterA.next(op_a.length);
      continue;
    }

    // 这里只会出现RETAIN和DELETE操作
    uint32_t min_len = std::min(op_a.length, op_b.length);
    if (op_a.type == OpType::RETAIN) {
      if (op_b.type == OpType::RETAIN) {
        result.OpRetain(min_len, std::move(op_a.attributes));
      }
    } else if (op_a.type == OpType::DELETE) {
      if (op_b.type == OpType::RETAIN) {
        result.OpDelete(min_len);
      }
    }

    iterA.next(min_len);
    iterB.next(min_len);
  }
  return result;
}

Operation compose(const Operation &op_old, const Operation &op_new) {
  Operation result;
  OpIterator iterA(op_old);
  OpIterator iterB(op_new);
  spdlog::warn("compose function.");
  while (iterA.HasNext() || iterB.HasNext()) {
    auto comp_a = iterA.peek();
    auto comp_b = iterB.peek();
    if (comp_b.type == OpType::INSERT) {
      spdlog::warn("compose function comp_b insert.");
      result.OpInsert(std::move(comp_b.text), std::move(comp_b.attributes));
      spdlog::warn("compose function comp_b insert finish.");
      iterB.next(comp_b.length);
      continue;
    }
    if (comp_a.type == OpType::DELETE) {
      spdlog::warn("compose function comp_a delete.");
      result.OpDelete(comp_a.length);
      iterA.next(comp_a.length);
      continue;
    }
    uint32_t min_len = std::min(comp_a.length, comp_b.length);
    if (comp_a.type == OpType::INSERT) {
      if (comp_b.type == OpType::RETAIN) {
        spdlog::warn("compose function comp_a insert comp_b retain.");
        // 可能携带属性
        result.OpInsert(comp_a.text.substr(0, min_len),
                        compose_attributes(std::move(comp_b.attributes),
                                           std::move(comp_a.attributes)));
      } else if (comp_b.type == OpType::DELETE) {
        // 什么都不干，抵消了
      }
    } else if (comp_a.type == OpType::RETAIN) {
      if (comp_b.type == OpType::DELETE) {
        spdlog::warn("compose function comp_a retain comp_b delete.");
        result.OpDelete(min_len);
      } else if (comp_b.type == OpType::RETAIN) {
        spdlog::warn("compose function comp_a retain comp_b retain.");
        // 可能修改了原先的属性
        result.OpRetain(min_len,
                        compose_attributes(std::move(comp_b.attributes),
                                           std::move(comp_a.attributes)));
      }
    }
    iterA.next(min_len);
    iterB.next(min_len);
  }
  spdlog::warn("compose function finish.");
  return result;
}

nlohmann::json compose_attributes(const nlohmann::json &attr_new,
                                  const nlohmann::json &attr_old) {
  if (attr_new.is_null())
    return attr_old;
  if (attr_old.is_null())
    return attr_new;
  nlohmann::json result(attr_old);
  for (auto &[key, value] : attr_new.items()) {
    // 属性为null则取消该属性，否则覆盖/合并
    if (value.is_null()) {
      result.erase(key);
    } else {
      result[key] = value;
    }
  }
  return result;
}

void from_json(const nlohmann::json &j, op_component &op_item) {
  nlohmann::json attr = nullptr;
  if (j.contains("attributes")) {
    attr = j["attributes"];
  }
  if (j.contains("insert")) {
    op_item.type = OpType::INSERT;
    op_item.attributes = attr;
    if (j["insert"].is_string()) {
      op_item.text = j["insert"].get<std::string>();
      op_item.length = op_item.text.length();
    } else if (j["insert"].is_object()) {
      // TODO: 插入的元素是一个对象，暂不处理，使用unicode字符代替
      op_item.text = "\ufffc";
      op_item.length = 1;
    }
  } else if (j.contains("retain")) {
    op_item.type = OpType::RETAIN;
    op_item.length = j["retain"].get<uint32_t>();
    op_item.attributes = attr;
    op_item.text = {};
  } else if (j.contains("delete")) {
    op_item.type = OpType::DELETE;
    op_item.length = j["delete"].get<uint32_t>();
    op_item.attributes = nullptr;
    op_item.text = {};
  }
}

void to_json(nlohmann::json &j, const op_component &op_item) {
  if (op_item.attributes != nullptr && !op_item.attributes.is_null()) {
    j["attributes"] = op_item.attributes;
  }
  switch (op_item.type) {
  case OpType::RETAIN: {
    j["retain"] = op_item.length;
    break;
  }
  case OpType::INSERT: {
    j["insert"] = op_item.text;
    break;
  }
  case OpType::DELETE: {
    j["delete"] = op_item.length;
    break;
  }
  }
}

} // namespace jdocs
