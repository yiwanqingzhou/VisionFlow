#pragma once
#include <any>
#include <mutex>
#include <string>
#include <unordered_map>

struct Blackboard {
  std::unordered_map<std::string, std::any> data;
  std::unordered_map<std::string, long long> metrics;
  std::mutex mtx;

  template <typename T> void write(const std::string &key, T value) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = value;
  }

  template <typename T> T read(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    return data.count(key) ? std::any_cast<T>(data[key]) : T();
  }

  void record_metric(const std::string &id, long long ms) {
    std::lock_guard<std::mutex> lock(mtx);
    metrics[id] += ms;
  }
};
