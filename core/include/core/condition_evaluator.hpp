#pragma once

#include <cstddef>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <typeindex>
#include <vector>

#include "core/blackboard.hpp"
#include "utils/logger.hpp"

// Lightweight expression evaluator for Condition nodes.
// Supports:
//   $NodeId.PinName > 5       (Blackboard variable comparison)
//   $NodeId.PinName == "ok"   (String comparison)
//   expr1 && expr2            (Logical AND)
//   expr1 || expr2            (Logical OR)
//
// Supported operators: >, <, >=, <=, ==, !=

#include "core/module_metadata.hpp"

namespace ConditionEvaluator {

enum class EvalResult {
  TRUE = 1,
  FALSE = 0,
  ERROR = -1,
};

const std::unordered_map<EvalResult, std::string> eval_result_to_str{
    {EvalResult::TRUE, "TRUE"},
    {EvalResult::FALSE, "FALSE"},
    {EvalResult::ERROR, "ERROR"}};

// Base interface for all AST Nodes
struct ASTNode {
  virtual ~ASTNode() = default;
  // Evaluate the node. Returns std::any containing the result.
  virtual std::any evaluate(Blackboard& db) const = 0;
};

// Wrapper struct to distinguish a parsed literal from a Blackboard std::string
struct LiteralToken {
  std::string str;
};

// Represents a literal value (e.g., 7, "hello", true)
struct LiteralNode : public ASTNode {
  std::string token_str;

  LiteralNode(const std::string& token) : token_str(token) {}

  std::any evaluate(Blackboard&) const override {
    return LiteralToken{token_str};
  }
};

// Represents a variable fetch from the Blackboard: $Node.Pin[.Property]
struct VariableNode : public ASTNode {
  std::string full_path;  // e.g. "ImageSrc.Image_Out.empty()"

  VariableNode(const std::string& path) : full_path(path) {}

  std::any evaluate(Blackboard& db) const override {
    std::string db_key = full_path;
    std::string property_suffix_method;

    // Try finding exact match first (e.g. key is "A.B.C")
    if (!db.has(db_key)) {
      // It might be "Node.Pin" + ".Property"
      size_t last_dot = db_key.find_last_of('.');
      if (last_dot != std::string::npos) {
        std::string base_key = db_key.substr(0, last_dot);
        if (db.has(base_key)) {
          property_suffix_method = db_key.substr(last_dot + 1);
          db_key = base_key;
        }
      }
    }

    if (!db.has(db_key)) {
      LOG_WARN("[ConditionEvaluator] Blackboard key not found: " + full_path);
      return {};
    }

    std::any val = db.read_any(db_key);
    if (!val.has_value()) return {};

    // If there's a property suffix to extract, lookup the property extractor
    if (!property_suffix_method.empty()) {
      auto& extractors = TypeRegistry::get_property_extractors();
      auto it = extractors.find(std::type_index(val.type()));
      if (it != extractors.end()) {
        std::any extracted_val = it->second(val, property_suffix_method);
        if (!extracted_val.has_value()) {
          LOG_WARN("[ConditionEvaluator] Extractor failed for property: " +
                   property_suffix_method + " on type " + val.type().name());
        }
        return extracted_val;
      } else {
        LOG_WARN(std::string("[ConditionEvaluator] No property extractor "
                             "registered for type: ") +
                 val.type().name());
        return {};
      }
    }

    return val;
  }
};

// Helper to perform generic comparison between two identical types
template <typename T>
EvalResult compare_values(const T& l, const std::string& op, const T& r) {
  if (op == "==") return (l == r) ? EvalResult::TRUE : EvalResult::FALSE;
  if (op == "!=") return (l != r) ? EvalResult::TRUE : EvalResult::FALSE;
  if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
    if (op == ">") return (l > r) ? EvalResult::TRUE : EvalResult::FALSE;
    if (op == "<") return (l < r) ? EvalResult::TRUE : EvalResult::FALSE;
    if (op == ">=") return (l >= r) ? EvalResult::TRUE : EvalResult::FALSE;
    if (op == "<=") return (l <= r) ? EvalResult::TRUE : EvalResult::FALSE;
  }
  return EvalResult::ERROR;
}

template <typename T>
EvalResult coerce_and_compare(const std::any& lhs, const std::string& op,
                              const std::string& rhs) {
  try {
    if constexpr (std::is_same_v<T, int>)
      return compare_values(std::any_cast<int>(lhs), op, std::stoi(rhs));
    else if constexpr (std::is_same_v<T, double>)
      return compare_values(std::any_cast<double>(lhs), op, std::stod(rhs));
    else if constexpr (std::is_same_v<T, float>)
      return compare_values(std::any_cast<float>(lhs), op, std::stof(rhs));
  } catch (...) {
  }
  return EvalResult::ERROR;
}

template <>
EvalResult coerce_and_compare<bool>(const std::any& lhs, const std::string& op,
                                    const std::string& rhs) {
  bool l = std::any_cast<bool>(lhs);
  bool r;
  if (rhs == "true" || rhs == "1") {
    r = true;
  } else if (rhs == "false" || rhs == "0") {
    r = false;
  } else {
    // If it's a quoted string being compared to a bool, reject it
    if (rhs.size() >= 2 && rhs.front() == '"' && rhs.back() == '"') {
      LOG_ERROR(
          "[ConditionEvaluator] Cannot compare boolean with string literal: " +
          rhs);
      return EvalResult::ERROR;
    }
    LOG_ERROR("[ConditionEvaluator] Invalid boolean literal: " + rhs);
    return EvalResult::ERROR;
  }

  return compare_values(l, op, r);
}

template <>
EvalResult coerce_and_compare<std::string>(const std::any& lhs,
                                           const std::string& op,
                                           const std::string& rhs) {
  std::string l = std::any_cast<std::string>(lhs);
  std::string r = rhs;

  if (r.size() >= 2 && r.front() == '"' && r.back() == '"') {
    r = r.substr(1, r.size() - 2);
  } else {
    // If it's not a quoted string, it must be an invalid literal because it
    // reached this point
    LOG_ERROR(
        "[ConditionEvaluator] Cannot compare string with unquoted literal "
        "token: " +
        rhs);
    return EvalResult::ERROR;
  }

  return compare_values(l, op, r);
}

inline EvalResult dispatch_coerce_and_compare(const std::any& lhs,
                                              const std::string& op,
                                              const std::string& rhs_token) {
  if (lhs.type() == typeid(int))
    return coerce_and_compare<int>(lhs, op, rhs_token);
  if (lhs.type() == typeid(double))
    return coerce_and_compare<double>(lhs, op, rhs_token);
  if (lhs.type() == typeid(float))
    return coerce_and_compare<float>(lhs, op, rhs_token);
  if (lhs.type() == typeid(bool))
    return coerce_and_compare<bool>(lhs, op, rhs_token);
  if (lhs.type() == typeid(std::string))
    return coerce_and_compare<std::string>(lhs, op, rhs_token);
  return EvalResult::ERROR;
}

// Core Value Dispatcher
inline EvalResult evaluate_any_comparison(const std::any& lhs,
                                          const std::string& op,
                                          const std::any& rhs) {
  if (lhs.type() != typeid(LiteralToken) &&
      rhs.type() != typeid(LiteralToken)) {
    if (lhs.type() != rhs.type()) {
      LOG_ERROR("[ConditionEvaluator] Type Mismatch: Cannot compare '"
                << lhs.type().name() << "' with '" << rhs.type().name()
                << "'.");
      return EvalResult::ERROR;
    }
  }

  // If BOTH are literals, try numeric compare first, fallback to string compare
  if (lhs.type() == typeid(LiteralToken) &&
      rhs.type() == typeid(LiteralToken)) {
    std::string l = std::any_cast<LiteralToken>(lhs).str;
    std::string r = std::any_cast<LiteralToken>(rhs).str;

    if (l.size() >= 2 && l.front() == '"' && l.back() == '"')
      l = l.substr(1, l.size() - 2);
    if (r.size() >= 2 && r.front() == '"' && r.back() == '"')
      r = r.substr(1, r.size() - 2);

    try {
      size_t pos_l, pos_r;
      double num_l = std::stod(l, &pos_l);
      double num_r = std::stod(r, &pos_r);
      if (pos_l == l.size() && pos_r == r.size()) {
        return compare_values(num_l, op, num_r);
      }
    } catch (...) {
    }

    return compare_values(l, op, r);
  }

  // If LHS is a primitive/known type, and RHS is a literal token, coerce RHS
  // and compare
  if (rhs.type() == typeid(LiteralToken)) {
    return dispatch_coerce_and_compare(lhs, op,
                                       std::any_cast<LiteralToken>(rhs).str);
  }
  // If RHS is a known type, and LHS is a literal token
  else if (lhs.type() == typeid(LiteralToken)) {
    std::string lhs_token = std::any_cast<LiteralToken>(lhs).str;
    // Reverse operations (e.g. 7 > $var is same as $var < 7)
    std::string rev_op = op;
    if (op == ">")
      rev_op = "<";
    else if (op == "<")
      rev_op = ">";
    else if (op == ">=")
      rev_op = "<=";
    else if (op == "<=")
      rev_op = ">=";

    return dispatch_coerce_and_compare(rhs, rev_op, lhs_token);
  }
  // If both are exact matching types (Var == Var)
  else if (lhs.type() == rhs.type()) {
    if (lhs.type() == typeid(int))
      return compare_values(std::any_cast<int>(lhs), op,
                            std::any_cast<int>(rhs));
    if (lhs.type() == typeid(double))
      return compare_values(std::any_cast<double>(lhs), op,
                            std::any_cast<double>(rhs));
    if (lhs.type() == typeid(float))
      return compare_values(std::any_cast<float>(lhs), op,
                            std::any_cast<float>(rhs));
    if (lhs.type() == typeid(bool))
      return compare_values(std::any_cast<bool>(lhs), op,
                            std::any_cast<bool>(rhs));
    if (lhs.type() == typeid(std::string))
      return compare_values(std::any_cast<std::string>(lhs), op,
                            std::any_cast<std::string>(rhs));
  }

  LOG_WARN("[ConditionEvaluator] Unsupported comparison types.");
  return EvalResult::ERROR;
}

// Tokenize an expression string into tokens, preserving multi-char operators
inline std::vector<std::string> tokenize(const std::string& expr) {
  std::vector<std::string> tokens;
  std::string current;

  for (size_t i = 0; i < expr.size(); ++i) {
    char c = expr[i];

    if (c == ' ' || c == '\t') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    if (i + 1 < expr.size()) {
      std::string two = expr.substr(i, 2);
      if (two == "&&" || two == "||" || two == ">=" || two == "<=" ||
          two == "==" || two == "!=") {
        if (!current.empty()) {
          tokens.push_back(current);
          current.clear();
        }
        tokens.push_back(two);
        ++i;
        continue;
      }
    }

    if (c == '>' || c == '<' || c == '!') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      tokens.push_back(std::string(1, c));
      continue;
    }

    if (c == '"') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      std::string quoted = "\"";
      ++i;
      while (i < expr.size() && expr[i] != '"') {
        quoted += expr[i++];
      }
      quoted += "\"";
      tokens.push_back(quoted);
      continue;
    }

    current += c;
  }
  if (!current.empty()) tokens.push_back(current);
  return tokens;
}

// Simple AST Parser
inline ASTNode* parse_node(const std::string& token) {
  if (token.empty()) return new LiteralNode("");
  if (token[0] == '$') return new VariableNode(token.substr(1));

  // Strict check: if it's not a number, not a quoted string, and not
  // true/false/1/0, it's invalid
  if (token != "true" && token != "false" && token != "1" && token != "0") {
    if (token.front() != '"' || token.back() != '"') {
      // Could be a number?
      try {
        size_t pos;
        std::stod(token, &pos);
        if (pos != token.size()) {
          throw std::invalid_argument(
              "Unrecognized raw identifier or missing '$' for variable: " +
              token);
        }
      } catch (const std::invalid_argument& e) {
        throw;  // re-throw the specific syntax error
      } catch (...) {
        throw std::invalid_argument(
            "Unrecognized raw identifier or missing '$' for variable: " +
            token);
      }
    }
  }

  return new LiteralNode(token);
}

// Validates expression syntax without evaluating Blackboard, and extracts
// variable references.
inline bool validate_expression_syntax_and_vars(
    const std::string& expr, std::vector<std::string>& out_vars,
    std::string& out_error) {
  if (expr.empty()) return true;
  auto tokens = tokenize(expr);
  if (tokens.empty()) return true;

  size_t pos = 0;
  auto dry_parse_comparison = [&](const std::vector<std::string>& t,
                                  size_t& p) -> bool {
    if (p < t.size() && t[p] == "!") p++;
    if (p >= t.size()) {
      out_error = "Unexpected end of expression after '!'.";
      return false;
    }

    std::string lhs_token = t[p++];
    try {
      std::unique_ptr<ASTNode> dummy(parse_node(lhs_token));
      if (lhs_token[0] == '$') out_vars.push_back(lhs_token.substr(1));
    } catch (const std::exception& e) {
      out_error = e.what();
      return false;
    }

    if (p >= t.size() || t[p] == "&&" || t[p] == "||") return true;

    std::string op = t[p++];
    if (op != "==" && op != "!=" && op != ">" && op != "<" && op != ">=" &&
        op != "<=") {
      out_error = "Invalid comparison operator: " + op;
      return false;
    }

    if (p >= t.size()) {
      out_error = "Missing right-hand side token for operator '" + op + "'.";
      return false;
    }

    std::string rhs_token = t[p++];
    try {
      std::unique_ptr<ASTNode> dummy(parse_node(rhs_token));
      if (rhs_token[0] == '$') out_vars.push_back(rhs_token.substr(1));
    } catch (const std::exception& e) {
      out_error = e.what();
      return false;
    }
    return true;
  };

  while (pos < tokens.size()) {
    if (!dry_parse_comparison(tokens, pos)) return false;
    if (pos < tokens.size()) {
      std::string log_op = tokens[pos++];
      if (log_op != "&&" && log_op != "||") {
        out_error =
            "Invalid consecutive tokens or logical operator: '" + log_op + "'";
        return false;
      }
      if (pos >= tokens.size()) {
        out_error = "Unexpected end of expression after logical operator '" +
                    log_op + "'.";
        return false;
      }
    }
  }
  return true;
}

// Main entry point for evaluating expressions
inline EvalResult evaluate_expression(Blackboard& db, const std::string& expr) {
  if (expr.empty()) {
    LOG_ERROR("[ConditionEvaluator] Expression cannot be empty.");
    return EvalResult::ERROR;
  }

  auto tokens = tokenize(expr);
  if (tokens.empty()) {
    LOG_ERROR("[ConditionEvaluator] Expression contains no valid tokens.");
    return EvalResult::ERROR;
  }

  auto parse_comparison = [&](const std::vector<std::string>& t,
                              size_t& pos) -> EvalResult {
    // Check for unary NOT
    bool negate = false;
    if (pos < t.size() && t[pos] == "!") {
      negate = true;
      pos++;
    }

    if (pos >= t.size()) return EvalResult::ERROR;

    std::unique_ptr<ASTNode> lhs(parse_node(t[pos++]));

    if (pos >= t.size() || t[pos] == "&&" || t[pos] == "||") {
      std::any result = lhs->evaluate(db);
      if (!result.has_value()) return EvalResult::ERROR;

      bool final_bool = false;
      if (result.type() == typeid(bool))
        final_bool = std::any_cast<bool>(result);
      else if (result.type() == typeid(int))
        final_bool = std::any_cast<int>(result) != 0;
      else if (result.type() == typeid(std::string))
        final_bool = !std::any_cast<std::string>(result).empty();
      else if (result.type() == typeid(LiteralToken)) {
        std::string raw = std::any_cast<LiteralToken>(result).str;
        if (raw == "true" || raw == "1") {
          final_bool = true;
        } else if (raw == "false" || raw == "0") {
          final_bool = false;
        } else {
          LOG_ERROR(
              "[ConditionEvaluator] Evaluating non-boolean literal as "
              "boolean: " +
              raw + " in expression: " + expr);
          return EvalResult::ERROR;  // Fail execution
        }
      } else {
        LOG_ERROR("[ConditionEvaluator] Standalone variable '"
                  << t[pos - 1] << "' of type [" << result.type().name()
                  << "] is NOT a valid boolean condition.");
        return EvalResult::ERROR;
      }

      final_bool = negate ? !final_bool : final_bool;
      return (final_bool ? EvalResult::TRUE : EvalResult::FALSE);
    }

    // Otherwise, there is a binary comparison operator
    std::string op = t[pos++];
    if (pos >= t.size()) return EvalResult::ERROR;
    std::unique_ptr<ASTNode> rhs(parse_node(t[pos++]));

    EvalResult comp_result =
        evaluate_any_comparison(lhs->evaluate(db), op, rhs->evaluate(db));
    if (comp_result == EvalResult::ERROR) {
      LOG_ERROR("Fail to evaluate comparison: " + expr);
      return EvalResult::ERROR;  // Abort operation sequence
    }

    // Invert the result if there was a `!` (1 -> 0, 0 -> 1)
    if (negate)
      comp_result = (comp_result == EvalResult::TRUE) ? EvalResult::FALSE
                                                      : EvalResult::TRUE;
    return comp_result;
  };

  try {
    size_t pos = 0;
    EvalResult current_and_result = parse_comparison(tokens, pos);
    if (current_and_result == EvalResult::ERROR) return EvalResult::ERROR;

    EvalResult final_result = EvalResult::FALSE;
    bool have_or = false;

    while (pos < tokens.size()) {
      const std::string& logical_op = tokens[pos++];
      if (logical_op == "&&") {
        EvalResult next = parse_comparison(tokens, pos);
        if (next == EvalResult::ERROR)
          return EvalResult::ERROR;  // Fast-fail on syntax error
        current_and_result =
            (current_and_result == EvalResult::TRUE && next == EvalResult::TRUE)
                ? EvalResult::TRUE
                : EvalResult::FALSE;
      } else if (logical_op == "||") {
        if (!have_or) {
          final_result = current_and_result;
          have_or = true;
        } else {
          final_result = (final_result == EvalResult::TRUE ||
                          current_and_result == EvalResult::TRUE)
                             ? EvalResult::TRUE
                             : EvalResult::FALSE;
        }
        current_and_result = parse_comparison(tokens, pos);
        if (current_and_result == EvalResult::ERROR)
          return EvalResult::ERROR;  // Fast-fail on syntax error
      } else {
        LOG_WARN("[ConditionEvaluator] Unexpected logical token: '" +
                 logical_op + "' in expression: " + expr);
        return EvalResult::ERROR;
      }
    }

    if (have_or)
      return (final_result == EvalResult::TRUE ||
              current_and_result == EvalResult::TRUE)
                 ? EvalResult::TRUE
                 : EvalResult::FALSE;
    return current_and_result;

  } catch (const std::exception& e) {
    LOG_ERROR("Failed to parse condition: " << expr);
    LOG_ERROR("Unexpected error in ConditionEvaluator: " +
              std::string(e.what()));
  }
  return EvalResult::ERROR;
}

}  // namespace ConditionEvaluator
