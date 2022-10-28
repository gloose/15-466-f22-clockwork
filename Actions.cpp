#include "Actions.hpp"
#include <iostream>

void attack(int damage, Compiler::Object* target) {
	target->property("HEALTH") -= (damage / target->property("DEFENSE"));
	std::cout << "health was " + std::to_string(target->property("HEALTH")) + "\n";
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
	}
	std::cout << "health is now " + std::to_string(target->property("HEALTH")) + "\n";
}

void attack_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " attacked the " << target->name << "\n";
	attack(20, target);
}

void defend_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " defended the " << target->name << "\n";
	target->property("DEFENDED") = 1;
	user->property("DEFENDING") = 1;
}

void freeze_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " froze the " << target->name << "\n";
	target->property("FROZEN") = 1;
}

void burn_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " burned the " << target->name << "\n";
	target->property("BURNED") = 1;
}

void heal_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " healed the " << target->name << "\n";
	std::cout << "health was " + std::to_string(target->property("HEALTH")) + "\n";
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
	std::cout << "health is now " + std::to_string(target->property("HEALTH")) + "\n";
}

void shoot_function(Compiler::Object* user, Compiler::Object* target) {
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	std::cout << user->name << " shot the " << target->name << "\n";
	if (user->property("ARROWS") > 0) {
		attack(25, target);
		user->property("ARROWS")--;
	}
}
