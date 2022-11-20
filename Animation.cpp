#include "Animation.hpp"

Scene::Transform* heal_transform;
Scene::Transform* freeze_transform;
Scene::Transform* burn_transform;
Scene::Transform* arrow_transform;
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

void init_sounds() {
	attack_sample = new Sound::Sample(data_path("Sounds/Attack.wav"));
	freeze_sample = new Sound::Sample(data_path("Sounds/Freeze.wav"));
	burn_sample = new Sound::Sample(data_path("Sounds/Burn.wav"));
	arrow_sample = new Sound::Sample(data_path("Sounds/Arrow.wav"));
	heal_sample = new Sound::Sample(data_path("Sounds/Heal.wav"));
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

void register_ranger_object(Object* o) {
	ranger_object = o;
}

void reset_energy() {
	heal_transform->position = offscreen_position();
	freeze_transform->position = offscreen_position();
	burn_transform->position = offscreen_position();
	arrow_transform->position = ranger_object->transform->position + arrow_offset;
}

bool Animation::update(float update_time) {
	assert(false && "Animation::update must be overriden by child class.");
	return false;
}

MoveAnimation::MoveAnimation(Object* source, Object* target) {
	start_position = source->transform->position;
	target_position = target->transform->position;
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
	health_target = target;
}

DeathAnimation::DeathAnimation(Object* victim) {
	start_position = victim->transform->position;
	type = AnimationType::DEATH;
	id = animation_id++;
	transform = victim->transform;
	elapsed_time = 0.0f;
}

EnergyAnimation::EnergyAnimation(EnergyType nrg, Object* target) {
	start_position = target->transform->position;
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

// Note, define a constant to be some downward translation that will move a character offscreen. I'll call it 0.5 units for now.
bool DeathAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time < (turn_length / 2.0f)) {
		return true;
	} else if ((turn_length / 2.0f) <= elapsed_time && elapsed_time < turn_length) {
		transform->position -= 0.5f * (elapsed_time / (turn_length / 2.0f));
		return true;
	} else {
		return false;
	}
}

// Note, define a constant to be some scale that will make an energy ball approximately the size of a character. I'll call it 2 for now.
bool EnergyAnimation::update(float update_time) {
	elapsed_time += update_time;
	transform->position = energy_target->transform->position;
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