#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace vf {

// ANSI Color Codes
#define NO_COLOR "\033[0m"
#define FG_BLACK "\033[30m"
#define FG_RED "\033[31m"
#define FG_GREEN "\033[32m"
#define FG_YELLOW "\033[33m"
#define FG_BLUE "\033[34m"
#define FG_MAGENTA "\033[35m"
#define FG_BRED "\033[1;31m"
#define FG_CYAN "\033[36m"

class Logger {
 public:
  enum Level { VERBOSE = 0, DEBUG, INFO, WARN, ERROR, FATAL, COUNT };

  static Logger& instance();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void setLevel(Level level) { filter_level_ = level; }
  void setLevel(const std::string& level_str);
  void setShowSourceLocation(bool show) { show_source_loc_ = show; }

  void print(Level level, const char* file, int line, const std::string& msg);
  void print(Level level, const char* file, int line,
             const std::stringstream& ss);

 private:
#ifdef VF_LOG_SHOW_LOCATION
  Logger() : filter_level_(DEBUG), show_source_loc_(true) {}
#else
  Logger() : filter_level_(DEBUG), show_source_loc_(false) {}
#endif
  ~Logger() = default;

  std::string getTimestamp() const;
  const char* getLevelName(Level level) const;
  const char* getLevelColor(Level level) const;

  static const std::unordered_map<std::string, Level> map_level_from_str;
  static const std::unordered_map<Level, const char*> map_level_to_str;
  static const std::unordered_map<Level, const char*> map_level_to_color;

  std::mutex mtx_;
  Level filter_level_;
  bool show_source_loc_;
};

}  // namespace vf

// Helper Macros
#define LOG_BASE(level, msg)                                                 \
  do {                                                                       \
    std::stringstream ss;                                                    \
    ss << msg;                                                               \
    vf::Logger::instance().print(vf::Logger::level, __FILE__, __LINE__, ss); \
  } while (0)

#define LOG_VERBOSE(msg) LOG_BASE(VERBOSE, msg)
#define LOG_DEBUG(msg) LOG_BASE(DEBUG, msg)
#define LOG_INFO(msg) LOG_BASE(INFO, msg)
#define LOG_WARN(msg) LOG_BASE(WARN, msg)
#define LOG_ERROR(msg) LOG_BASE(ERROR, msg)
#define LOG_FATAL(msg) LOG_BASE(FATAL, msg)
