#include "SimpleIniParser.h"

bool SimpleIniParser::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return false;

    std::string line, currentSection;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
        } else {
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            data_[currentSection][key] = value;
        }
    }
    return true;
}

std::string SimpleIniParser::get(const std::string& section, const std::string& key, const std::string& defaultValue) const {
    auto it = data_.find(section);
    if (it != data_.end()) {
        auto it2 = it->second.find(key);
        if (it2 != it->second.end()) {
            return it2->second;
        }
    }
    return defaultValue;
}

int SimpleIniParser::getInt(const std::string& section, const std::string& key, int defaultValue) const {
    try {
        return std::stoi(get(section, key));
    } catch (...) {
        return defaultValue;
    }
}