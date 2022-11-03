#include "Object.hpp"
#include <iostream>
#include <random>
#include <string>

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

void attack_function(Object* user, Object* target);
void defend_function(Object* user, Object* target);
void freeze_function(Object* user, Object* target);
void burn_function(Object* user, Object* target);
void heal_function(Object* user, Object* target);
void shoot_function(Object* user, Object* target);

std::string& get_action_string();
std::string& get_effect_string();

#endif