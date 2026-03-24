/**
 * \file	Environment.cpp
 * \author	Daniel Meister
 * \date	2014/05/10
 * \brief	Environment class source file.
 */

#include "Environment.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "../../../external/tinygltf/json.hpp"

using json = nlohmann::json;

Environment * Environment::instance = nullptr;

bool Environment::filterValue(const std::string & value, std::string & filteredValue, OptType type) {
    bool valid = true;
    if (type == OPT_INT) {
        try {
            int val = std::stoi(value);
            filteredValue = std::to_string(val);
        } catch (...) {
            valid = false;
        }
    }
    else if (type == OPT_FLOAT) {
        try {
            float val = std::stof(value);
            filteredValue = std::to_string(val);
        } catch (...) {
            valid = false;
        }
    }
    else if (type == OPT_BOOL) {
        std::string v = value;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        if (v == "true" || v == "yes" || v == "on" || v == "1") {
            filteredValue = "1";
        }
        else if (v == "false" || v == "no" || v == "off" || v == "0") {
            filteredValue = "0";
        }
        else {
            valid = false;
        }
    }
    else if (type == OPT_VECTOR) {
        std::stringstream ss(value);
        float v[3];
        int count = 0;
        while (ss >> v[count]) {
            count++;
            if (count == 3) break;
        }
        valid = (count == 3);
        if (valid) {
            filteredValue = std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]);
        }
    }
    else {
        filteredValue = value;
        if (value.empty()) valid = false;
    }
    return valid;
}

bool Environment::findOption(const std::string & name, Option & option) {
    auto it = options.find(name);
    if (it != options.end()) {
        option = it->second;
        return true;
    }
    return false;
}

void Environment::registerOption(const std::string & name, const std::string defaultValue, OptType type) {
    Option opt;
    if (!filterValue(defaultValue, opt.defaultValue, type)) {
        std::cerr << "ERROR <Environment> Invalid default value for option '" << name << "'.\n";
        exit(EXIT_FAILURE);
    }
    opt.name = name;
    opt.type = type;
    options[name] = opt;
}

void Environment::registerOption(const std::string & name, OptType type) {
    Option opt;
    opt.name = name;
    opt.type = type;
    options[name] = opt;
}

Environment * Environment::getInstance() {
    if (!instance)
        std::cerr << "WARN <Environment> Environment is not allocated.\n";
    return instance;
}

void Environment::deleteInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void Environment::setInstance(Environment * instance) {
    Environment::instance = instance;
}

Environment::Environment() {
}

Environment::~Environment() {
}

bool Environment::getIntValue(const std::string & name, int & value) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty() && !opt.values.front().empty()) {
        value = std::stoi(opt.values.front());
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        value = std::stoi(opt.defaultValue);
        return true;
    }
    return false;
}

bool Environment::getFloatValue(const std::string & name, float & value) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty() && !opt.values.front().empty()) {
        value = std::stof(opt.values.front());
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        value = std::stof(opt.defaultValue);
        return true;
    }
    return false;
}

bool Environment::getBoolValue(const std::string & name, bool & value) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty() && !opt.values.front().empty()) {
        value = (bool)std::stoi(opt.values.front());
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        value = (bool)std::stoi(opt.defaultValue);
        return true;
    }
    return false;
}

bool Environment::getVectorValue(const std::string & name, glm::vec3 & value) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    std::string valStr;
    if (!opt.values.empty() && !opt.values.front().empty()) {
        valStr = opt.values.front();
    }
    else if (!opt.defaultValue.empty()) {
        valStr = opt.defaultValue;
    } else {
        return false;
    }
    std::stringstream ss(valStr);
    ss >> value.x >> value.y >> value.z;
    return true;
}

bool Environment::getStringValue(const std::string & name, std::string & value) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty() && !opt.values.front().empty()) {
        value = opt.values.front();
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        value = opt.defaultValue;
        return true;
    }
    return false;
}

bool Environment::getIntValues(const std::string & name, std::vector<int> & values) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty()) {
        values.clear();
        for (auto& v : opt.values) values.push_back(std::stoi(v));
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        values.push_back(std::stoi(opt.defaultValue));
        return true;
    }
    return false;
}

bool Environment::getFloatValues(const std::string & name, std::vector<float> & values) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty()) {
        values.clear();
        for (auto& v : opt.values) values.push_back(std::stof(v));
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        values.push_back(std::stof(opt.defaultValue));
        return true;
    }
    return false;
}

bool Environment::getBoolValues(const std::string & name, std::vector<bool> & values) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty()) {
        values.clear();
        for (auto& v : opt.values) values.push_back((bool)std::stoi(v));
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        values.push_back((bool)std::stoi(opt.defaultValue));
        return true;
    }
    return false;
}

bool Environment::getVectorValues(const std::string & name, std::vector<glm::vec3> & values) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty()) {
        values.clear();
        for (auto& vStr : opt.values) {
            glm::vec3 v;
            std::stringstream ss(vStr);
            ss >> v.x >> v.y >> v.z;
            values.push_back(v);
        }
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        glm::vec3 v;
        std::stringstream ss(opt.defaultValue);
        ss >> v.x >> v.y >> v.z;
        values.push_back(v);
        return true;
    }
    return false;
}

bool Environment::getStringValues(const std::string & name, std::vector<std::string> & values) {
    auto it = options.find(name);
    if (it == options.end()) return false;
    Option& opt = it->second;
    if (!opt.values.empty()) {
        values = opt.values;
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        values.push_back(opt.defaultValue);
        return true;
    }
    return false;
}

bool Environment::readEnvFile(const std::string & filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "WARN <Environment> File '" << filename << "' does not exist or cannot be opened.\n";
        return false;
    }

    json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        std::cerr << "ERROR <Environment> Failed to parse JSON: " << e.what() << "\n";
        return false;
    }

    for (auto& it : options) {
        Option& opt = it.second;
        std::string name = opt.name;
        
        // Find in JSON. Support nested structure e.g. "Renderer.numberOfAOSamples" -> data["Renderer"]["numberOfAOSamples"]
        json* current = &data;
        std::stringstream ss(name);
        std::string segment;
        bool found = true;
        
        while (std::getline(ss, segment, '.')) {
            if (current->is_object()) {
                auto it_json = current->find(segment);
                if (it_json != current->end()) {
                    current = &(*it_json);
                } else {
                    found = false;
                    break;
                }
            } else {
                found = false;
                break;
            }
        }

        if (found) {
            opt.values.clear();
            auto process_element = [&](const json& el) {
                if (el.is_string()) {
                    opt.values.push_back(el.get<std::string>());
                } else if (el.is_boolean()) {
                    opt.values.push_back(el.get<bool>() ? "1" : "0");
                } else {
                    opt.values.push_back(el.dump());
                }
            };

            if (current->is_array()) {
                for (auto& el : *current) {
                    process_element(el);
                }
            } else {
                process_element(*current);
            }
        }
    }

    return true;
}
