#include "utils/logger.hpp"

namespace vf {

const std::unordered_map<std::string, Logger::Level>
    Logger::map_level_from_str = {{"VERBOSE", VERBOSE}, {"DEBUG", DEBUG},
                                  {"INFO", INFO},       {"WARN", WARN},
                                  {"ERROR", ERROR},     {"FATAL", FATAL}};

const std::unordered_map<Logger::Level, const char*> Logger::map_level_to_str =
    {{VERBOSE, "VERBOSE"}, {DEBUG, "DEBUG"}, {INFO, "INFO"},
     {WARN, "WARN"},       {ERROR, "ERROR"}, {FATAL, "FATAL"}};

const std::unordered_map<Logger::Level, const char*>
    Logger::map_level_to_color = {{VERBOSE, NO_COLOR}, {DEBUG, FG_GREEN},
                                  {INFO, NO_COLOR},    {WARN, FG_YELLOW},
                                  {ERROR, FG_RED},     {FATAL, FG_BRED}};

void Logger::setLevel(const std::string& level_str) {
  auto it = map_level_from_str.find(level_str);
  if (it != map_level_from_str.end()) {
    filter_level_ = it->second;
  }
}

Logger& Logger::instance() {
  static Logger instance_;
  return instance_;
}

const char* Logger::getLevelName(Level level) const {
  auto it = map_level_to_str.find(level);
  return (it != map_level_to_str.end()) ? it->second : "UNKNOWN";
}

const char* Logger::getLevelColor(Level level) const {
  auto it = map_level_to_color.find(level);
  return (it != map_level_to_color.end()) ? it->second : NO_COLOR;
}

std::string Logger::getTimestamp() const {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << '.'
     << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}

void Logger::print(Level level, const char* file, int line,
                   const std::string& msg) {
  if (level < filter_level_) {
    return;
  }

  std::stringstream output;

  // Format: COLOR[LEVEL] Timestamp (File:Line): msg NO_COLOR
  output << getLevelColor(level) << "[" << getLevelName(level) << "] "
         << getTimestamp() << " ";
  if (show_source_loc_) {
    output << "(" << file << ":" << line << ") ";
  }
  output << msg << NO_COLOR;

  std::lock_guard<std::mutex> lock(mtx_);
  std::cout << output.str() << std::endl;
}

void Logger::print(Level level, const char* file, int line,
                   const std::stringstream& ss) {
  print(level, file, line, ss.str());
}

}  // namespace vf
