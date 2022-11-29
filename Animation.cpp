#include "Animation.hpp"

Scene::Transform* heal_transform;
Scene::Transform* freeze_transform;
Scene::Transform* burn_transform;
Scene::Transform* arrow_transform;
Scene::Transform* wave_transform;
Scene::Transform* bolt_transform;
Object* ranger_object;

void end(size_t my_id);

std::vector<Animation*> active_animations;

size_t animation_id = 0;

float turn_length = 2.0f;

Sound::Sample *attack_sample;
Sound::Sample *freeze_sample;
Sound::Sample *burn_sample;
Sound::Sample *arrow_sample;
Sound::Sample *heal_sample;
Sound::Sample *wave_sample;
Sound::Sample* bolt_sample;

void init_sounds() {
	attack_sample = new Sound::Sample(data_path("Sounds/Attack.wav"));
	freeze_sample = new Sound::Sample(data_path("Sounds/Freeze.wav"));
	burn_sample = new Sound::Sample(data_path("Sounds/Burn.wav"));
	arrow_sample = new Sound::Sample(data_path("Sounds/Arrow.wav"));
	heal_sample = new Sound::Sample(data_path("Sounds/Heal.wav"));
	wave_sample = new Sound::Sample(data_path("Sounds/Shockwave.wav"));
	bolt_sample = new Sound::Sample(data_path("Sounds/Bolt.wav"));
}

float turn_duration() {
	return turn_length;
}

glm::vec3 offscreen_position() {
	return glm::vec3(0.0f, 0.0f, 100.0f);
}

void add_animation(Animation* animation) {
	active_animations.push_back(animation);
}

void update_animations(float time) {
	auto iter = active_animations.begin();
	while (iter != active_animations.end()) {
		Animation* animation = *iter;
		MoveAnimation* move;
		DeathAnimation* death;
		EnergyAnimation* energy;
		ShootAnimation* shoot;
		WaveAnimation* wave;
		BoltAnimation* bolt;
		switch (animation->type) {
		case MOVE:
			move = (MoveAnimation*)animation;
			if (move->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		case DEATH:
			death = (DeathAnimation*)animation;
			if (death->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		case ENERGY:
			energy = (EnergyAnimation*)animation;
			if (energy->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		case SHOOT:
			shoot = (ShootAnimation*)animation;
			if (shoot->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		case WAVE:
			wave = (WaveAnimation*)animation;
			if (wave->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		case BOLT:
			bolt = (BoltAnimation*)animation;
			if (bolt->update(time)) {
				iter++;
			} else {
				iter = active_animations.erase(iter);
			}
			break;
		default:
			iter = active_animations.end();
			break;
		}
	}
}

void clear_animations() {
	active_animations.clear();
}

void register_heal_transform(Scene::Transform *t) {
	heal_transform = t;
}

void register_freeze_transform(Scene::Transform* t) {
	freeze_transform = t;
}

void register_burn_transform(Scene::Transform* t) {
	burn_transform = t;
}

void register_arrow_transform(Scene::Transform* t) {
	arrow_transform = t;
}

void register_wave_transform(Scene::Transform* t) {
	wave_transform = t;
}

void register_bolt_transform(Scene::Transform* t) {
	bolt_transform = t;
}

void register_ranger_object(Object* o) {
	ranger_object = o;
}

void reset_energy() {
	heal_transform->position = offscreen_position();
	freeze_transform->position = offscreen_position();
	burn_transform->position = offscreen_position();
	wave_transform->position = offscreen_position();
	bolt_transform->position = offscreen_position();
	arrow_transform->position = ranger_object->getStartPosition() + arrow_offset;
}

bool Animation::update(float update_time) {
	assert(false && "Animation::update must be overriden by child class.");
	return false;
}

MoveAnimation::MoveAnimation(Object* source, Object* target) {
	start_position = source->getStartPosition();
	target_position = target->getStartPosition();
	type = AnimationType::MOVE;
	id = animation_id++;
	transform = source->transform;
	elapsed_time = 0.0f;
	sound_playing = false;
	health_target = target;
}

ShootAnimation::ShootAnimation(Object* target) : MoveAnimation(ranger_object, target) {
	start_position += arrow_offset;
	transform = arrow_transform;
	// TODO: Some trig magic to rotate the arrow to face the enemy.
	type = AnimationType::SHOOT;
	play(*arrow_sample);
	sound_playing = true;
}

BoltAnimation::BoltAnimation(Object* source, Object* target) : MoveAnimation(source, target) {
	type = AnimationType::BOLT;
	start_position.x -= 1.0f;
	transform = bolt_transform;
	play(*bolt_sample);
	sound_playing = true;
}

DeathAnimation::DeathAnimation(Object* victim) {
	start_position = victim->getStartPosition();
	type = AnimationType::DEATH;
	id = animation_id++;
	transform = victim->transform;
	elapsed_time = 0.0f;
	target = victim;
	glm::quat start_quat = target->start_rotation;
	glm::quat end_quat = start_quat;
	if (transform->name.find("caster") != std::string::npos) {
		end_quat = glm::quat(0.0f, sqrt(0.5f), 0.0f, -sqrt(0.5f));
	} else if (transform->name.find("warrior") != std::string::npos || transform->name.find("healer") != std::string::npos || transform->name.find("ranger") != std::string::npos) {
		if (transform->name.find("ranger") != std::string::npos) {
			arrow_transform->position = offscreen_position();
		}
		end_quat = glm::quat(0.5f, -0.5f, -0.5f, 0.5f);
	} else if (transform->name.find("monster") != std::string::npos) {
		end_quat = glm::quat(0.5f, 0.5f, 0.5f, 0.5f);
	} else if (transform->name.find("gunner") != std::string::npos) {
		end_quat = glm::quat(0.0f, -sqrt(0.5f), 0.0f, -sqrt(0.5f));
	} else if (transform->name.find("speedster") != std::string::npos) {
		end_quat = glm::quat(0.0f, -sqrt(0.5f), 0.0f, -sqrt(0.5f));
	} else if (transform->name.find("tank") != std::string::npos) {
		end_quat = glm::quat(0.5f, -0.5f, 0.5f, -0.5f);
	}
	delta = end_quat - start_quat;
}

EnergyAnimation::EnergyAnimation(EnergyType nrg, Object* target) {
	start_position = target->getStartPosition();
	type = AnimationType::ENERGY;
	id = animation_id++;
	sound_playing = false;
	energy_type = nrg;
	energy_target = target;
	switch (nrg) {
	case HEAL:
		transform = heal_transform;
		break;
	case FREEZE:
		transform = freeze_transform;
		break;
	case BURN:
		transform = burn_transform;
		break;
	default:
		transform = nullptr;
		break;
	}
	transform->position = start_position;
	transform->scale = glm::vec3(0, 0, 0);
	elapsed_time = 0.0f;
}

WaveAnimation::WaveAnimation(Object* target, Compiler *input_compiler) {
	start_position = target->getStartPosition();
	wave_target = target;
	type = AnimationType::WAVE;
	id = animation_id++;
	compiler = input_compiler;
	sound_playing = true;
	wave_hit = false;
	play(*wave_sample);
	transform = wave_transform;
	transform->position = start_position;
	transform->scale = glm::vec3(0, 0, 1.0f);
	elapsed_time = 0.0f;
}

bool MoveAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time <= turn_length / 2.0f) {
		transform->position = start_position + (target_position - start_position) * (elapsed_time / (turn_length / 2.0f));
		return true;
	} else if (elapsed_time < turn_length) {
		if (!sound_playing) {
			health_target->updateHealth();
			play(*attack_sample);
			sound_playing = true;
		}
		transform->position = target_position + (start_position - target_position) * (elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f);
		return true;
	} else {
		transform->position = start_position;
		return false;
	}
}

bool ShootAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time <= turn_length / 2.0f) {
		transform->position = start_position + (target_position - start_position) * (elapsed_time / (turn_length / 2.0f));
		return true;
	} else {
		health_target->updateHealth();
		transform->position = start_position;
		play(*attack_sample);
		return false;
	}
}

bool BoltAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time <= turn_length / 2.0f) {
		transform->position = start_position + (target_position - start_position) * (elapsed_time / (turn_length / 2.0f));
		return true;
	} else {
		health_target->updateHealth();
		transform->position = offscreen_position();
		play(*attack_sample);
		return false;
	}
}

// Note, define a constant to be some downward translation that will move a character offscreen. I'll call it 0.5 units for now.
bool DeathAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time < (turn_length / 2.0f)) {
		return true;
	} else if ((turn_length / 2.0f) <= elapsed_time && elapsed_time < turn_length) {
		transform->position.z = start_position.z - 4.0f * ((elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f));
		transform->rotation = target->start_rotation + delta * ((elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f));
		if (target->team == Team::TEAM_PLAYER) {
			transform->position.x = start_position.x - 2.0f * ((elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f));
		} else if (target->team == Team::TEAM_ENEMY) {
			transform->position.x = start_position.x - 2.0f * ((elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f));
		}
		return true;
	} else {
		transform->position = offscreen_position();
		transform->rotation = target->start_rotation + delta;
		return false;
	}
}

// Note, define a constant to be some scale that will make an energy ball approximately the size of a character. I'll call it 2 for now.
bool EnergyAnimation::update(float update_time) {
	elapsed_time += update_time;
	transform->position = energy_target->getStartPosition();
	if (elapsed_time <= turn_length / 2.0f) {
		transform->scale = glm::vec3(2.0f, 2.0f, 2.0f) * (elapsed_time / (turn_length / 2.0f));
		return true;
	} else if (elapsed_time < turn_length) {
		if (!sound_playing) {
			sound_playing = true;
			switch (energy_type) {
			case HEAL:
				energy_target->updateHealth();
				play(*heal_sample);
				break;
			case FREEZE:
				play(*freeze_sample);
				break;
			case BURN:
				play(*burn_sample);
				break;
			default:
				break;
			}
		}
		transform->scale = glm::vec3(2.0f, 2.0f, 2.0f) - glm::vec3(2.0f, 2.0f, 2.0f) * (elapsed_time - (turn_length / 2.0f)) / (turn_length / 2.0f);
		return true;
	} else {
		transform->position = offscreen_position();
		return false;
	}
}

bool WaveAnimation::update(float update_time) {
	elapsed_time += update_time;
	transform->position = wave_target->getStartPosition();
	if (elapsed_time < turn_length) {
		transform->scale = glm::vec3(25.0f, 25.0f, 10.0f) * (elapsed_time / turn_length);
		if (elapsed_time >= turn_length / 2.0f && !wave_hit) {
			wave_hit = true;
			if (wave_target->team == Team::TEAM_PLAYER) {
				for (size_t i = 0; i < compiler->enemies.size(); i++) {
					compiler->enemies[i]->updateHealth();
				}
			} else if (wave_target->team == Team::TEAM_ENEMY) {
				for (size_t i = 0; i < compiler->players.size(); i++) {
					compiler->players[i]->updateHealth();
				}
			}
		}
		return true;
	} else {
		transform->position = offscreen_position();
		return false;
	}
}