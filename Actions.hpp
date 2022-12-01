#pragma once

#include "Animation.hpp"
#include "Compiler.hpp"
#include <iostream>
#include <random>
#include <string>

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

void attack_function(Compiler* compiler, Object* user, Object* target, bool* result);
void gunner_attack_function(Compiler* compiler, Object* user, Object* target, bool* result);
void freeze_function(Compiler* compiler, Object* user, Object* target, bool* result);
void burn_function(Compiler* compiler, Object* user, Object* target, bool* result);
void heal_function(Compiler* compiler, Object* user, Object* target, bool* result);
void full_heal_function(Compiler* compiler, Object* user, Object* target, bool* result);
void burn_heal_function(Compiler* compiler, Object* user, Object* target, bool* result);
void shoot_function(Compiler* compiler, Object* user, Object* target, bool* result);
void shockwave_function(Compiler* compiler, Object* user, Object* target, bool* result);
void kill_function(Compiler* compiler, Object* user, Object* target, bool* result);
void destroy_function(Compiler* compiler, Object* user, Object* target, bool* result);
void annihilate_function(Compiler* compiler, Object* user, Object* target, bool* result);

#endif