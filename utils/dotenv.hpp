#pragma once
#include <cstdlib>
#include <fstream>
#include <drogon/drogon.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace dotenv {
    inline std::string trim(const std::string& s){
        size_t start = s.find_first_not_of(" \t\r");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r");
        return s.substr(start, end - start + 1);
    }

    inline void set_var(const std::string& key, const std::string& value, bool overwrite) {
    if (!overwrite && std::getenv(key.c_str())) return;   // real env vars win by default
    #ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
    #else
        setenv(key.c_str(), value.c_str(), 1);
    #endif
    }

    inline bool load_dotenv(const std::string& path = ".env", bool overwrite = false) {
        std::ifstream file(path);
        if (!file) {
            LOG_ERROR << "Failed to open .env file: " << path;
            return false;
        }
        std::string line;
        while (std::getline(file,line)){
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;       
        if (line.rfind("export ", 0) == 0) line = trim(line.substr(7));
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
            value = value.substr(1, value.size() - 2);          
        else if (auto hash = value.find(" #"); hash != std::string::npos)
            value = trim(value.substr(0, hash));         
        set_var(key, value, overwrite);
    }
      return true;
        
    }

    inline std::optional<std::string> get(const std::string& key) {
    const char* v = std::getenv(key.c_str());
    return v ? std::optional<std::string>(v) : std::nullopt;
    }

    inline std::string get_or(const std::string& key, const std::string& fallback) {
    return get(key).value_or(fallback);
    }

    inline std::string require(const std::string& key) {          
    auto v = get(key);
    if (!v) throw std::runtime_error("Missing required environment variable: " + key);
    return *v;
}
} 