#pragma once

#include "Animation.hpp"
#include "Compiler.hpp"
#include <iostream>
#include <random>
#include <string>

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

void attack_function(Compiler* compiler, Object* user, Object* target);
void gunner_attack_function(Compiler* compiler, Object* user, Object* target);
void defend_function(Compiler* compiler, Object* user, Object* target);
void freeze_function(Compiler* compiler, Object* user, Object* target);
void burn_function(Compiler* compiler, Object* user, Object* target);
void heal_function(Compiler* compiler, Object* user, Object* target);
void full_heal_function(Compiler* compiler, Object* user, Object* target);
void burn_heal_function(Compiler* compiler, Object* user, Object* target);
void shoot_function(Compiler* compiler, Object* user, Object* target);
void shockwave_function(Compiler* compiler, Object* user, Object* target);
void kill_function(Compiler* compiler, Object* user, Object* target);
void destroy_function(Compiler* compiler, Object* user, Object* target);
void annihilate_function(Compiler* compiler, Object* user, Object* target);

std::string& get_action_string();
std::string& get_effect_string();

#endif