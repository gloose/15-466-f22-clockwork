#include "Actions.hpp"

std::string action_string = "";
std::string effect_string = "";

int calc_damage(int damage, Object* target) {
	return (int)std::round(damage * (100 - target->property("DEFENSE")) / 100.);
}

void attack(int damage, Object* target) {
	target->property("HEALTH") -= calc_damage(damage, target);
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
		add_animation(new DeathAnimation(target));
		effect_string = target->name + " has been killed.";
	} else {
		effect_string = target->name + " has " + std::to_string(target->property("HEALTH")) + " health.";
	}
}

bool check_burn(Object* user) {
	if (user->property("BURNED") == 1) {
		user->property("HEALTH") -= 10;
		user->updateHealth();
		add_animation(new EnergyAnimation(EnergyType::BURN, user));
		if (user->property("HEALTH") <= 0) {
			user->property("ALIVE") = 0;
			add_animation(new DeathAnimation(user));
			effect_string = user->name + " died to burn damage.";
			return true;
		}
	}
	return false;
}

bool check_freeze(Object* user) {
	if (user->property("FROZEN") == 1) {
		user->property("FREEZE_COUNTDOWN")--;
		if (user->property("FREEZE_COUNTDOWN") == 0) {
			effect_string = user->name + " was frozen and could not move.";
			user->property("FREEZE_COUNTDOWN") = 3;
			add_animation(new EnergyAnimation(EnergyType::FREEZE, user));
			return true;
		}
	}
	return false;
}

void attack_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	int damage = user->property("POWER");
	action_string = user->name + " attacked " + target->name + " for " + std::to_string(calc_damage(damage, target)) + " damage.";
	add_animation(new MoveAnimation(user, target));
	attack(damage, target);
}

void defend_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	action_string = user->name + " defended " + target->name + ".";
	target->property("DEFENDED") = 1;
	user->property("DEFENDING") = 1;
}

void freeze_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	if (target->property("FROZEN") == 0) {
		add_animation(new EnergyAnimation(EnergyType::FREEZE, target));
		target->property("FROZEN") = 1;
		target->property("FREEZE_COUNTDOWN") = 3;
		action_string = user->name + " froze " + target->name + ".";
	} else {
		action_string = target->name + " is already frozen.";
	}
}

void burn_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	action_string = user->name + " burned " + target->name + ".";
	add_animation(new EnergyAnimation(EnergyType::BURN, target));
	target->property("BURNED") = 1;
}

void heal_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		return;
	}
	action_string = user->name + " healed " + target->name + ".";
	add_animation(new EnergyAnimation(EnergyType::HEAL, target));
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
	effect_string = target->name + " has " + std::to_string(target->property("HEALTH")) + " health.";
}

void shoot_function(Compiler* compiler, Object* user, Object* target) {
	if (check_burn(user) || check_freeze(user)) {
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		return;
	}
	
	if (user->property("ARROWS") > 0) {
		add_animation(new ShootAnimation(target));
		attack(user->property("POWER"), target);
		user->property("ARROWS")--;
		action_string = user->name + " shot " + target->name + " for " + std::to_string(user->property("POWER")) + " damage, and has " + std::to_string(user->property("ARROWS")) + " arrows remaining.";
	} else {
		action_string = user->name + " tried to shoot " + target->name + ", but was out of arrows!";
	}
}

void destroy_function(Compiler* compiler, Object* user, Object* target) {
	if (user->team == Team::TEAM_PLAYER) {
		for (size_t i = 0; i < compiler->enemies.size(); i++) {
			attack(50, compiler->enemies[i]);
			compiler->enemies[i]->updateHealth();
		}
	} else if (user->team == Team::TEAM_ENEMY) {
		for (size_t i = 0; i < compiler->players.size(); i++) {
			attack(50, compiler->players[i]);
			compiler->players[i]->updateHealth();
		}
	}
}

std::string& get_action_string() { return action_string; }
std::string& get_effect_string() { return effect_string; }