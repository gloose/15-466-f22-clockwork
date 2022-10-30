#include "Compiler.hpp"
#include <iostream>
#include <random>
#include <string>

void attack_function(Compiler::Object* user, Compiler::Object* target);
void defend_function(Compiler::Object* user, Compiler::Object* target);
void freeze_function(Compiler::Object* user, Compiler::Object* target);
void burn_function(Compiler::Object* user, Compiler::Object* target);
void heal_function(Compiler::Object* user, Compiler::Object* target);
void shoot_function(Compiler::Object* user, Compiler::Object* target);

std::string& get_action_string();
std::string& get_effect_string();