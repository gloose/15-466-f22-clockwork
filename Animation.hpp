#pragma once

#include "Object.hpp"
#include "Compiler.hpp"
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
void register_wave_transform(Scene::Transform* t);
void register_ranger_object(Object* o);
void reset_energy();

void init_sounds();

enum AnimationType {
	MOVE,
	DEATH,
	ENERGY,
	SHOOT,
	WAVE
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
	Object* health_target;
	bool update(float update_time);
	glm::vec3 target_position;
};

struct DeathAnimation : Animation {
	DeathAnimation(Object* victim);
	bool update(float update_time);
};

struct EnergyAnimation : Animation {
	EnergyAnimation(EnergyType nrg, Object *target);
	Object* energy_target;
	EnergyType energy_type;
	bool update(float update_time);
};

struct ShootAnimation : MoveAnimation {
	ShootAnimation(Object* target);
	Object* health_target;
	bool update(float update_time);
};

struct WaveAnimation : Animation {
	WaveAnimation(Object* target, Compiler* input_compiler);
	Object* wave_target;
	bool wave_hit;
	Compiler* compiler;
	bool update(float update_time);
};

void update_animations(float time);

void add_animation(Animation* animation);

void clear_animations();

float turn_duration();

glm::vec3 offscreen_position();

static glm::vec3 arrow_offset = glm::vec3(0.94f, 0.f, 0.056f);

#endif