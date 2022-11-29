#include "Object.hpp"
#include <algorithm>

std::string formatCase(std::string str) {
    auto upperChar = [](char c) {
        return (char)toupper(c);
    };
    std::transform(str.begin(), str.end(), str.begin(), upperChar);
    return str;
}

// Construct action with function and duration
Action::Action(ActionFunction func, float duration, bool has_target) : func(func), duration(duration), has_target(has_target) {}

// Construct object with name
Object::Object(std::string name, Team team) : name(formatCase(name)), team(team) {}

// Add action to object's map of valid actions
void Object::addAction(std::string action_name, ActionFunction func, float duration, bool has_target) {
    action_name = formatCase(action_name);
    actions.emplace(action_name, Action(func, duration, has_target));
    action_names.push_back(action_name);
}

// Add property to object's map of properties
void Object::addProperty(std::string property_name, int default_value) {
    property_name = formatCase(property_name);
    properties.emplace(property_name, new int(default_value));
    property_names.push_back(property_name);
}

void Object::updateHealth() {
    health_level = std::max(0.0f, (float)property("HEALTH") / (float)property("HEALTH_MAX"));
}

// Reset an object
void Object::reset() {
    property("ALIVE") = 1;
    property("BURNED") = 0;
    property("FROZEN") = 0;
    property("HEALTH") = property("HEALTH_MAX");
    if (name == "RANGER") {
        property("ARROWS") = 8;
    }
    updateHealth();
    transform->position = glm::vec3(start_position, floor_height);
    transform->rotation = start_rotation;
}

// Returns a reference to the property named property_name
// If the object has no such property, create one with default value 0
int& Object::property(std::string property_name) {
    property_name = formatCase(property_name);
    auto prop = properties.find(property_name);
    if (prop == properties.end()) {
        addProperty(property_name, 0);
        prop = properties.find(property_name);
    }
    return *prop->second;
}
