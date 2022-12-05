#pragma once

#include "Animation.hpp"
#include "Compiler.hpp"
#include <iostream>
#include <random>
#include <string>

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

void attack_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void gunner_attack_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void freeze_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void burn_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void heal_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void full_heal_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void burn_heal_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void shoot_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void shockwave_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void kill_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void destroy_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);
void annihilate_function(Compiler* compiler, Object* user, Object* target, bool* result, float duration);

#endif