#include "Object.hpp"
#include "Scene.hpp"
#include <iostream>
#include <random>
#include <vector>
#include <glm/glm.hpp>

#ifndef _ANIMATION_H_
#define _ANIMATION_H_

void register_heal_transform(Scene::Transform* t);
void register_freeze_transform(Scene::Transform* t);
void register_burn_transform(Scene::Transform* t);
void register_arrow_transform(Scene::Transform* t);

enum AnimationType {
	MOVE,
	DEATH,
	ENERGY
};

enum EnergyType {
	HEAL,
	FREEZE,
	BURN
};

struct Animation {
	float elapsed_time;
	glm::vec3 start_position;
	Scene::Transform* transform;
	AnimationType type;
	size_t id;
	void update(float update_time);
};

struct MoveAnimation : Animation {
	MoveAnimation(Object* source, Object* target);
	void update(float update_time);
	glm::vec3 target_position;
};

struct DeathAnimation : Animation {
	DeathAnimation(Object* victim);
	void update(float update_time);
};

struct EnergyAnimation : Animation {
	EnergyAnimation(EnergyType nrg, Object *target);
	void update(float update_time);
};

void update_animations(float time);

void add_animation(Animation* animation);

float turn_duration();

#endif