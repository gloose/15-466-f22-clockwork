#include "Animation.hpp"

Scene::Transform* heal_transform;
Scene::Transform* freeze_transform;
Scene::Transform* burn_transform;
Scene::Transform* arrow_transform;

void end(size_t my_id);

std::vector<Animation*> active_animations;

size_t animation_id = 0;

float turn_time = 2.0f;

float turn_duration() {
	return turn_time;
}

void add_animation(Animation* animation) {
	active_animations.push_back(animation);
}

void update_animations(float time) {
	for (auto* animation : active_animations) {
		animation->update(time);
	}
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

void Animation::update(float update_time) {
	assert(false && "Animation::update must be overriden by child class.");
}

// Uncomment when objects have a transform field
MoveAnimation::MoveAnimation(Object* source, Object* target) {
	// start_position = source->transform->position;
	// target_position = target->transform->position;
	start_position = glm::vec3(0, 0, 0);
	target_position = glm::vec3(1, 1, 1);
	type = AnimationType::MOVE;
	id = animation_id++;
	// transform = source->transform;
	transform = nullptr;
	elapsed_time = 0.0f;
}

DeathAnimation::DeathAnimation(Object* victim) {
	// start_position = victim->transform->position;
	start_position = glm::vec3(0, 0, 0);
	type = AnimationType::DEATH;
	id = animation_id++;
	// transform = victim->transform;
	transform = nullptr;
	elapsed_time = 0.0f;
}

EnergyAnimation::EnergyAnimation(EnergyType nrg, Object* target) {
	// start_position = target->transform->position;
	start_position = glm::vec3(0, 0, 0);
	type = AnimationType::ENERGY;
	id = animation_id++;
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

void MoveAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time <= turn_time / 2.0f) {
		transform->position = start_position + (target_position - start_position) * (elapsed_time / (turn_time / 2.0f));
	} else if (elapsed_time < turn_time) {
		transform->position = target_position + (start_position - target_position) * (elapsed_time - (turn_time / 2.0f)) / (turn_time / 2.0f);
	} else {
		transform->position = start_position;
		end(id);
	}
}

// Note, define a constant to be some downward translation that will move a character offscreen. I'll call it 1 unit for now.
void DeathAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time < turn_time) {
		transform->position -= glm::vec3(1.0f, 1.0f, 1.0f) * (elapsed_time / turn_time);
	} else {
		end(id);
	}
}

// Note, define a constant to be some scale that will make an energy ball approximately the size of a character. I'll call it 2 for now.
void EnergyAnimation::update(float update_time) {
	elapsed_time += update_time;
	if (elapsed_time <= turn_time / 2.0f) {
		transform->scale = glm::vec3(2.0f, 2.0f, 2.0f) * (elapsed_time / (turn_time / 2.0f));
	} else if (elapsed_time < turn_time) {
		transform->scale = glm::vec3(2.0f, 2.0f, 2.0f) - glm::vec3(2.0f, 2.0f, 2.0f) * (elapsed_time - (turn_time / 2.0f)) / (turn_time / 2.0f);
	} else {
		transform->position = glm::vec3(0, 0, -1);
		end(id);
	}
}

void end(size_t my_id) {
	for (auto iter = active_animations.begin(); iter != active_animations.end(); iter++) {
		if ((*iter)->id == my_id) {
			active_animations.erase(iter);
		}
	}
}