#include "Object.hpp"
#include "Scene.hpp"
#include <iostream>
#include <random>
#include <vector>
#include "Sound.hpp"
#include "data_path.hpp"
#include <glm/glm.hpp>

#ifndef _ANIMATION_H_
#define _ANIMATION_H_

void register_heal_transform(Scene::Transform* t);
void register_freeze_transform(Scene::Transform* t);
void register_burn_transform(Scene::Transform* t);
void register_arrow_transform(Scene::Transform* t);
void register_archer_object(Object* o);

void init_sounds();

enum AnimationType {
	MOVE,
	DEATH,
	ENERGY,
	SHOOT
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
	bool sound_playing;
	bool update(float update_time);
};

struct MoveAnimation : Animation {
	MoveAnimation(Object* source, Object* target);
	bool update(float update_time);
	glm::vec3 target_position;
};

struct DeathAnimation : Animation {
	DeathAnimation(Object* victim);
	bool update(float update_time);
};

struct EnergyAnimation : Animation {
	EnergyAnimation(EnergyType nrg, Object *target);
	EnergyType energy_type;
	bool update(float update_time);
};

struct ShootAnimation : MoveAnimation {
	ShootAnimation(Object* target);
	bool update(float update_time);
};

void update_animations(float time);

void add_animation(Animation* animation);

float turn_duration();

glm::vec3 offscreen_position();

static glm::vec3 arrow_offset = glm::vec3(0.94f, 0.f, 0.056f);

#endif