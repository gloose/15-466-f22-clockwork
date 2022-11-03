#include <unordered_map>
#include <vector>
#include <list>
#include <string>

#ifndef _OBJECT_H_
#define _OBJECT_H_

struct Object;

typedef void (*ActionFunction)(Object*, Object*);

struct Action {
    ActionFunction func;
    float duration;

    Action(ActionFunction func, float duration);
};

struct Object {
    std::string name = "";
    std::unordered_map<std::string, Action> actions;
    std::unordered_map<std::string, int*> properties;

    Object(std::string name);
    void addAction(std::string action_name, ActionFunction func, float duration);
    void addProperty(std::string property_name, int default_value);
    void reset();
    int& property(std::string property_name);
};

static std::string formatCase(std::string str);

#endif