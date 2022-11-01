#include "Actions.hpp"

std::string action_string = "";
std::string effect_string = "";

int calc_damage(int damage, Compiler::Object* target) {
	return (int)std::round(damage * (100 + target->property("DEFENSE")) / 100.);
}

void attack(int damage, Compiler::Object* target) {
	target->property("HEALTH") -= calc_damage(damage, target);
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
		effect_string = target->name + " has been killed.";
	} else {
		effect_string = target->name + " has " + std::to_string(target->property("HEALTH")) + " health.";
	}
}

bool check_burn(Compiler::Object* user) {
	if (user->property("BURNED") == 1) {
		user->property("HEALTH") -= 10;
		if (user->property("HEALTH") <= 0) {
			user->property("ALIVE") = 0;
			effect_string = user->name + " died to burn damage.";
			return true;
		}
	}
	return false;
}

bool check_freeze(Compiler::Object* user) {
	srand((unsigned int)time(NULL));
	if (user->property("FROZEN") == 1 && ((rand() % 3) == 0)) {
		effect_string = user->name + " was frozen and could not move.";
		return true;
	}
	return false;
}

void attack_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	int damage = user->property("POWER");
	action_string = user->name + " attacked " + target->name + " for " + std::to_string(calc_damage(damage, target)) + " damage.";
	attack(damage, target);
}

void defend_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	action_string = user->name + " defended " + target->name + ".";
	target->property("DEFENDED") = 1;
	user->property("DEFENDING") = 1;
}

void freeze_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	action_string = user->name + " froze " + target->name + ".";
	target->property("FROZEN") = 1;
}

void burn_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	action_string = user->name + " burned " + target->name + ".";
	target->property("BURNED") = 1;
}

void heal_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	action_string = user->name + " healed " + target->name + ".";
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
	effect_string = target->name + " has " + std::to_string(target->property("HEALTH")) + " health.";
}

void shoot_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	
	if (user->property("ARROWS") > 0) {
		attack(20, target);
		user->property("ARROWS")--;
		action_string = user->name + " shot " + target->name + " and has " + std::to_string(user->property("ARROWS")) + " arrows remaining.";
	} else {
		action_string = user->name + " tried to shoot " + target->name + ", but was out of arrows!";
	}
}

std::string& get_action_string() { return action_string; }
std::string& get_effect_string() { return effect_string; }