#include <unordered_map>
#include <vector>
#include <list>
#include <string>
#include "Scene.hpp"

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
    std::vector<std::string> action_names;
    std::unordered_map<std::string, int*> properties;
    std::vector<std::string> property_names;
    std::unordered_map<std::string, Scene::Drawable*> drawables;
    Scene::Transform* transform;
    glm::vec2 start_position;
    float health_level = 1.0f;
    float floor_height = 0.f;

    Object(std::string name);
    void addAction(std::string action_name, ActionFunction func, float duration);
    void addProperty(std::string property_name, int default_value);
    void reset();
    int& property(std::string property_name);
    void updateHealth();
};

#endif