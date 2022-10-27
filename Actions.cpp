#include "Actions.hpp"

void attack(int damage, Compiler::Object* target) {
	target->property("HEALTH") -= (damage / target->property("DEFENSE"));
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
	}
}

void attack_function(Compiler::Object* user, Compiler::Object* target) {
	attack(20, target);
}

void defend_function(Compiler::Object* user, Compiler::Object* target) {
	target->property("DEFENDED") = 1;
	user->property("DEFENDING") = 1;
}

void freeze_function(Compiler::Object* user, Compiler::Object* target) {
	target->property("FROZEN") = 1;
}

void burn_function(Compiler::Object* user, Compiler::Object* target) {
	target->property("BURNED") = 1;
}

void heal_function(Compiler::Object* user, Compiler::Object* target) {
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
}

void shoot_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ARROWS") > 0) {
		attack(25, target);
		user->property("ARROWS")--;
	}
}