/**
 * \file	Environment.h
 * \author	Daniel Meister
 * \date	2014/08/19
 * \brief	Environment class header file.
 */

#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

enum OptType {
    OPT_INT,
    OPT_FLOAT,
    OPT_BOOL,
    OPT_VECTOR,
    OPT_STRING
};

struct Option {
    OptType type;
    std::string name;
    std::vector<std::string> values;
    std::string defaultValue;
};

class Environment {

private:

    static Environment * instance;

    std::unordered_map<std::string, Option> options;

    bool filterValue(const std::string & value, std::string & filteredValue, OptType type);
    bool findOption(const std::string & name, Option & option);

protected:

    void registerOption(const std::string & name, const std::string defaultValue, OptType type);
    void registerOption(const std::string & name, OptType type);
    virtual void registerOptions(void) = 0;

public:

    static Environment * getInstance(void);
    static void deleteInstance(void);
    static void setInstance(Environment * instance);

    Environment(void);
    virtual ~Environment(void);

    bool getIntValue(const std::string & name, int & value);
    bool getFloatValue(const std::string & name, float & value);
    bool getBoolValue(const std::string & name, bool & value);
    bool getVectorValue(const std::string & name, glm::vec3 & value);
    bool getStringValue(const std::string & name, std::string & value);

    bool getIntValues(const std::string & name, std::vector<int> & values);
    bool getFloatValues(const std::string & name, std::vector<float> & values);
    bool getBoolValues(const std::string & name, std::vector<bool> & values);
    bool getVectorValues(const std::string & name, std::vector<glm::vec3> & values);
    bool getStringValues(const std::string & name, std::vector<std::string> & values);

    bool readEnvFile(const std::string & filename);

};

#endif /* _ENVIRONMENT_H_ */
