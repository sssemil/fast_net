#pragma once

#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>
#include <string>

class Config {
 public:
  static in_port_t port;
  static size_t num_requests;
  static std::string logging_level;
  static size_t page_count;

  static void load_config(const std::string& env_file_path) {
    std::ifstream env_file(env_file_path.data());
    std::string line;

    if (!env_file.is_open()) {
      throw std::runtime_error("Failed to open .env file.");
    }

    while (std::getline(env_file, line)) {
      parse_env_line(line);
    }

    env_file.close();

    set_logging_level();

    if (port == 0 || num_requests == 0) {
      throw std::runtime_error("Invalid configuration values.");
    }
  }

  static void load_config() {
    const std::string env_file_path = ".env";
    load_config(env_file_path);
  }

 private:
  static std::string trim(const std::string& str) {
    const size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    const size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
  }

  static void parse_env_line(const std::string& line) {
    if (const size_t delimiter_pos = line.find('=');
        delimiter_pos != std::string::npos) {
      const std::string key = trim(line.substr(0, delimiter_pos));
      const std::string value = trim(line.substr(delimiter_pos + 1));

      if (key == "PORT") {
        port = static_cast<in_port_t>(std::stoul(value));
      } else if (key == "NUM_REQUESTS") {
        num_requests = std::stoul(value);
      } else if (key == "LOGGING_LEVEL") {
        logging_level = value;
      } else if (key == "PAGE_COUNT") {
        page_count = std::stoul(value);
      } else {
        spdlog::warn("Unknown key '{}'.", key);
      }
    }
  }

  static void set_logging_level() {
    if (logging_level == "DEBUG") {
      spdlog::set_level(spdlog::level::debug);
    } else if (logging_level == "INFO") {
      spdlog::set_level(spdlog::level::info);
    } else if (logging_level == "WARN") {
      spdlog::set_level(spdlog::level::warn);
    } else if (logging_level == "ERROR") {
      spdlog::set_level(spdlog::level::err);
    } else if (logging_level == "CRITICAL") {
      spdlog::set_level(spdlog::level::critical);
    } else if (logging_level == "OFF") {
      spdlog::set_level(spdlog::level::off);
    } else {
      spdlog::warn("Unknown LOGGING_LEVEL '{}'. Defaulting to INFO.",
                   logging_level);
      spdlog::set_level(spdlog::level::info);
    }
  }
};

in_port_t Config::port = 0;
size_t Config::num_requests = 0;
std::string Config::logging_level = "INFO";
size_t Config::page_count = 0;