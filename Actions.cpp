#include "Actions.hpp"

int calc_damage(int damage, Object* target) {
	return (int)std::round(damage * (100 - target->property("DEFENSE")) / 100.);
}

void attack(int damage, Object* target) {
	target->property("HEALTH") -= calc_damage(damage, target);
	if (target->property("HEALTH") <= 0) {
		target->property("ALIVE") = 0;
		add_animation(new DeathAnimation(target));
	}
}

bool check_burn(Object* user) {
	if (user->property("BURNED") == 1) {
		user->property("HEALTH") -= 10;
		user->updateHealth();
		if (user->property("HEALTH") <= 0) {
			user->property("ALIVE") = 0;
			add_animation(new DeathAnimation(user));
			return true;
		} else {
			add_animation(new EnergyAnimation(EnergyType::BURN, user));
		}
	}
	return false;
}

bool check_freeze(Object* user) {
	if (user->property("FROZEN") == 1) {
		user->property("FREEZE_COUNTDOWN")--;
		if (user->property("FREEZE_COUNTDOWN") == 0 && user->property("ALIVE") != 0) {
			user->property("FREEZE_COUNTDOWN") = 3;
			add_animation(new EnergyAnimation(EnergyType::FREEZE, user));
			return true;
		}
	}
	return false;
}

void attack_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	int damage = user->property("POWER");
	add_animation(new MoveAnimation(user, target));
	attack(damage, target);
	*result = true;
}

void gunner_attack_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	int damage = user->property("POWER");
	add_animation(new BoltAnimation(user, target));
	attack(damage, target);
	*result = true;
}

void freeze_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	if (target->property("FROZEN") == 0) {
		add_animation(new EnergyAnimation(EnergyType::FREEZE, target));
		target->property("FROZEN") = 1;
		target->property("FREEZE_COUNTDOWN") = 3;
		*result = true;
	} else {
		*result = false;
	}
}

void burn_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	if (target->name == "FLAMMY") {
		*result = false;
		return;
	}
	if (target->property("BURNED") == 0) {
		add_animation(new EnergyAnimation(EnergyType::BURN, target));
		target->property("BURNED") = 1;
		*result = true;
	} else {
		*result = false;
	}
}

void heal_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new EnergyAnimation(EnergyType::HEAL, target));
	if (target->property("HEALTH_MAX") - target->property("HEALTH") < 20) {
		target->property("HEALTH") = target->property("HEALTH_MAX");
	} else {
		target->property("HEALTH") += 20;
	}
	*result = true;
}

void full_heal_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new EnergyAnimation(EnergyType::HEAL, target));
	target->property("HEALTH") = target->property("HEALTH_MAX");
	*result = true;
}

void burn_heal_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new EnergyAnimation(EnergyType::HEAL, target));
	if (target->property("BURNED")) {
		target->property("BURNED") = 0;
		*result = true;
	} else {
		*result = false;
	}
}

void shoot_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	
	if (user->property("ARROWS") > 0) {
		add_animation(new ShootAnimation(target));
		attack(user->property("POWER"), target);
		user->property("ARROWS")--;
		*result = true;
	} else {
		*result = false;
	}
}

void shockwave_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new WaveAnimation(user, compiler));
	if (user->team == Team::TEAM_PLAYER) {
		for (size_t i = 0; i < compiler->enemies.size(); i++) {
			compiler->enemies[i]->property("HEALTH") = 10;
		}
	} else if (user->team == Team::TEAM_ENEMY) {
		for (size_t i = 0; i < compiler->players.size(); i++) {
			compiler->players[i]->property("HEALTH") = 10;
		}
	}
	*result = true;
}

void kill_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0 || target->property("ALIVE") == 0 || user == target) {
		*result = false;
		return;
	}
	target->property("HEALTH") = 0;
	target->property("ALIVE") = 0;
	add_animation(new MoveAnimation(user, target));
	add_animation(new DeathAnimation(target));
	*result = true;
}

void annihilate_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new WaveAnimation(user, compiler));
	if (user->team == Team::TEAM_PLAYER) {
		for (size_t i = 0; i < compiler->enemies.size(); i++) {
			compiler->enemies[i]->property("HEALTH") = 0;
			compiler->enemies[i]->property("ALIVE") = 0;
			add_animation(new DeathAnimation(compiler->enemies[i]));
		}
	} else if (user->team == Team::TEAM_ENEMY) {
		for (size_t i = 0; i < compiler->players.size(); i++) {
			compiler->players[i]->property("HEALTH") = 0;
			compiler->players[i]->property("ALIVE") = 0;
			add_animation(new DeathAnimation(compiler->players[i]));
		}
	}
	*result = true;
}

void destroy_function(Compiler* compiler, Object* user, Object* target, bool* result) {
	if (check_burn(user) || check_freeze(user)) {
		*result = false;
		return;
	}
	if (user->property("ALIVE") == 0) {
		*result = false;
		return;
	}
	add_animation(new WaveAnimation(user, compiler));
	if (user->team == Team::TEAM_PLAYER) {
		for (size_t i = 0; i < compiler->enemies.size(); i++) {
			attack(50, compiler->enemies[i]);
		}
	} else if (user->team == Team::TEAM_ENEMY) {
		for (size_t i = 0; i < compiler->players.size(); i++) {
			attack(50, compiler->players[i]);
		}
	}
	*result = true;
}