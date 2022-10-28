#include "Actions.hpp"

void attack(int damage, Compiler::Object* target) {
	target->property("HEALTH") -= (damage / target->property("DEFENSE"));
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
	}
}

bool check_burn(Compiler::Object* user) {
	if (user->property("BURNED") == 1) {
		user->property("HEALTH") -= 10;
		if (user->property("HEALTH") <= 0) {
			user->property("ALIVE") = 0;
			return true;
		}
	}
	return false;
}

bool check_freeze(Compiler::Object* user) {
	srand((unsigned int)time(NULL));
	if (user->property("FROZEN") == 1 && ((rand() % 3) == 0)) {
		return true;
	}
	return false;
}

void attack_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	attack(20, target);
}

void defend_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	target->property("DEFENDED") = 1;
	user->property("DEFENDING") = 1;
}

void freeze_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	target->property("FROZEN") = 1;
}

void burn_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	target->property("BURNED") = 1;
}

void heal_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
}

void shoot_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ARROWS") > 0) {
		attack(25, target);
		user->property("ARROWS")--;
	}
}
