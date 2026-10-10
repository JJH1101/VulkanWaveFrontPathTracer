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
#include <utility>
#include <cmath>
#include "../../../external/tinygltf/json.hpp"
#include "VulkanTools.h"

using json = nlohmann::json;

std::unique_ptr<Environment> Environment::instance;

bool Environment::filterValue(const std::string & value, std::string & filteredValue, OptType type) {
    bool valid = true;
    if (type == OPT_INT) {
        std::stringstream ss(value);
        int val;
        valid = static_cast<bool>(ss >> val);
        if (valid) {
            ss >> std::ws;
            valid = ss.eof();
            filteredValue = value;
        }
    }
    else if (type == OPT_FLOAT) {
        try {
            size_t parsed;
            float val = std::stof(value, &parsed);
            valid = std::isfinite(val) && value.find_first_not_of(" \t\r\n", parsed) == std::string::npos;
            filteredValue = value;
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
        valid = static_cast<bool>(ss >> v[0] >> v[1] >> v[2]);
        if (valid) {
            ss >> std::ws;
            valid = ss.eof() && std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
            filteredValue = value;
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
    return instance.get();
}

void Environment::deleteInstance() {
    instance.reset();
}

void Environment::setInstance(std::unique_ptr<Environment> instance) {
    Environment::instance = std::move(instance);
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
    glm::vec3 parsed;
    if (!(ss >> parsed.x >> parsed.y >> parsed.z)) return false;
    value = parsed;
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
            if (!(ss >> v.x >> v.y >> v.z)) return false;
            values.push_back(v);
        }
        return true;
    }
    else if (!opt.defaultValue.empty()) {
        glm::vec3 v;
        std::stringstream ss(opt.defaultValue);
        if (!(ss >> v.x >> v.y >> v.z)) return false;
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
    json data;

#if defined(__ANDROID__) && !USE_ANDROID_TMP_PATH
    AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        vks::tools::exitFatal("ERROR <Environment> Failed to load JSON \"" + filename + "\"", -1);
        return false;
    }

    size_t size = AAsset_getLength(asset);
    std::vector<char> buffer(size);
    int readBytes = AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    if (readBytes < 0) {
        vks::tools::exitFatal("ERROR <Environment> Failed to read Asset \"" + filename + "\"", -1);
        return false;
    }

    try {
        data = json::parse(buffer.begin(), buffer.end());
    }
    catch (const std::exception& e) {
        vks::tools::exitFatal("ERROR <Environment> Failed to parse JSON \"" + filename + "\": " + e.what(), -1);
        return false;
    }
#else
    std::ifstream file(filename);
    if (!file.is_open()) {
        vks::tools::exitFatal("ERROR <Environment> Failed to load file \"" + filename + "\"", -1);
        return false;
    }

    try {
        file >> data;
    }
    catch (const std::exception& e) {
        vks::tools::exitFatal("ERROR <Environment> Failed to parse JSON \"" + filename + "\": " + e.what(), -1);
        return false;
    }
#endif

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
                std::string value;
                if (el.is_string()) {
                    value = el.get<std::string>();
                } else if (el.is_boolean()) {
                    value = el.get<bool>() ? "1" : "0";
                } else {
                    value = el.dump();
                }
                std::string filtered;
                if (!filterValue(value, filtered, opt.type))
                    vks::tools::exitFatal("Invalid value for environment option '" + name + "': " + value, -1);
                opt.values.push_back(filtered);
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
