#include "pch.h"
#pragma once
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

class SimpleIniParser {
public:
    bool load(const std::string& filename);

    std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& section, const std::string& key, int defaultValue = 0) const;

private:
    std::map<std::string, std::map<std::string, std::string>> data_;
};