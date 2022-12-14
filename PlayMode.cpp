#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "gl_compile_program.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>

#include <random>
#include <sstream>
#include <iomanip>

Load< PlayMode::PPUTileProgram > tile_program(LoadTagEarly); //will 'new PPUTileProgram()' by default
Load< PlayMode::PPUDataStream > data_stream(LoadTagDefault);

GLuint character_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > character_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("characters.pnct"));
	character_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > character_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("characters.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){});
});

PlayMode::PlayMode() : scene(*character_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	camera->aspect = float(worldbox_size.x) / float(worldbox_size.y);

	// Initialize matrix converting world coordinates to screen coordinates
	glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
	glm::mat4 screen_to_clip = glm::mat4(
		glm::vec4(2.0f / worldbox_size.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 2.0f / worldbox_size.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-1.0f - worldbox_pos.x * 2.f / worldbox_size.x, -1.0f - worldbox_pos.y * 2.f / worldbox_size.y, 0.0f, 1.0f)
	);
	glm::mat4 clip_to_screen = glm::inverse(screen_to_clip);
	world_to_screen = clip_to_screen * world_to_clip;

	// Set up text rendering
	// Adapted from Harfbuzz example linked on assignment page
	// This font was obtained from https://fonts.google.com/specimen/Roboto+Mono
	// See the license in dist/RobotoMono/LICENSE.txt
	std::string fontfilestring = data_path("Roboto_Mono/static/RobotoMono-Regular.ttf");
	const char* fontfile = fontfilestring.c_str();

	// Initialize FreeType and create FreeType font face.
	if (FT_Init_FreeType(&ft_library))
		abort();
	if (FT_New_Face(ft_library, fontfile, 0, &ft_face))
		abort();
	if (FT_Set_Char_Size(ft_face, font_size * 64, font_size * 64, 0, 0))
		abort();

	// Create hb-ft font.
	hb_font = hb_ft_font_create(ft_face, NULL);

	// Determine a fixed size and baseline for all character tiles
	for (size_t i = min_char; i <= max_char; i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(ft_face, (char)i);
		FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);

		uint32_t w = ft_face->glyph->bitmap.width + ft_face->glyph->bitmap_left;

		int top = ft_face->glyph->bitmap_top;
		int bottom = ft_face->glyph->bitmap.rows - top;

		if (w > char_width) {
			char_width = w;
		}
		if (top > (int)char_top) {
			char_top = top;
		}
		if (bottom > (int)char_bottom) {
			char_bottom = bottom;
		}
	}
	char_height = char_top + char_bottom;

	//interpret tiles and build a 1 x num_chars color texture (adapated from PPU466)
	std::vector<glm::u8vec4> data;
	data.resize(num_chars * char_width * char_height);
	for (uint32_t i = 0; i < num_chars; i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(ft_face, (char)(min_char + i));
		FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);

		//location of tile in the texture:
		uint32_t ox = i * char_width;

		//copy tile indices into texture:
		for (int y = 0; y < (int)char_height; y++) {
			for (int x = 0; x < (int)char_width; x++) {
				int bitmap_x = x - ft_face->glyph->bitmap_left;
				int bitmap_baseline = ft_face->glyph->bitmap_top;
				int from_baseline = y - char_bottom;
				int bitmap_y = bitmap_baseline - from_baseline;

				if (bitmap_x >= 0 && bitmap_x < (int)ft_face->glyph->bitmap.width && bitmap_y >= 0 && bitmap_y < (int)ft_face->glyph->bitmap.rows) {
					data[ox + x + (char_width * num_chars) * y] = glm::u8vec4(0xff, 0xff, 0xff, ft_face->glyph->bitmap.buffer[bitmap_x + ft_face->glyph->bitmap.width * bitmap_y]);
				}
				else {
					data[ox + x + (char_width * num_chars) * y] = glm::u8vec4(0, 0, 0, 0);
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, data_stream->tile_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, char_width * num_chars, char_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	turn_time = 0.0f;
	player_done = true;
	enemy_done = true;
	turn_done = true;
	level_won = false;
	level_lost = false;
	turn = Turn::PLAYER;
	current_level = -1;
	player_units.clear();
	enemy_units.clear();
	create_levels();
	init_compiler();
	energyTransforms();
	init_sounds();
	dungeon_scene = makeObject("DUNGEON", "dungeon");
	cave_scene = makeObject("CAVE", "cave");
	forest_scene = makeObject("FOREST", "forest");
	scene_position = dungeon_scene->transform->position;
	offscreen_scene_position = cave_scene->transform->position;
	next_level();
	compile_failed = false;
	execution_result = ExecutionResult::NONE;
	
	lshift.pressed = false;
	rshift.pressed = false;
	//TODO: begining of the ambient sample
	ambient_sample = new Sound::Sample(data_path("Sounds/ambient.wav"));
}

Object* PlayMode::makeObject(std::string name, std::string model_name, Team team) {
	Object* obj = new Object(name, team);
	
	if (!model_name.empty()) {
		for (auto& transform : scene.transforms) {
			Scene::Transform* new_transform = new Scene::Transform();
			new_transform->name = name + ":" + transform.name;
			new_transform->position = transform.position;
			new_transform->rotation = transform.rotation;
			new_transform->scale = transform.scale;
			new_transform->parent = transform.parent;

			if (transform.name == model_name) {
				obj->transform = new_transform;
				obj->floor_height = obj->transform->position.z;
				obj->start_rotation = new_transform->rotation;
			}
			
			if (transform.name == model_name || transform.name.rfind(model_name + "-", 0) == 0) {
				scene.drawables.emplace_back(new_transform);
				setMesh(&scene.drawables.back(), transform.name);
				obj->drawables.emplace(transform.name, &scene.drawables.back());
			}
		}
		
		for (auto& drawable : obj->drawables) {
			Scene::Transform* transform = drawable.second->transform;
			
			if (transform->name != name + ":" + model_name) {
				if (transform->parent != nullptr && obj->drawables.find(transform->parent->name) != obj->drawables.end()) {
					transform->parent = obj->drawables[transform->parent->name]->transform;
				} else {
					std::cout << "Warning: " << transform->name << " is not a child of " << model_name << std::endl;
				}
			}
		}
	}
	return obj;
}

void PlayMode::energyTransforms() {
	for (auto& transform : scene.transforms) {
		if (transform.name == "heal") {
			register_heal_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = offscreen_position();
		} else if (transform.name == "arrow") {
			register_arrow_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = ranger->transform->position + arrow_offset;
		} else if (transform.name == "fire") {
			register_burn_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = offscreen_position();
		} else if (transform.name == "ice") {
			register_freeze_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = offscreen_position();
		} else if (transform.name == "shockwave") {
			register_wave_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = offscreen_position();
		} else if (transform.name == "bolt") {
			register_bolt_transform(&transform);
			scene.drawables.emplace_back(&transform);
			setMesh(&scene.drawables.back(), transform.name);
			transform.position = offscreen_position();
		}
	}
}

PlayMode::~PlayMode() {
}

void PlayMode::setMesh(Scene::Drawable* drawable, std::string mesh_name) {
	// Assign the mesh with the given name to the given drawable
	Mesh const& mesh = character_meshes->lookup(mesh_name);
	drawable->pipeline = lit_color_texture_program_pipeline;
	drawable->pipeline.vao = character_meshes_for_lit_color_texture_program;
	drawable->pipeline.type = mesh.type;
	drawable->pipeline.start = mesh.start;
	drawable->pipeline.count = mesh.count;
}

void PlayMode::create_levels() {
	level_guidance.push_back("An enemy approaches! Use \"brawler.attack(enemy1)\" to attack him with the brawler! Press shift + enter to submit your code.");
	level_guidance.push_back("Another enemy! This one can't be hurt by the brawler...but you also have a caster. With the same syntax, tell the \"caster\" to \"burn\" \"enemy2\". It will take damage whenever it moves.");
	level_guidance.push_back("Uh oh, enemy3 will survive a hit... After typing the line to have the brawler attack, press enter to move to the next line. Then have the brawler attack enemy3 again. Press shift + enter to submit both lines.");
	level_guidance.push_back("Enemy4 has a powerful attack coming up! The caster can also \"freeze\" enemies, making them unable to move every third turn. Freeze enemy4 and then attack him five times with the brawler. If you get tired of typing, just enter the first few letters of a word and let autocompletion take care of the rest!");
	level_guidance.push_back("Enemy5 will take three hits, and he does a lot of damage! If you just attack him, you'll lose. After the brawler attacks once, use the \"healer\" to \"heal\" the \"brawler\". Then have the brawler finish him off.");
	level_guidance.push_back("Your last unit is a ranger, who can attack faster than the brawler but has limited ammo! Try having the \"ranger\" \"shoot\" enemy6 twice before he has a chance to attack!");
	level_guidance.push_back("Enemy7 has a lot of health. It would take a lot of lines to beat him... You can use loops! Type \"while (true)\" and hit enter, have the brawler attack enemy7, and then type \"end\" below the last line to end the loop. While loops are extremely useful, but be careful: all condition checks, like the one on the WHILE line, take 1/8 of a turn to execute. You can hold CTRL to speed up the fight, or press ESC to end it.");
	level_guidance.push_back("You can also check properties. Try shooting enemy8 \"while (ranger.arrows > 0)\", and then use the brawler afterwards. Try typing just 'RANGER.' (or 'ENEMY8.', 'BRAWLER.', etc.) and check out the full property list in the info box; see if you can guess how many arrows and attacks it will take to score a kill!");
	level_guidance.push_back("If statements work the same way. Try checking \"if (brawler.health < 100)\" before healing him, then repeatedly attack enemy9 (100 is the brawler's maximum health, but this is not true of all units; have a look at the property window). Remember the \"end\"! You can also chain conditions with \"IF (this)\" \"AND (that)\", with the AND on a new line. OR works too.");
	level_guidance.push_back("Alright, time to test everything you've learned! Enemy10 is tough, but you can do it!");
	level_guidance.push_back("Gunners are very powerful! Be careful of Vrop's attacks.");
	level_guidance.push_back("Speedsters like Grum can attack multiple times in a row! Make sure your code is efficient. If you want to know just how fast Grum's attacks are, type 'GRUM.' to check his action info!");
	level_guidance.push_back("Tanks take a lot of damage. Be patient and make sure your code is robust enough to last a while against Yormun.");
	level_guidance.push_back("Not all monsters are the same. Gunner VropVrop is even more powerful!");
	level_guidance.push_back("You can face multiple enemies at once! Make sure your code handles both Fargoth and Rupol.");
	level_guidance.push_back("Great job. Now how about a different pair of enemies?");
	level_guidance.push_back("You're doing great! Keep it up!");
	level_guidance.push_back("You can also face multiple enemies of the same type. You may need a nap in a war against two tanks... Or you can use the ctrl skip. If that's still too slow for you, hold both left and right control for super speed!");
	level_guidance.push_back("If two tanks was too easy for you, how about three speedsters?");
	level_guidance.push_back("To finish off this set of 10 levels, beat one of each enemy!");
	level_guidance.push_back("The scientist has gotten smarter, so the next ten levels are puzzles! You'll have to think carefully about your solution... Shockwave will bring your whole team down to 10 health.");
	level_guidance.push_back("The scientist has developed an enemy with the capability to strike a critical point and kill a robot in one hit! Get rid of it quickly.");
	level_guidance.push_back("Now he's learned from you, and the units can heal each other. Don't let them stay at full health, or destroy_all will deal massive damage to all your units!");
	level_guidance.push_back("He's combined his kill and destroy ideas, and Garthon can defeat everybody with a single attack!");
	level_guidance.push_back("The sceintist has determined that the ranger and healer are your most valuable units. What will you do without them?");
	level_guidance.push_back("It has been discovered that the tanks are vulernable to a burn strategy. Not this one!");
	level_guidance.push_back("He's figured out that you like to deal a lot of damage early and has deployed some countermeasures...");
	level_guidance.push_back("Uh oh, they've figured out how to heal burns!");
	level_guidance.push_back("The enemies can burn you now! You're really getting a taste of your own medicine.");
	level_guidance.push_back("You'll need to use all of your units wisely to beat this last one...");

	level_enemy_code.push_back("EnemyCode/enemy1.txt");
	level_enemy_code.push_back("EnemyCode/enemy2.txt");
	level_enemy_code.push_back("EnemyCode/enemy3.txt");
	level_enemy_code.push_back("EnemyCode/enemy4.txt");
	level_enemy_code.push_back("EnemyCode/enemy5.txt");
	level_enemy_code.push_back("EnemyCode/enemy6.txt");
	level_enemy_code.push_back("EnemyCode/enemy7.txt");
	level_enemy_code.push_back("EnemyCode/enemy8.txt");
	level_enemy_code.push_back("EnemyCode/enemy9.txt");
	level_enemy_code.push_back("EnemyCode/enemy10.txt");
	level_enemy_code.push_back("EnemyCode/enemy11.txt");
	level_enemy_code.push_back("EnemyCode/enemy12.txt");
	level_enemy_code.push_back("EnemyCode/enemy13.txt");
	level_enemy_code.push_back("EnemyCode/enemy14.txt");
	level_enemy_code.push_back("EnemyCode/enemy15.txt");
	level_enemy_code.push_back("EnemyCode/enemy16.txt");
	level_enemy_code.push_back("EnemyCode/enemy17.txt");
	level_enemy_code.push_back("EnemyCode/enemy18.txt");
	level_enemy_code.push_back("EnemyCode/enemy19.txt");
	level_enemy_code.push_back("EnemyCode/enemy20.txt");
	level_enemy_code.push_back("EnemyCode/enemy21.txt");
	level_enemy_code.push_back("EnemyCode/enemy22.txt");
	level_enemy_code.push_back("EnemyCode/enemy23.txt");
	level_enemy_code.push_back("EnemyCode/enemy24.txt");
	level_enemy_code.push_back("EnemyCode/enemy25.txt");
	level_enemy_code.push_back("EnemyCode/enemy26.txt");
	level_enemy_code.push_back("EnemyCode/enemy27.txt");
	level_enemy_code.push_back("EnemyCode/enemy28.txt");
	level_enemy_code.push_back("EnemyCode/enemy29.txt");
	level_enemy_code.push_back("EnemyCode/enemy30.txt");
}

void PlayMode::init_compiler() {
	brawler = makeObject("BRAWLER", "warrior", Team::TEAM_PLAYER);
	brawler->start_position = glm::vec2(-6.f, -6.f);
	brawler->addAction("ATTACK", attack_function, turn_duration());
	brawler->addProperty("HEALTH_MAX", 100);
	brawler->addProperty("HEALTH", 100);
	brawler->addProperty("DEFENSE", 0);
	brawler->addProperty("ALIVE", 1);
	brawler->addProperty("POWER", 15);

	caster = makeObject("CASTER", "caster", Team::TEAM_PLAYER);
	caster->start_position = glm::vec2(-6.f, 6.f);
	caster->addAction("FREEZE", freeze_function, turn_duration() * 1.5f);
	caster->addAction("BURN", burn_function, turn_duration() * 1.5f);
	caster->addProperty("HEALTH_MAX", 60);
	caster->addProperty("HEALTH", 60);
	caster->addProperty("DEFENSE", 0);
	caster->addProperty("ALIVE", 1);

	ranger = makeObject("RANGER", "ranger", Team::TEAM_PLAYER);
	ranger->start_position = glm::vec2(-6.f, 2.f);
	register_ranger_object(ranger);
	ranger->addAction("SHOOT", shoot_function, turn_duration() * 0.5f);
	ranger->addProperty("HEALTH_MAX", 60);
	ranger->addProperty("HEALTH", 60);
	ranger->addProperty("DEFENSE", 0);
	ranger->addProperty("ALIVE", 1);
	ranger->addProperty("ARROWS", 8);
	ranger->addProperty("POWER", 20);

	healer = makeObject("HEALER", "healer", Team::TEAM_PLAYER);
	healer->start_position = glm::vec2(-6.f, -2.f);
	healer->addAction("HEAL", heal_function, turn_duration());
	healer->addProperty("HEALTH_MAX", 80);
	healer->addProperty("HEALTH", 80);
	healer->addProperty("DEFENSE", 0);
	healer->addProperty("ALIVE", 1);

	Object* enemy1 = makeObject("ENEMY1", "monster", Team::TEAM_ENEMY);
	enemy1->start_position = glm::vec2(6.f, 0.f);
	enemy1->addAction("ATTACK", attack_function, turn_duration());
	enemy1->addProperty("HEALTH_MAX", 15);
	enemy1->addProperty("HEALTH", 15);
	enemy1->addProperty("DEFENSE", 0);
	enemy1->addProperty("ALIVE", 1);
	enemy1->addProperty("POWER", 0);

	Object* enemy2 = makeObject("ENEMY2", "monster", Team::TEAM_ENEMY);
	enemy2->start_position = enemy1->start_position;
	enemy2->addAction("ATTACK", attack_function, turn_duration());
	enemy2->addProperty("HEALTH_MAX", 10);
	enemy2->addProperty("HEALTH", 10);
	enemy2->addProperty("DEFENSE", 100);
	enemy2->addProperty("ALIVE", 1);
	enemy2->addProperty("POWER", 10);

	Object* enemy3 = makeObject("ENEMY3", "monster", Team::TEAM_ENEMY);
	enemy3->start_position = enemy1->start_position;
	enemy3->addAction("ATTACK", attack_function, turn_duration());
	enemy3->addProperty("HEALTH_MAX", 30);
	enemy3->addProperty("HEALTH", 30);
	enemy3->addProperty("DEFENSE", 0);
	enemy3->addProperty("ALIVE", 1);
	enemy3->addProperty("POWER", 10);

	Object* enemy4 = makeObject("ENEMY4", "monster", Team::TEAM_ENEMY);
	enemy4->start_position = enemy1->start_position;
	enemy4->addAction("ATTACK", attack_function, turn_duration());
	enemy4->addProperty("HEALTH_MAX", 75);
	enemy4->addProperty("HEALTH", 75);
	enemy4->addProperty("DEFENSE", 0);
	enemy4->addProperty("ALIVE", 1);
	enemy4->addProperty("POWER", 200);

	Object* enemy5 = makeObject("ENEMY5", "monster", Team::TEAM_ENEMY);
	enemy5->start_position = enemy1->start_position;
	enemy5->addAction("ATTACK", attack_function, turn_duration());
	enemy5->addProperty("HEALTH_MAX", 45);
	enemy5->addProperty("HEALTH", 45);
	enemy5->addProperty("DEFENSE", 0);
	enemy5->addProperty("ALIVE", 1);
	enemy5->addProperty("POWER", 50);

	Object* enemy6 = makeObject("ENEMY6", "monster", Team::TEAM_ENEMY);
	enemy6->start_position = enemy1->start_position;
	enemy6->addAction("ATTACK", attack_function, turn_duration());
	enemy6->addProperty("HEALTH_MAX", 40);
	enemy6->addProperty("HEALTH", 40);
	enemy6->addProperty("DEFENSE", 0);
	enemy6->addProperty("ALIVE", 1);
	enemy6->addProperty("POWER", 100);

	Object* enemy7 = makeObject("ENEMY7", "monster", Team::TEAM_ENEMY);
	enemy7->start_position = enemy1->start_position;
	enemy7->addAction("ATTACK", attack_function, turn_duration());
	enemy7->addProperty("HEALTH_MAX", 200);
	enemy7->addProperty("HEALTH", 200);
	enemy7->addProperty("DEFENSE", 0);
	enemy7->addProperty("ALIVE", 1);
	enemy7->addProperty("POWER", 10);

	Object* enemy8 = makeObject("ENEMY8", "monster", Team::TEAM_ENEMY);
	enemy8->start_position = enemy1->start_position;
	enemy8->addAction("ATTACK", attack_function, turn_duration());
	enemy8->addProperty("HEALTH_MAX", 220);
	enemy8->addProperty("HEALTH", 175);
	enemy8->addProperty("DEFENSE", 0);
	enemy8->addProperty("ALIVE", 1);
	enemy8->addProperty("POWER", 10);

	Object* enemy9 = makeObject("ENEMY9", "monster", Team::TEAM_ENEMY);
	enemy9->start_position = enemy1->start_position;
	enemy9->addAction("ATTACK", attack_function, turn_duration());
	enemy9->addProperty("HEALTH_MAX", 90);
	enemy9->addProperty("HEALTH", 90);
	enemy9->addProperty("DEFENSE", 0);
	enemy9->addProperty("ALIVE", 1);
	enemy9->addProperty("POWER", 50);

	Object* enemy10 = makeObject("ENEMY10", "monster", Team::TEAM_ENEMY);
	enemy10->start_position = enemy1->start_position;
	enemy10->addAction("ATTACK", attack_function, turn_duration());
	enemy10->addProperty("HEALTH_MAX", 100);
	enemy10->addProperty("HEALTH", 100);
	enemy10->addProperty("DEFENSE", 0);
	enemy10->addProperty("ALIVE", 1);
	enemy10->addProperty("POWER", 30);

	Object* vrop = makeObject("VROP", "gunner", Team::TEAM_ENEMY);
	vrop->start_position = enemy1->start_position;
	vrop->addAction("ATTACK", gunner_attack_function, turn_duration());
	vrop->addProperty("HEALTH_MAX", 100);
	vrop->addProperty("HEALTH", 100);
	vrop->addProperty("DEFENSE", 0);
	vrop->addProperty("ALIVE", 1);
	vrop->addProperty("POWER", 50);

	Object* grum = makeObject("GRUM", "speedster", Team::TEAM_ENEMY);
	grum->start_position = enemy1->start_position;
	grum->addAction("ATTACK", attack_function, 0.25f * turn_duration());
	grum->addProperty("HEALTH_MAX", 80);
	grum->addProperty("HEALTH", 80);
	grum->addProperty("DEFENSE", 0);
	grum->addProperty("ALIVE", 1);
	grum->addProperty("POWER", 20);

	Object* yormun = makeObject("YORMUN", "tank", Team::TEAM_ENEMY);
	yormun->start_position = enemy1->start_position;
	yormun->addAction("ATTACK", attack_function, 1.5f * turn_duration());
	yormun->addProperty("HEALTH_MAX", 200);
	yormun->addProperty("HEALTH", 200);
	yormun->addProperty("DEFENSE", 0);
	yormun->addProperty("ALIVE", 1);
	yormun->addProperty("POWER", 30);

	Object* vropvrop = makeObject("VROPVROP", "gunner", Team::TEAM_ENEMY);
	vropvrop->start_position = enemy1->start_position;
	vropvrop->addAction("ATTACK", gunner_attack_function, turn_duration());
	vropvrop->addProperty("HEALTH_MAX", 100);
	vropvrop->addProperty("HEALTH", 100);
	vropvrop->addProperty("DEFENSE", 0);
	vropvrop->addProperty("ALIVE", 1);
	vropvrop->addProperty("POWER", 60);

	Object *fargoth = makeObject("FARGOTH", "tank", Team::TEAM_ENEMY);
	fargoth->start_position = glm::vec2(6.f, -3.f);
	fargoth->addAction("ATTACK", attack_function, turn_duration());
	fargoth->addProperty("HEALTH_MAX", 225);
	fargoth->addProperty("HEALTH", 225);
	fargoth->addProperty("DEFENSE", 0);
	fargoth->addProperty("ALIVE", 1);
	fargoth->addProperty("POWER", 20);

	Object *rupol = makeObject("RUPOL", "gunner", Team::TEAM_ENEMY);
	rupol->start_position = glm::vec2(6.f, 3.f);
	rupol->addAction("ATTACK", gunner_attack_function, turn_duration());
	rupol->addProperty("HEALTH_MAX", 150);
	rupol->addProperty("HEALTH", 150);
	rupol->addProperty("DEFENSE", 0);
	rupol->addProperty("ALIVE", 1);
	rupol->addProperty("POWER", 30);

	Object* blurok = makeObject("BLUROK", "speedster", Team::TEAM_ENEMY);
	blurok->start_position = fargoth->start_position;
	blurok->addAction("ATTACK", attack_function, 0.25f * turn_duration());
	blurok->addProperty("HEALTH_MAX", 100);
	blurok->addProperty("HEALTH", 100);
	blurok->addProperty("DEFENSE", 0);
	blurok->addProperty("ALIVE", 1);
	blurok->addProperty("POWER", 10);

	Object* qerbi = makeObject("QERBI", "tank", Team::TEAM_ENEMY);
	qerbi->start_position = rupol->start_position;
	qerbi->addAction("ATTACK", attack_function, turn_duration());
	qerbi->addProperty("HEALTH_MAX", 200);
	qerbi->addProperty("HEALTH", 200);
	qerbi->addProperty("DEFENSE", 0);
	qerbi->addProperty("ALIVE", 1);
	qerbi->addProperty("POWER", 30);

	Object* norver = makeObject("NORVER", "speedster", Team::TEAM_ENEMY);
	norver->start_position = fargoth->start_position;
	norver->addAction("ATTACK", attack_function, 0.5f * turn_duration());
	norver->addProperty("HEALTH_MAX", 120);
	norver->addProperty("HEALTH", 120);
	norver->addProperty("DEFENSE", 0);
	norver->addProperty("ALIVE", 1);
	norver->addProperty("POWER", 15);

	Object* almo = makeObject("ALMO", "gunner", Team::TEAM_ENEMY);
	almo->start_position = rupol->start_position;
	almo->addAction("ATTACK", gunner_attack_function, turn_duration());
	almo->addProperty("HEALTH_MAX", 130);
	almo->addProperty("HEALTH", 130);
	almo->addProperty("DEFENSE", 0);
	almo->addProperty("ALIVE", 1);
	almo->addProperty("POWER", 40);

	Object* harky = makeObject("HARKY", "tank", Team::TEAM_ENEMY);
	harky->start_position = fargoth->start_position;
	harky->addAction("ATTACK", attack_function, turn_duration());
	harky->addProperty("HEALTH_MAX", 250);
	harky->addProperty("HEALTH", 250);
	harky->addProperty("DEFENSE", 0);
	harky->addProperty("ALIVE", 1);
	harky->addProperty("POWER", 15);

	Object* marky = makeObject("MARKY", "tank", Team::TEAM_ENEMY);
	marky->start_position = rupol->start_position;
	marky->addAction("ATTACK", attack_function, turn_duration());
	marky->addProperty("HEALTH_MAX", 250);
	marky->addProperty("HEALTH", 250);
	marky->addProperty("DEFENSE", 0);
	marky->addProperty("ALIVE", 1);
	marky->addProperty("POWER", 15);

	Object* boro = makeObject("BORO", "speedster", Team::TEAM_ENEMY);
	boro->start_position = glm::vec2(6.f, -5.f);
	boro->addAction("ATTACK", attack_function, 0.5f * turn_duration());
	boro->addProperty("HEALTH_MAX", 100);
	boro->addProperty("HEALTH", 100);
	boro->addProperty("DEFENSE", 0);
	boro->addProperty("ALIVE", 1);
	boro->addProperty("POWER", 10);

	Object* coro = makeObject("CORO", "speedster", Team::TEAM_ENEMY);
	coro->start_position = glm::vec2(6.f, 0.f);
	coro->addAction("ATTACK", attack_function, 0.5f * turn_duration());
	coro->addProperty("HEALTH_MAX", 100);
	coro->addProperty("HEALTH", 100);
	coro->addProperty("DEFENSE", 0);
	coro->addProperty("ALIVE", 1);
	coro->addProperty("POWER", 10);

	Object* zoro = makeObject("ZORO", "speedster", Team::TEAM_ENEMY);
	zoro->start_position = glm::vec2(6.f, 5.f);
	zoro->addAction("ATTACK", attack_function, 0.5f * turn_duration());
	zoro->addProperty("HEALTH_MAX", 100);
	zoro->addProperty("HEALTH", 100);
	zoro->addProperty("DEFENSE", 0);
	zoro->addProperty("ALIVE", 1);
	zoro->addProperty("POWER", 10);

	Object* poryo = makeObject("PORYO", "speedster", Team::TEAM_ENEMY);
	poryo->start_position = glm::vec2(6.f, -5.f);
	poryo->addAction("ATTACK", attack_function, 0.5f * turn_duration());
	poryo->addProperty("HEALTH_MAX", 100);
	poryo->addProperty("HEALTH", 100);
	poryo->addProperty("DEFENSE", 0);
	poryo->addProperty("ALIVE", 1);
	poryo->addProperty("POWER", 10);

	Object* therfu = makeObject("THERFU", "gunner", Team::TEAM_ENEMY);
	therfu->start_position = glm::vec2(6.f, -2.f);
	therfu->addAction("ATTACK", gunner_attack_function, turn_duration());
	therfu->addProperty("HEALTH_MAX", 120);
	therfu->addProperty("HEALTH", 120);
	therfu->addProperty("DEFENSE", 0);
	therfu->addProperty("ALIVE", 1);
	therfu->addProperty("POWER", 30);

	Object* wurmp = makeObject("WURMP", "tank", Team::TEAM_ENEMY);
	wurmp->start_position = glm::vec2(6.f, 5.f);
	wurmp->addAction("ATTACK", attack_function, 1.5f * turn_duration());
	wurmp->addProperty("HEALTH_MAX", 200);
	wurmp->addProperty("HEALTH", 200);
	wurmp->addProperty("DEFENSE", 0);
	wurmp->addProperty("ALIVE", 1);
	wurmp->addProperty("POWER", 20);

	Object* bardor = makeObject("BARDOR", "tank", Team::TEAM_ENEMY);
	bardor->start_position = enemy1->start_position;
	bardor->addAction("ATTACK", attack_function, turn_duration());
	bardor->addAction("SHOCKWAVE", shockwave_function, turn_duration(), false);
	bardor->addProperty("HEALTH_MAX", 200);
	bardor->addProperty("HEALTH", 200);
	bardor->addProperty("DEFENSE", 90);
	bardor->addProperty("ALIVE", 1);
	bardor->addProperty("POWER", 20);

	Object* girof = makeObject("GIROF", "gunner", Team::TEAM_ENEMY);
	girof->start_position = enemy1->start_position;
	girof->addAction("KILL", kill_function, turn_duration());
	girof->addProperty("HEALTH_MAX", 80);
	girof->addProperty("HEALTH", 80);
	girof->addProperty("DEFENSE", 0);
	girof->addProperty("ALIVE", 1);
	girof->addProperty("POWER", 1000000);

	Object* vernie = makeObject("VERNIE", "speedster", Team::TEAM_ENEMY);
	vernie->start_position = fargoth->start_position;
	vernie->addAction("HEAL", heal_function, turn_duration());
	vernie->addAction("DESTROY_ALL", destroy_function, turn_duration(), false);
	vernie->addProperty("HEALTH_MAX", 180);
	vernie->addProperty("HEALTH", 180);
	vernie->addProperty("DEFENSE", 0);
	vernie->addProperty("ALIVE", 1);
	vernie->addProperty("POWER", 50);

	Object* purgen = makeObject("PURGEN", "speedster", Team::TEAM_ENEMY);
	purgen->start_position = rupol->start_position;
	purgen->addAction("HEAL", heal_function, turn_duration());
	purgen->addAction("DESTROY_ALL", destroy_function, turn_duration(), false);
	purgen->addProperty("HEALTH_MAX", 180);
	purgen->addProperty("HEALTH", 180);
	purgen->addProperty("DEFENSE", 0);
	purgen->addProperty("ALIVE", 1);
	purgen->addProperty("POWER", 50);

	Object* vurly = makeObject("VURLY", "speedster", Team::TEAM_ENEMY);
	vurly->start_position = fargoth->start_position;
	vurly->addAction("ATTACK", attack_function, turn_duration());
	vurly->addProperty("HEALTH_MAX", 200);
	vurly->addProperty("HEALTH", 200);
	vurly->addProperty("DEFENSE", 0);
	vurly->addProperty("ALIVE", 1);
	vurly->addProperty("POWER", 35);

	Object* garthon = makeObject("GARTHON", "gunner", Team::TEAM_ENEMY);
	garthon->start_position = rupol->start_position;
	garthon->addAction("ANNIHILATE", annihilate_function, 2.0f * turn_duration(), false);
	garthon->addProperty("HEALTH_MAX", 150);
	garthon->addProperty("HEALTH", 150);
	garthon->addProperty("DEFENSE", 0);
	garthon->addProperty("ALIVE", 1);
	garthon->addProperty("POWER", 1000000);

	Object* kerqul = makeObject("KERQUL", "gunner", Team::TEAM_ENEMY);
	kerqul->start_position = enemy1->start_position;
	kerqul->addAction("KILL", kill_function, turn_duration());
	kerqul->addProperty("HEALTH_MAX", 100);
	kerqul->addProperty("HEALTH", 100);
	kerqul->addProperty("DEFENSE", 0);
	kerqul->addProperty("ALIVE", 1);
	kerqul->addProperty("POWER", 1000000);

	Object* flammy = makeObject("FLAMMY", "tank", Team::TEAM_ENEMY);
	flammy->start_position = enemy1->start_position;
	flammy->addAction("ATTACK", attack_function, 0.75f * turn_duration());
	flammy->addProperty("HEALTH_MAX", 50);
	flammy->addProperty("HEALTH", 50);
	flammy->addProperty("DEFENSE", 90);
	flammy->addProperty("ALIVE", 1);
	flammy->addProperty("POWER", 10);

	Object* turpin = makeObject("TURPIN", "speedster", Team::TEAM_ENEMY);
	turpin->start_position = enemy1->start_position;
	turpin->addAction("ATTACK", attack_function, turn_duration());
	turpin->addAction("KILL", kill_function, turn_duration());
	turpin->addAction("FULL_HEAL", full_heal_function, turn_duration());
	turpin->addProperty("HEALTH_MAX", 160);
	turpin->addProperty("HEALTH", 160);
	turpin->addProperty("DEFENSE", 0);
	turpin->addProperty("ALIVE", 1);
	turpin->addProperty("POWER", 20);

	Object* rentol = makeObject("RENTOL", "tank", Team::TEAM_ENEMY);
	rentol->start_position = enemy1->start_position;
	rentol->addAction("ATTACK", attack_function, turn_duration());
	rentol->addAction("BURN_HEAL", burn_heal_function, turn_duration());
	rentol->addProperty("HEALTH_MAX", 200);
	rentol->addProperty("HEALTH", 200);
	rentol->addProperty("DEFENSE", 100);
	rentol->addProperty("ALIVE", 1);
	rentol->addProperty("POWER", 20);

	Object* dingo = makeObject("DINGO", "speedster", Team::TEAM_ENEMY);
	dingo->start_position = fargoth->start_position;
	dingo->addAction("ATTACK", attack_function, turn_duration());
	dingo->addAction("BURN", burn_function, turn_duration());
	dingo->addAction("HEAL", heal_function, turn_duration());
	dingo->addProperty("HEALTH_MAX", 100);
	dingo->addProperty("HEALTH", 100);
	dingo->addProperty("DEFENSE", 50);
	dingo->addProperty("ALIVE", 1);
	dingo->addProperty("POWER", 20);

	Object* wingo = makeObject("WINGO", "speedster", Team::TEAM_ENEMY);
	wingo->start_position = rupol->start_position;
	wingo->addAction("ATTACK", attack_function, turn_duration());
	wingo->addAction("BURN", burn_function, turn_duration());
	wingo->addAction("HEAL", heal_function, turn_duration());
	wingo->addProperty("HEALTH_MAX", 100);
	wingo->addProperty("HEALTH", 100);
	wingo->addProperty("DEFENSE", 50);
	wingo->addProperty("ALIVE", 1);
	wingo->addProperty("POWER", 20);

	Object* shrolin = makeObject("SHROLIN", "gunner", Team::TEAM_ENEMY);
	shrolin->start_position = fargoth->start_position;
	shrolin->addAction("ATTACK", gunner_attack_function, turn_duration());
	shrolin->addProperty("HEALTH_MAX", 100);
	shrolin->addProperty("HEALTH", 100);
	shrolin->addProperty("DEFENSE", 0);
	shrolin->addProperty("ALIVE", 1);
	shrolin->addProperty("POWER", 40);

	Object* mingar = makeObject("MINGAR", "tank", Team::TEAM_ENEMY);
	mingar->start_position = rupol->start_position;
	mingar->addAction("ATTACK", attack_function, turn_duration());
	mingar->addProperty("HEALTH_MAX", 200);
	mingar->addProperty("HEALTH", 200);
	mingar->addProperty("DEFENSE", 0);
	mingar->addProperty("ALIVE", 1);
	mingar->addProperty("POWER", 10);

	player_units.push_back(brawler);
	player_units.push_back(caster);
	player_units.push_back(healer);
	player_units.push_back(ranger);

	std::vector<Object *> level1;
	level1.push_back(enemy1);
	std::vector<Object*> level2;
	level2.push_back(enemy2);
	std::vector<Object*> level3;
	level3.push_back(enemy3);
	std::vector<Object*> level4;
	level4.push_back(enemy4);
	std::vector<Object*> level5;
	level5.push_back(enemy5);
	std::vector<Object*> level6;
	level6.push_back(enemy6);
	std::vector<Object*> level7;
	level7.push_back(enemy7);
	std::vector<Object*> level8;
	level8.push_back(enemy8);
	std::vector<Object*> level9;
	level9.push_back(enemy9);
	std::vector<Object*> level10;
	level10.push_back(enemy10);
	std::vector<Object*> level11;
	level11.push_back(vrop);
	std::vector<Object*> level12;
	level12.push_back(grum);
	std::vector<Object*> level13;
	level13.push_back(yormun);
	std::vector<Object*> level14;
	level14.push_back(vropvrop);
	std::vector<Object*> level15;
	level15.push_back(fargoth);
	level15.push_back(rupol);
	std::vector<Object*> level16;
	level16.push_back(blurok);
	level16.push_back(qerbi);
	std::vector<Object*> level17;
	level17.push_back(norver);
	level17.push_back(almo);
	std::vector<Object*> level18;
	level18.push_back(harky);
	level18.push_back(marky);
	std::vector<Object*> level19;
	level19.push_back(boro);
	level19.push_back(coro);
	level19.push_back(zoro);
	std::vector<Object*> level20;
	level20.push_back(poryo);
	level20.push_back(therfu);
	level20.push_back(wurmp);
	std::vector<Object*> level21;
	level21.push_back(bardor);
	std::vector<Object*> level22;
	level22.push_back(girof);
	std::vector<Object*> level23;
	level23.push_back(vernie);
	level23.push_back(purgen);
	std::vector<Object*> level24;
	level24.push_back(vurly);
	level24.push_back(garthon);
	std::vector<Object*> level25;
	level25.push_back(kerqul);
	std::vector<Object*> level26;
	level26.push_back(flammy);
	std::vector<Object*> level27;
	level27.push_back(turpin);
	std::vector<Object*> level28;
	level28.push_back(rentol);
	std::vector<Object*> level29;
	level29.push_back(dingo);
	level29.push_back(wingo);
	std::vector<Object*> level30;
	level30.push_back(shrolin);
	level30.push_back(mingar);

	enemy_units.push_back(level1);
	enemy_units.push_back(level2);
	enemy_units.push_back(level3);
	enemy_units.push_back(level4);
	enemy_units.push_back(level5);
	enemy_units.push_back(level6);
	enemy_units.push_back(level7);
	enemy_units.push_back(level8);
	enemy_units.push_back(level9);
	enemy_units.push_back(level10);
	enemy_units.push_back(level11);
	enemy_units.push_back(level12);
	enemy_units.push_back(level13);
	enemy_units.push_back(level14);
	enemy_units.push_back(level15);
	enemy_units.push_back(level16);
	enemy_units.push_back(level17);
	enemy_units.push_back(level18);
	enemy_units.push_back(level19);
	enemy_units.push_back(level20);
	enemy_units.push_back(level21);
	enemy_units.push_back(level22);
	enemy_units.push_back(level23);
	enemy_units.push_back(level24);
	enemy_units.push_back(level25);
	enemy_units.push_back(level26);
	enemy_units.push_back(level27);
	enemy_units.push_back(level28);
	enemy_units.push_back(level29);
	enemy_units.push_back(level30);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_LCTRL) {
			lctrl.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RCTRL) {
			rctrl.pressed = true;
			return true;
		} else if(evt.key.keysym.sym == SDLK_LSHIFT){
			lshift.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RSHIFT) {
			rshift.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LCTRL) {
			lctrl.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RCTRL) {
			rctrl.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			lshift.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RSHIFT) {
			rshift.pressed = false;
			return true;
		}
	}

	if (evt.type == SDL_KEYDOWN && (lctrl.pressed || rctrl.pressed) && evt.key.keysym.sym == SDLK_m) {
		music_muted = !music_muted;
		if (music_muted) {
			bgm_playing_sample->set_volume(0.f);
		} else {
			bgm_playing_sample->set_volume(1.f);
		}
		return true;
	}

	if (game_end) {
		return false;
	} else if (!game_start) {
		if (evt.type == SDL_KEYDOWN) {
			if (evt.key.keysym.sym == SDLK_RETURN) {
				bgm_playing_sample = loop(*ambient_sample);
				game_start = true;
				return true;
			}
		}
		return false;
	}

	if (!turn_done) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
			player_done = true;
			enemy_done = true;
			level_won = false;
			level_lost = false;
			turn_done = true;
			reset_level();
			clear_animations();
			return true;
		}
		return false;
	}

	if (evt.type == SDL_KEYDOWN) {
		if(evt.key.keysym.sym == SDLK_RETURN) {
			if (lshift.pressed || rshift.pressed) {
				player_exe = player_compiler.compile(text_buffer);
				if (player_exe == nullptr) {
					compile_failed = true;
					return true;
				}
				compile_failed = false;
				player_statement = player_exe->next();
				enemy_exe = enemy_compiler.compile(level_enemy_code[current_level]);
				enemy_statement = enemy_exe->next();
				player_done = false;
				enemy_done = false;
				turn_done = false;
				player_time = turn_duration();
				enemy_time = turn_duration();
			} else {
				if (!autofill()) {
					line_break();
				}
			}
			return true;
		} else if(evt.key.keysym.sym == SDLK_DOWN) {
			move_down();
			return true;
		} else if(evt.key.keysym.sym == SDLK_UP) {
			move_up();
			return true;
		} else if(evt.key.keysym.sym == SDLK_LEFT) {
			if ((lshift.pressed || rshift.pressed) && (lctrl.pressed || rctrl.pressed)) {
				current_level -= 2;
				next_level();
			} else {
				move_left();
			}
			return true;
		} else if(evt.key.keysym.sym == SDLK_RIGHT) {
			if ((lshift.pressed || rshift.pressed) && (lctrl.pressed || rctrl.pressed)) {
				next_level();
			} else {
				move_right();
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			insert("A");
			return true;
		} else if(evt.key.keysym.sym == SDLK_b) {
			insert("B");
			return true;
		} else if(evt.key.keysym.sym == SDLK_c) {
			insert("C");
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			insert("D");
			return true;
		} else if(evt.key.keysym.sym == SDLK_e) {
			insert("E");
			return true;
		} else if(evt.key.keysym.sym == SDLK_f) {
			insert("F");
			return true;
		} else if(evt.key.keysym.sym == SDLK_g) {
			insert("G");
			return true;
		} else if(evt.key.keysym.sym == SDLK_h) {
			insert("H");
			return true;
		} else if(evt.key.keysym.sym == SDLK_i) {
			insert("I");
			return true;
		} else if(evt.key.keysym.sym == SDLK_j) {
			insert("J");
			return true;
		} else if(evt.key.keysym.sym == SDLK_k) {
			insert("K");
			return true;
		} else if(evt.key.keysym.sym == SDLK_l) {
			insert("L");
			return true;
		} else if(evt.key.keysym.sym == SDLK_m) {
			insert("M");
			return true;
		} else if(evt.key.keysym.sym == SDLK_n) {
			insert("N");
			return true;
		} else if(evt.key.keysym.sym == SDLK_o) {
			insert("O");
			return true;
		} else if(evt.key.keysym.sym == SDLK_p) {
			insert("P");
			return true;
		} else if(evt.key.keysym.sym == SDLK_q) {
			insert("Q");
			return true;
		} else if(evt.key.keysym.sym == SDLK_r) {
			insert("R");
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			insert("S");
			return true;
		} else if(evt.key.keysym.sym == SDLK_t) {
			insert("T");
			return true;
		} else if(evt.key.keysym.sym == SDLK_u) {
			insert("U");
			return true;
		} else if(evt.key.keysym.sym == SDLK_v) {
			insert("V");
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			insert("W");
			return true;
		} else if(evt.key.keysym.sym == SDLK_x) {
			insert("X");
			return true;
		} else if(evt.key.keysym.sym == SDLK_y) {
			insert("Y");
			return true;
		} else if(evt.key.keysym.sym == SDLK_z) {
			insert("Z");
			return true;
		} else if(evt.key.keysym.sym == SDLK_DELETE || evt.key.keysym.sym == SDLK_BACKSPACE){
			delete_text();
		} else if(evt.key.keysym.sym == SDLK_0){
			if (lshift.pressed || rshift.pressed) {
				autofill();
				insert(")");
			} else{
				insert("0");
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_1) {
			if (lshift.pressed || rshift.pressed) {
				autofill();
				insert("!");
			} else {
				insert("1");
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_2) {
			insert("2");
			return true;
		} else if (evt.key.keysym.sym == SDLK_3) {
			insert("3");
			return true;
		} else if(evt.key.keysym.sym == SDLK_4) {
			insert("4");
			return true;
		} else if (evt.key.keysym.sym == SDLK_5) {
			insert("5");
			return true;
		} else if (evt.key.keysym.sym == SDLK_6) {
			insert("6");
			return true;
		} else if (evt.key.keysym.sym == SDLK_7) {
			insert("7");
			return true;
		} else if (evt.key.keysym.sym == SDLK_8) {
			insert("8");
			return true;
		} else if (evt.key.keysym.sym == SDLK_9) {
			if (lshift.pressed || rshift.pressed) {
				autofill();
				insert("(");
			} else{
				insert("9");
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			autofill();
			insert(" ");
			return true;
		} else if (evt.key.keysym.sym == SDLK_TAB) {
			if (!autofill()) {
				insert(" ");
				insert(" ");
			}
			return true;
		} else if(evt.key.keysym.sym == SDLK_PERIOD) {
			autofill();
			if (lshift.pressed || rshift.pressed) {
				insert(">");
			} else {
				insert(".");
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_EQUALS) {
			autofill();
			insert("=");
			return true;
		} else if(evt.key.keysym.sym == SDLK_COMMA) {
			if (lshift.pressed || rshift.pressed) {
				autofill();
				insert("<");
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_MINUS) {
			if (lshift.pressed || rshift.pressed) {
				insert("_");
			}
		}
	}

	return false;
}

void PlayMode::execute_player_statement() {
	float time = player_statement->duration;
	execution_line_index = (int)player_statement->line_num;
	enemy_execution_line_index = -1;
	execution_result = ExecutionResult::NONE;
	if (player_time >= time) {
		auto obj = player_units.begin();
		if (player_statement->type == Compiler::ACTION_STATEMENT) {
			Compiler::ActionStatement *action_statement = dynamic_cast<Compiler::ActionStatement *>(player_statement);
			obj = std::find(player_units.begin(), player_units.end(), action_statement->object);
		}
		execution_result = ExecutionResult::FAILURE;
		if (obj != player_units.end() && player_statement->execute()) {
			execution_result = ExecutionResult::SUCCESS;
		}
		bool enemies_alive = false;
		bool players_alive = false;
		for (auto& enemy : enemy_units[current_level]) {
			if (enemy->property("ALIVE")) {
				enemies_alive = true;
				break;
			}
		}
		for (auto& player : player_units) {
			if (player->property("ALIVE")) {
				players_alive = true;
				break;
			}
		}
		if (!players_alive) {
			player_done = true;
			enemy_done = true;
			level_lost = true;
			return;
		}
		if (!enemies_alive) {
			player_done = true;
			enemy_done = true;
			level_won = true;
			return;
		}
		player_statement = player_exe->next();
		if (player_statement == nullptr) {
			player_done = true;
			if (!enemy_done) {
				turn = Turn::ENEMY;
				enemy_time = turn_duration();
			}
		} else {
			player_time -= time;
			if (player_time <= 0.0f) {
				if (!enemy_done) {
					turn = Turn::ENEMY;
					enemy_time = turn_duration();
				} else {
					player_time = turn_duration();
				}
			}
		}
	} else {
		player_statement->duration -= player_time;
		if (!enemy_done) {
			turn = Turn::ENEMY;
			enemy_time = turn_duration();
		} else {
			player_time = turn_duration();
		}
	}
}

void PlayMode::execute_enemy_statement() {
	float time = enemy_statement->duration;
	execution_line_index = -1;
	enemy_execution_line_index = (int)enemy_statement->line_num;
	execution_result = ExecutionResult::NONE;
	if (enemy_time >= time) {
		if (enemy_statement->execute()) {
			execution_result = ExecutionResult::SUCCESS;
		} else {
			execution_result = ExecutionResult::FAILURE;
		}
		bool enemies_alive = false;
		bool players_alive = false;
		for (auto& enemy : enemy_units[current_level]) {
			if (enemy->property("ALIVE")) {
				enemies_alive = true;
				break;
			}
		}
		for (auto& player : player_units) {
			if (player->property("ALIVE")) {
				players_alive = true;
				break;
			}
		}
		if (!players_alive) {
			player_done = true;
			enemy_done = true;
			level_lost = true;
			return;
		}
		if (!enemies_alive) {
			player_done = true;
			enemy_done = true;
			level_won = true;
			return;
		}
		enemy_statement = enemy_exe->next();
		if (enemy_statement == nullptr) {
			enemy_done = true;
			turn = Turn::PLAYER;
			player_time = turn_duration();
		} else {
			enemy_time -= time;
			if (enemy_time <= 0.0f) {
				if (!player_done) {
					turn = Turn::PLAYER;
					player_time = turn_duration();
				} else {
					enemy_time = turn_duration();
				}
			}
		}
	} else {
		enemy_statement->duration -= enemy_time;
		// If both are done, we want to switch control to the player for the next turn
		if (enemy_done || !player_done) {
			turn = Turn::PLAYER;
			player_time = turn_duration();
		} else {
			enemy_time = turn_duration();
		}
	}
}

void PlayMode::take_turn() {
	if (turn == Turn::PLAYER) {
		execute_player_statement();
	} else {
		execute_enemy_statement();
	}
}

void PlayMode::reset_level() {
	turn = Turn::PLAYER;
	for (Object* p : player_units) {
		p->reset();
	}
	for (size_t i = 0; i < enemy_units.size(); i++) {
		for (Object* e : enemy_units[i]) {
			if ((int)i == current_level) {
				e->reset();
			} else {
				e->transform->position = offscreen_position();
			}
		}
	}
	reset_energy();
	clear_animations();
	turn_time = 0.f;
	execution_line_index = -1;
	enemy_execution_line_index = -1;
}

void PlayMode::next_level() {
	current_level++;
	if (current_level < 0) {
		current_level = 0;
	} else if (current_level < 10) {
		dungeon_scene->transform->position = scene_position;
		cave_scene->transform->position = offscreen_scene_position;
		forest_scene->transform->position = offscreen_scene_position;
	} else if (current_level < 20) {
		dungeon_scene->transform->position = offscreen_scene_position;
		cave_scene->transform->position = scene_position;
		forest_scene->transform->position = offscreen_scene_position;
	} else if (current_level < 30) {
		dungeon_scene->transform->position = offscreen_scene_position;
		cave_scene->transform->position = offscreen_scene_position;
		forest_scene->transform->position = scene_position;
	} else if (current_level >= 30) {
		game_end = true;
		current_level = (int)level_guidance.size() - 1;
	}

	// Compiler should recognize only those objects that exist in this level
	player_compiler.clearObjects();
	enemy_compiler.clearObjects();
	/*
	for (Object *u : player_units) {
		player_compiler.addObject(u);
		enemy_compiler.addObject(u);
	}
	*/
	if (current_level >= first_brawler_level) {
		player_compiler.addObject(brawler);
		enemy_compiler.addObject(brawler);
		brawler->start_position = glm::vec2(-6.f, -6.f);
	} else {
		brawler->start_position = glm::vec2(100.f, 0.f);
	}
	if (current_level >= first_caster_level) {
		player_compiler.addObject(caster);
		enemy_compiler.addObject(caster);
		caster->start_position = glm::vec2(-6.f, 6.f);
	} else {
		caster->start_position = glm::vec2(100.f, 0.f);
	}
	if (current_level >= first_healer_level) {
		player_compiler.addObject(healer);
		enemy_compiler.addObject(healer);
		healer->start_position = glm::vec2(-6.f, -2.f);
	} else {
		healer->start_position = glm::vec2(100.f, 0.f);
	}
	if (current_level >= first_ranger_level) {
		player_compiler.addObject(ranger);
		enemy_compiler.addObject(ranger);
		ranger->start_position = glm::vec2(-6.f, 2.f);
	} else {
		ranger->start_position = glm::vec2(100.f, 0.f);
	}

	for (Object* u : enemy_units[current_level]) {
		player_compiler.addObject(u);
		enemy_compiler.addObject(u);
	}

	enemy_text_buffer = Compiler::readFile(level_enemy_code[current_level]);

	reset_level();
	text_buffer.clear();
	text_buffer.push_back("");
	line_index = 0;
	cur_cursor_pos = 0;
}


void PlayMode::update(float elapsed) {
	if (game_end) {
		brawler->transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
		brawler->transform->position = glm::vec3(-6.0f, -3.0f, brawler->floor_height);
		caster->transform->rotation = glm::quat(sqrt(0.5f), 0.0f, 0.0f, sqrt(0.5f));
		caster->transform->position = glm::vec3(-2.0f, -3.0f, caster->floor_height);
		ranger->transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
		ranger->transform->position = glm::vec3(2.0f, -3.0f, ranger->floor_height);
		healer->transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
		healer->transform->position = glm::vec3(6.0f, -3.0f, healer->floor_height);
		for (Object* u : enemy_units[current_level]) {
			u->transform->position = offscreen_position();
		}
		for (auto& transform : scene.transforms) {
			if (transform.name == "arrow") {
				transform.position = offscreen_position();
			}
		}
	}
	if (lctrl.pressed || rctrl.pressed) {
		update_animations(10.0f * elapsed);
	} else {
		update_animations(elapsed);
	}

	if (!turn_done) {
		if (!player_done || !enemy_done) {
			if (turn_time <= 0.0f) {
				turn_time = turn_duration();
				if (turn == Turn::PLAYER && player_statement != nullptr) {
					if (player_time >= player_statement->duration) {
						turn_time = std::min(player_statement->base_duration, turn_duration());
					} else {
						turn_time = 0.5f;
					}
				} else if (turn == Turn::ENEMY && enemy_statement != nullptr) {
					if (enemy_time >= enemy_statement->duration) {
						turn_time = std::min(enemy_statement->base_duration, turn_duration());
					} else {
						turn_time = 0.5f;
					}
				}
				take_turn();
			} else {
				if (lctrl.pressed && rctrl.pressed) {
					turn_time -= elapsed * 100.f;
				} else if (lctrl.pressed || rctrl.pressed) {
					turn_time -= elapsed * 10.f;
				} else {
					turn_time -= elapsed;
				}
			}
		} else {
			if (turn_time <= 0.0f) {
				turn_done = true;
				execution_line_index = -1;
				enemy_execution_line_index = -1;
				if (!level_lost && !level_won) {
					reset_level();
				} else {
					if (level_won) {
						next_level();
						level_won = false;
					} else if (level_lost) {
						reset_level();
						level_lost = false;
					}
				}
			} else {
				if (lctrl.pressed && rctrl.pressed) {
					turn_time -= elapsed * 100.f;
				} else if (lctrl.pressed || rctrl.pressed) {
					turn_time -= elapsed * 10.f;
				} else {
					turn_time -= elapsed;
				}
			}
		}
	}
}

glm::ivec2 PlayMode::drawTextLarge(std::string text, glm::vec2 position, size_t width, int large_font_size, glm::u8vec4 color_large, bool cursor_line_large){
		std::vector< PPUDataStream::Vertex > triangle_strip;

	//helper to put a single tile somewhere on the screen:
	auto draw_tile = [&](glm::ivec2 const& lower_left, uint8_t tile_index, glm::u8vec4 tile_color) {
		//convert tile index to lower-left pixel coordinate in tile image:
		glm::ivec2 tile_coord = glm::ivec2(tile_index * char_width, 0);

		//build a quad as a (very short) triangle strip that starts and ends with degenerate triangles:
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + 0, lower_left.y - char_bottom), glm::ivec2(tile_coord.x + 0, tile_coord.y + 0), tile_color);
		triangle_strip.emplace_back(triangle_strip.back());
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + 0, lower_left.y + char_top), glm::ivec2(tile_coord.x + 0, tile_coord.y + char_height), tile_color);
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + char_width, lower_left.y - char_bottom), glm::ivec2(tile_coord.x + char_width, tile_coord.y + 0), tile_color);
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + char_width, lower_left.y + char_top), glm::ivec2(tile_coord.x + char_width, tile_coord.y + char_height), tile_color);
		triangle_strip.emplace_back(triangle_strip.back());
	};

	const char* text_c_str = text.c_str();
	size_t start_line = 0;
	size_t line_num = 0;

	if (start_line == text.size() && cursor_line_large) {
		drawText("|", position, width);
	}

	glm::ivec2 ret(0, 0);

	while (start_line < text.size()) {
		line_num++;

		// Create hb-buffer and populate.
		hb_buffer_t* hb_buffer;
		hb_buffer = hb_buffer_create();
		hb_buffer_add_utf8(hb_buffer, text_c_str + start_line, -1, 0, -1);
		hb_buffer_guess_segment_properties(hb_buffer);

		// Shape it!
		hb_feature_t feature;
		hb_feature_from_string("-liga", -1, &feature);
		hb_shape(hb_font, hb_buffer, &feature, 1);

		// Get glyph information and positions out of the buffer.
		unsigned int len = hb_buffer_get_length(hb_buffer);
		hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);

		// Draw text
		double current_x = position.x;
		double current_y = position.y - line_num * large_font_size;

		for (size_t i = 0; i < len; i++)
		{
			if (cursor_line_large && i == cur_cursor_pos) {
				drawText("|", glm::vec2(current_x - 5., current_y + large_font_size), width);
			}
			// Line break if next word would overflow
			if (text[start_line + i] == ' ') {
				double cx = current_x + pos[i].x_advance / 64.;
				bool line_break = false;
				for (size_t j = i + 1; j < len; j++) {
					if (text[start_line + j] == ' ') {
						break;
					}
					cx += pos[j].x_advance / 64.;
					if (cx + char_width > position.x + width) {
						line_break = true;
						break;
					}
				}
				if (line_break) {
					start_line = start_line + i + 1;
					break;
				}
			}

			// Draw character
			draw_tile(glm::ivec2((int)(current_x + pos[i].x_offset / 64.), (int)(current_y + pos[i].y_offset / 64.)), (uint8_t)text[start_line + i] - (uint8_t)min_char, color_large);
			
			// Advance position
			current_x += pos[i].x_advance / 64.;
			current_y += pos[i].y_advance / 64.;

			ret.x = std::max(ret.x, (int)(current_x - position.x));
			ret.y = std::max(ret.y, (int)current_y);
			
			// Line break on overflow (may be necessary if there are no spaces)
			if (current_x + char_width > position.x + width || i == len - 1) {
				start_line = start_line + i + 1;
				break;
			}
			
		}
		if (cursor_line_large && cur_cursor_pos == text.size()) {
			drawText("|", glm::vec2(current_x - 5., current_y + large_font_size), width);
		}
	}

	drawVertexArray(GL_TRIANGLE_STRIP, triangle_strip, true);
	
	//return (int)(line_num * font_size);
	return ret;
}

std::vector<hb_glyph_position_t> PlayMode::getGlyphPositions(std::string text, size_t offset) {
	// Create hb-buffer and populate.
	hb_buffer_t* hb_buffer;
	hb_buffer = hb_buffer_create();
	hb_buffer_add_utf8(hb_buffer, text.c_str() + offset, -1, 0, -1);
	hb_buffer_guess_segment_properties(hb_buffer);

	// Shape it!
	hb_feature_t feature;
	hb_feature_from_string("-liga", -1, &feature);
	hb_shape(hb_font, hb_buffer, &feature, 1);

	// Get glyph information and positions out of the buffer.
	unsigned int len = hb_buffer_get_length(hb_buffer);
	hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);

	// Copy the glyph positions into a vector
	std::vector<hb_glyph_position_t> ret;
	for (size_t i = 0; i < len; i++) {
		ret.push_back(pos[i]);
	}

	// Deallocate the harfbuzz buffer now that it is safely copied
	hb_buffer_destroy(hb_buffer);

	return ret;
}


glm::ivec2 PlayMode::getPositionInText(std::string text, glm::vec2 position, size_t index, size_t offset) {
	// Get glyph positions for the given text string
	std::vector<hb_glyph_position_t> pos = getGlyphPositions(text, offset);

	// Increment position until we reach the end of the string, or the given end index
	glm::dvec2 ret = position;
	for (size_t i = 0; i < pos.size() && i < index; i++) {
		ret.x += pos[i].x_advance / 64.;
		ret.y += pos[i].y_advance / 64.;
	}

	// Return the final position
	return ret;
}


glm::ivec2 PlayMode::drawText(std::string text, glm::vec2 position, size_t width, glm::u8vec4 color, bool cursor_line) {
	// Width of 0 means no width limit, just make it ridiculously big
	if (width == 0) {
		width = (size_t)1e6;
	}
	std::vector< PPUDataStream::Vertex > triangle_strip;

	//helper to put a single tile somewhere on the screen:
	auto draw_tile = [&](glm::ivec2 const& lower_left, uint8_t tile_index, glm::u8vec4 tile_color) {
		//convert tile index to lower-left pixel coordinate in tile image:
		glm::ivec2 tile_coord = glm::ivec2(tile_index * char_width, 0);

		//build a quad as a (very short) triangle strip that starts and ends with degenerate triangles:
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + 0, lower_left.y - char_bottom), glm::ivec2(tile_coord.x + 0, tile_coord.y + 0), tile_color);
		triangle_strip.emplace_back(triangle_strip.back());
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + 0, lower_left.y + char_top), glm::ivec2(tile_coord.x + 0, tile_coord.y + char_height), tile_color);
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + char_width, lower_left.y - char_bottom), glm::ivec2(tile_coord.x + char_width, tile_coord.y + 0), tile_color);
		triangle_strip.emplace_back(glm::ivec2(lower_left.x + char_width, lower_left.y + char_top), glm::ivec2(tile_coord.x + char_width, tile_coord.y + char_height), tile_color);
		triangle_strip.emplace_back(triangle_strip.back());
	};

	size_t start_line = 0;
	size_t line_num = 0;

	if (start_line == text.size() && cursor_line) {
		drawText("|", position, width);
	}

	bool do_autofill = false;
	if (cursor_line && !autofill_suggestion.empty() && text.size() - (autofill_word_end - autofill_word_offset) + autofill_suggestion.size() <= max_line_chars) {
		text.erase(autofill_word_offset, autofill_word_end - autofill_word_offset);
		text.insert(autofill_word_offset, autofill_suggestion);
		do_autofill = true;
	}

	glm::ivec2 ret(0, 0);

	while (start_line < text.size()) {
		line_num++;

		std::vector<hb_glyph_position_t> pos = getGlyphPositions(text, start_line);

		// Draw text
		double current_x = position.x;
		double current_y = position.y - line_num * font_size;

		for (size_t i = 0; i < pos.size(); i++)
		{
			if (cursor_line && i == cur_cursor_pos) {
				drawText("|", glm::vec2(current_x - 5., current_y + font_size), width);
			}
			// Line break if next word would overflow
			if (text[start_line + i] == ' ') {
				double cx = current_x + pos[i].x_advance / 64.;
				bool line_break = false;
				for (size_t j = i + 1; j < pos.size(); j++) {
					cx += pos[j].x_advance / 64.;
					if (cx > position.x + width) {
						line_break = true;
						break;
					}
					if (text[start_line + j] == ' ') {
						break;
					}
				}
				if (line_break) {
					start_line = start_line + i + 1;
					break;
				}
			}

			// Draw character
			glm::u8vec4 glyph_color = color;
			if (do_autofill && (int)i >= autofill_word_end && (int)i < autofill_word_offset + (int)autofill_suggestion.size()) {
				glyph_color = glm::u8vec4(glyph_color.r / 2, glyph_color.g / 2, glyph_color.b / 2, glyph_color.a);
			}
			draw_tile(glm::ivec2((int)(current_x + pos[i].x_offset / 64.), (int)(current_y + pos[i].y_offset / 64.)), (uint8_t)text[start_line + i] - (uint8_t)min_char, glyph_color);
			
			// Advance position
			current_x += pos[i].x_advance / 64.;
			current_y += pos[i].y_advance / 64.;

			ret.x = std::max(ret.x, (int)(current_x - position.x));
			ret.y = std::max(ret.y, -(int)(current_y - position.y));
			
			// Line break on overflow (may be necessary if there are no spaces)
			if (current_x + char_width > position.x + width || i == pos.size() - 1) {
				start_line = start_line + i + 1;
				break;
			}
		}
		if (cursor_line && cur_cursor_pos == text.size()) {
			drawText("|", glm::vec2(current_x - 5., current_y + font_size), width);
		}
	}

	drawVertexArray(GL_TRIANGLE_STRIP, triangle_strip, true);
	
	return ret;
}

void PlayMode::move_up(){
	if (line_index > 0) {
		line_index--;
		if (cur_cursor_pos > text_buffer[line_index].size()) {
			cur_cursor_pos = text_buffer[line_index].size();
		}
	}
}

void PlayMode::move_down(){
	if(line_index < text_buffer.size() - 1){
		line_index++;
		if (cur_cursor_pos > text_buffer[line_index].size()) {
			cur_cursor_pos = text_buffer[line_index].size();
		}
	}
}

void PlayMode::move_right(){
	if(cur_cursor_pos < text_buffer[line_index].size()){
		cur_cursor_pos++;
	}
}

void PlayMode::move_left(){
	if(cur_cursor_pos > 0){
		cur_cursor_pos--;
	}
}

void PlayMode::line_break(){
	if (text_buffer.size() < max_lines) {
		text_buffer.insert(text_buffer.begin() + line_index + 1, text_buffer[line_index].substr(cur_cursor_pos, text_buffer[line_index].size() - cur_cursor_pos));
		text_buffer[line_index] = text_buffer[line_index].substr(0, cur_cursor_pos);
		line_index++;
		cur_cursor_pos = 0;
	}
}

void PlayMode::delete_text(){
	if (cur_cursor_pos > 0){
		text_buffer[line_index].erase(cur_cursor_pos - 1, 1);
		cur_cursor_pos = cur_cursor_pos - 1;
	} else if (line_index > 0 && text_buffer[line_index - 1].size() + text_buffer[line_index].size() <= max_line_chars) {
		cur_cursor_pos = text_buffer[line_index - 1].size();
		text_buffer[line_index - 1] += text_buffer[line_index];
		text_buffer.erase(text_buffer.begin() + line_index);
		line_index--;
	}
}

void PlayMode::insert(std::string cur_letter){
	if (text_buffer[line_index].size() < max_line_chars) {
		text_buffer[line_index].insert(cur_cursor_pos, cur_letter);
		cur_cursor_pos++;
	}
}

void PlayMode::render(){
	int x = input_pos.x + text_margin.x;
	int y = input_pos.y + input_size.y + text_margin.y;

	drawText("Your Code", glm::vec2(x, y), 0, glm::u8vec4(0x80, 0x80, 0x80, 0xff));
	y -= font_size;

	glm::u8vec4 pen_color = default_line_color;
	for(size_t i = 0; i < text_buffer.size(); i++){
		if (!turn_done && (int)i == execution_line_index) {
			switch (execution_result) {
			case ExecutionResult::SUCCESS:
				pen_color = execute_success_color;
				break;
			case ExecutionResult::FAILURE:
				pen_color = execute_failure_color;
				break;
			default:
				pen_color = execute_normal_color;
				break;
			}
		} else if (turn_done && i == line_index) {
			pen_color = cur_line_color;
		} else {
			pen_color = default_line_color;
		}
		drawText(text_buffer[i], glm::vec2(x, y - i * font_size), 0, pen_color, i == line_index);
	}
	if (compile_failed) {
		drawText(player_compiler.error_message, glm::ivec2(error_pos.x + text_margin.x, error_pos.y + error_size.y + text_margin.y), error_size.x - 2 * text_margin.x);
	}
	drawText(level_guidance[current_level], prompt_pos + glm::ivec2(0, prompt_size.y) + text_margin, prompt_size.x - 2 * text_margin.x);
}

//TODO: render text end
void PlayMode::drawVertexArray(GLenum mode, const std::vector<PPUDataStream::Vertex>& vertex_array, bool use_texture) {
	// Upload vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, data_stream->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(decltype(vertex_array[0])) * vertex_array.size(), vertex_array.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set up the pipeline:
	// set blending function for output fragments:
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// set the shader programs:
	glUseProgram(tile_program->program);

	// configure attribute streams:
	glBindVertexArray(data_stream->vertex_buffer_for_tile_program);

	// set uniforms for shader programs:
	{ //set matrix to transform [0,ScreenWidth]x[0,ScreenHeight] -> [-1,1]x[-1,1]:
		//NOTE: glm uses column-major matrices:
		glm::mat4 OBJECT_TO_CLIP = glm::mat4(
			glm::vec4(2.0f / ScreenWidth, 0.0f, 0.0f, 0.0f),
			glm::vec4(0.0f, 2.0f / ScreenHeight, 0.0f, 0.0f),
			glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
			glm::vec4(-1.0f - scroll_x * 2.f / ScreenWidth, -1.0f - scroll_y * 2.f / ScreenHeight, 0.0f, 1.0f)
		);
		glUniformMatrix4fv(tile_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(OBJECT_TO_CLIP));
		glUniform1i(tile_program->USE_TEXTURE_bool, (int)use_texture);
	}

	// bind texture units to proper texture objects:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, data_stream->tile_tex);

	//now that the pipeline is configured, trigger drawing of triangle strip:
	glDrawArrays(mode, 0, GLsizei(vertex_array.size()));

	//return state to default:
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	GL_ERRORS();
}


void PlayMode::drawRectangle(glm::ivec2 pos, glm::ivec2 size, glm::u8vec4 color, bool filled) {
	// Find corners of rectangle, counter-clockwise from bottom left, first corner repeated at end
	std::array<glm::ivec2, 5> corners = {
		glm::ivec2(pos.x, pos.y),
		glm::ivec2(pos.x, pos.y + size.y),
		glm::ivec2(pos.x + size.x, pos.y + size.y),
		glm::ivec2(pos.x + size.x, pos.y),
		glm::ivec2(pos.x, pos.y)
	};

	// Switch corners if filled to get it into the format for a triangle strip
	if (filled) {
		glm::ivec2 temp = corners[2];
		corners[2] = corners[3];
		corners[3] = temp;
	}
	
	// Build vertex array
	std::vector<PPUDataStream::Vertex> vertex_array;
	for (size_t i = 0; i < (filled ? 4 : 5); i++) {
		vertex_array.emplace_back(corners[i], glm::ivec2(0, 0), color);
	}

	// Draw vertex array
	drawVertexArray(filled ? GL_TRIANGLE_STRIP : GL_LINE_STRIP, vertex_array, false);

	// Fill in the bottom left corner, which is empty for some reason
	std::vector<PPUDataStream::Vertex> point;
	point.emplace_back(corners[0], glm::ivec2(0, 0), color);
	drawVertexArray(GL_POINTS, point, false);
}

void PlayMode::drawThickRectangleOutline(glm::ivec2 pos, glm::ivec2 size, glm::u8vec4 color, int thickness) {
	thickness--;
	for (int i = -thickness; i <= thickness; i++) {
		drawRectangle(pos + glm::ivec2(i, i), size - 2 * glm::ivec2(i, i), color, false);
	}
}


glm::vec2 PlayMode::worldToScreen(glm::vec3 pos) {
	glm::vec4 screen_pos = world_to_screen * glm::vec4(pos, 1.f);
	screen_pos /= screen_pos.w;
	return glm::vec2(screen_pos.x, screen_pos.y);
}


void PlayMode::drawHealthBar(Object* unit) {
	if (unit->property("health_max") > 0 && unit->health_level > 0) {
		glm::ivec2 health_bar_pos = worldToScreen(unit->transform->position + glm::vec3(0.f, 0.f, 2.5f)) - glm::vec2(health_bar_size.x / 2.f, 0);
		
		int name_width = drawText(unit->name, health_bar_pos + glm::ivec2(0, health_bar_size.y + font_size), health_bar_size.x, glm::u8vec4(0xff, 0xff, 0xff, 0xff)).x;
		drawRectangle(health_bar_pos - glm::ivec2(2, 1), glm::ivec2(name_width + 2, health_bar_size.y + font_size) + glm::ivec2(3, 3), glm::u8vec4(0, 0, 0, 255), true);
		drawText(unit->name, health_bar_pos + glm::ivec2(0, health_bar_size.y + font_size + 2), health_bar_size.x, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		
		drawRectangle(health_bar_pos, health_bar_size, glm::u8vec4(0, 0, 0, 255), true);
		glm::ivec2 filled_size = glm::vec2(health_bar_size.x * unit->health_level, health_bar_size.y);
		drawRectangle(health_bar_pos, filled_size - glm::ivec2(1, 0), glm::u8vec4(0, 255, 0, 255), true);
		if (filled_size.x > 0) {
			drawThickRectangleOutline(health_bar_pos + glm::ivec2(2, 2), filled_size - glm::ivec2(4, 4), glm::u8vec4(0, 128, 0, 255), 2);
		}
		drawThickRectangleOutline(health_bar_pos, health_bar_size, glm::u8vec4(0, 0, 0, 255), 2);
		float segment_width = 10.f / unit->property("health_max") * health_bar_size.x;
		float segment_start = 0;
		while (segment_start < health_bar_size.x) {
			int x1 = (int)segment_start;
			int x2 = (int)(segment_start + segment_width);
			drawRectangle(glm::ivec2(health_bar_pos.x + x1, health_bar_pos.y), glm::ivec2(std::min(x2 - x1, health_bar_size.x - x1), health_bar_size.y), glm::u8vec4(0, 0, 0, 255), false);
			segment_start += segment_width;
		}
	}
}


bool PlayMode::isObject(std::string name) {
	return player_compiler.objects.find(name) != player_compiler.objects.end();
}


Object* PlayMode::getObject(std::string name) {
	auto it = player_compiler.objects.find(name);
	if (it != player_compiler.objects.end()) {
		return it->second;
	}
	return nullptr;
}


bool PlayMode::isPlayer(Object* obj) {
	return std::find(player_units.begin(), player_units.end(), obj) != player_units.end();
}


void PlayMode::updateAutofillSuggestion() {
	// Reset various autofill properties
	autofill_suggestion = "";
	autofill_word_offset = 0;
	autofill_word_end = 0;
	autofill_user = nullptr;

	// Parse the current line into a vector of words
	std::vector<int> offsets;
	Compiler::Line line = Compiler::readLine(text_buffer[line_index], &offsets);
	assert(offsets.size() == line.size());

	// Determine whether this line is a conditional statement (as opposed to an action)
	bool is_condition = line.size() > 0 && (line[0] == "IF" || line[0] == "WHILE" || line[0] == "AND" || line[0] == "OR");

	// Find the index of the word that the cursor is inside or at the end of
	int word_index = -1;
	for (int i = 0; i < (int)line.size(); i++) {
		std::string word = line[i];
		int offset = offsets[i];
		if ((int)cur_cursor_pos > offset && (int)cur_cursor_pos <= offset + (int)word.size()) {
			word_index = i;
			break;
		}
	}

	// Continue if the cursor is inside a word
	if (word_index >= 0) {
		std::string word = line[word_index];
		autofill_word_offset = offsets[word_index];
		autofill_word_end = autofill_word_offset + (int)word.size();

		bool suggestion_is_player = false;

		// Update suggestion if new word starts with word and is shorter than the current suggestion
		auto updateSuggestion = [&](std::string new_word, bool is_player = false) {
			bool starts_with_word = new_word.size() >= word.size() && new_word.substr(0, word.size()) == word;
			bool priority = ((autofill_suggestion.empty())
			              || (new_word.size() < autofill_suggestion.size())
						  || (is_player && !suggestion_is_player))
						  && (is_player || !suggestion_is_player);
			if (starts_with_word && priority) {
				autofill_suggestion = new_word;
				suggestion_is_player = is_player;
			}
		};

		// Find the object whose name comes before the ".", if applicable
		if (word_index >= 2 && line[word_index - 1] == ".") {
			autofill_user = getObject(line[word_index - 2]);
		} else if (word_index >= 1 && word == ".") {
			autofill_user = getObject(line[word_index - 1]);
		}

		// Generate the autofill suggestion
		if (autofill_user) {
			// If we already have an object, attempt to autofill an action or property
			if (is_condition) {
				// For a condition line, attempt to autofill a property
				for (const auto& prop : autofill_user->properties) {
					updateSuggestion(prop.first);
				}
			} else {
				// For an action line, attempt to autofill an action
				for (const auto& action : autofill_user->actions) {
					updateSuggestion(action.first);
				}
			}
		} else {
			// Otherwise, attempt to autofill an object name
			for (const auto& obj : player_compiler.objects) {
				updateSuggestion(obj.first, isPlayer(obj.second));
			}
			if (is_condition) {
				updateSuggestion("TRUE");
				updateSuggestion("FALSE");
			}
			if (word_index == 0) {
				updateSuggestion("IF");
				updateSuggestion("WHILE");
				updateSuggestion("AND");
				updateSuggestion("OR");
				updateSuggestion("END");
			}
		}
	}
}


// Replace the word at the cursor position with the autofill suggestion
bool PlayMode::autofill() {
	if (!autofill_suggestion.empty()) {
		std::string& line = text_buffer[line_index];
		std::string old_line = line;
		line.erase(autofill_word_offset, autofill_word_end - autofill_word_offset);
		line.insert(autofill_word_offset, autofill_suggestion);
		cur_cursor_pos = autofill_word_offset + autofill_suggestion.size();
		return old_line != line;
	}

	return false;
}


void PlayMode::drawObjectInfoBox(Object* obj) {
	// Size of box
	glm::ivec2 size = glm::ivec2(obj_info_box_width, (obj->properties.size() + obj->actions.size() + 3) * font_size + 2 * abs(text_margin.y));

	// Determine whether the object is a player or an enemy
	bool is_player = std::find(player_units.begin(), player_units.end(), obj) != player_units.end();
	
	// Draw box on the right side for players, and the left side for enemies
	glm::ivec2 offset;
	if (is_player) {
		offset = glm::vec2(100, -size.y / 2);
	} else {
		offset = glm::vec2(-100 - size.x, -size.y / 2);
	}

	// Position of lower left corner of box
	glm::ivec2 pos = (glm::ivec2)worldToScreen(obj->transform->position) + offset;
	
	// Draw box
	drawRectangle(pos, size, glm::u8vec4(0x00, 0x00, 0x00, 0xff), true);
	drawRectangle(pos + glm::ivec2(5, 5), size - glm::ivec2(10, 10), glm::u8vec4(0xff, 0xff, 0xff, 0xff), false);

	// Draw triangle pointing to the object
	std::vector<PPUDataStream::Vertex> triangle;
	glm::ivec2 midpoint;
	if (is_player) {
		midpoint = glm::ivec2(pos.x, pos.y + size.y / 2);
		triangle.emplace_back(midpoint - glm::ivec2(20, 0), glm::u8vec4(0x00, 0x00, 0x00, 0xff));
	} else {
		midpoint = glm::ivec2(pos.x + size.x, pos.y + size.y / 2);
		triangle.emplace_back(midpoint + glm::ivec2(20, 0), glm::u8vec4(0x00, 0x00, 0x00, 0xff));
	}
	triangle.emplace_back(midpoint + glm::ivec2(0, 10), glm::u8vec4(0x00, 0x00, 0x00, 0xff));
	triangle.emplace_back(midpoint + glm::ivec2(0, -10), glm::u8vec4(0x00, 0x00, 0x00, 0xff));
	drawVertexArray(GL_TRIANGLES, triangle, false);

	// Position at which to write new lines of text
	glm::ivec2 text_pos = pos + glm::ivec2(0, size.y) + text_margin;

	// Add a line of text to the box, indented if not a header
	auto writeLine = [&](std::string text, bool header = false) {
		text_pos.y -= drawText((header ? "" : "  ") + text, text_pos, obj_info_box_width - 2 * text_margin.x).y;
	};

	// Write actions
	writeLine("ACTION       TURNS", true);
	size_t dur_offset = 16;
	for (auto& action : obj->action_names) {
		std::string action_line = action;
		std::stringstream dur_ss;
		dur_ss << std::setprecision(2) << obj->actions.at(action).duration / turn_duration();
		std::string dur_string = dur_ss.str();
		while(action_line.size() + dur_string.size() < dur_offset) {
			action_line.append(" ");
		}
		action_line.append(dur_string);
		writeLine(action_line);
	}

	// Write properties
	writeLine(" ");
	writeLine("PROPERTY     VALUE", true);
	size_t val_offset = 16;
	for (auto& prop : obj->property_names) {
		std::string prop_line = prop;
		std::string val_string = std::to_string(obj->property(prop));
		while(prop_line.size() + val_string.size() < val_offset) {
			prop_line.append(" ");
		}
		prop_line.append(val_string);
		writeLine(prop_line);
	}
}


void PlayMode::drawEnemyCode() {
	int x = enemy_pos.x + text_margin.x;
	int y = enemy_pos.y + enemy_size.y + text_margin.y;

	drawText("Enemy Code", glm::vec2(x, y), 0, glm::u8vec4(0x80, 0x80, 0x80, 0xff));
	y -= font_size;

	glm::u8vec4 pen_color = default_line_color;
	for(size_t i = 0; i < enemy_text_buffer.size(); i++){
		if (!turn_done && (int)i == enemy_execution_line_index) {
			switch (execution_result) {
			case ExecutionResult::SUCCESS:
				pen_color = execute_success_color;
				break;
			case ExecutionResult::FAILURE:
				pen_color = execute_failure_color;
				break;
			default:
				pen_color = execute_normal_color;
				break;
			}
		} else {
			pen_color = default_line_color;
		}
		drawText(enemy_text_buffer[i], glm::vec2(x, y - i * font_size), 0, pen_color);
	}
}


void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	//camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	//background gets background color:
	glClearColor(
		background_color.r / 255.0f,
		background_color.g / 255.0f,
		background_color.b / 255.0f,
		1.0f
	);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// glEnable(GL_DEPTH_TEST);
	// glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	// glViewport(worldbox_pos.x, worldbox_pos.y, worldbox_size.x, worldbox_size.y);
	// scene.draw(*camera);
	// glViewport(0, 0, drawable_size.x, drawable_size.y);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	if(game_start && (!game_end)){

		glViewport(worldbox_pos.x, worldbox_pos.y, worldbox_size.x, worldbox_size.y);
		scene.draw(*camera);
		glViewport(0, 0, drawable_size.x, drawable_size.y);
		glDisable(GL_DEPTH_TEST);

		drawRectangle(worldbox_pos - glm::ivec2(5, 5), worldbox_size + glm::ivec2(10, 10), glm::u8vec4(255, 255, 255, 255), false);

		for (size_t i = 0; i < player_units.size(); i++) {
			drawHealthBar(player_units[i]);
		}
		for (size_t i = 0; i < enemy_units[current_level].size(); i++) {
			drawHealthBar(enemy_units[current_level][i]);
		}

		drawRectangle(enemy_pos, enemy_size, glm::u8vec4(0, 0, 0, 255), true);
		drawRectangle(enemy_pos + glm::ivec2(5, 5), enemy_size - glm::ivec2(10, 10), glm::u8vec4(255, 255, 255, 255), false);
		drawRectangle(input_pos, input_size, glm::u8vec4(0, 0, 0, 255), true);
		drawRectangle(input_pos + glm::ivec2(5, 5), input_size - glm::ivec2(10, 10), glm::u8vec4(255, 255, 255, 255), false);
		drawRectangle(prompt_pos, prompt_size, glm::u8vec4(0, 0, 0, 255), true);
		drawRectangle(prompt_pos + glm::ivec2(5, 5), prompt_size - glm::ivec2(10, 10), glm::u8vec4(255, 255, 255, 255), false);
		drawRectangle(error_pos, error_size, glm::u8vec4(0, 0, 0, 255), true);
		drawRectangle(error_pos + glm::ivec2(5, 5), error_size - glm::ivec2(10, 10), glm::u8vec4(255, 255, 255, 255), false);

		updateAutofillSuggestion();

		if (autofill_user && turn_done) {
			drawObjectInfoBox(autofill_user);
		}

		render();

		drawEnemyCode();
	}
	else if(game_end && game_start){
		//Draw game start here
		int x = 450;
		int y = 450;
		scene.draw(*camera);
		drawTextLarge("Congratulations! You Beat the Monsters!", glm::ivec2(x,y), 500, 54, default_color, false);
	}
	else if((!game_start) && (!game_end)){
		//Draw game start here
		int x = 600;
		int y = 450;
		scene.draw(*camera);
		//not working cannot set font size
		glm::ivec2 new_pos = drawTextLarge("Code Quest", glm::ivec2(x,y), 500, 54, default_color, false);
		drawText("PRESS ENTER", glm::ivec2(new_pos.x + 500, 100), 500);
	}

	GL_ERRORS();
}




// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Functions for initializing the rendering structs, adapted from PPU466

PlayMode::PPUTileProgram::PPUTileProgram() {
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in ivec2 TileCoord;\n"
		"out vec2 tileCoord;\n"
		"in vec4 Color;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	tileCoord = TileCoord;\n"
		"	color = Color;\n"
		"}\n"
		,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TILE_TABLE;\n"
		"uniform bool USE_TEXTURE;\n"
		"in vec2 tileCoord;\n"
		"out vec4 fragColor;\n"
		"in vec4 color;\n"
		"void main() {\n"
		"if (USE_TEXTURE) {\n"
		"	fragColor = texelFetch(TILE_TABLE, ivec2(tileCoord), 0);\n"
		"} else {\n"
		"	fragColor.a = 1.0;\n"
		"}\n"
		"fragColor.r = color.r;\n"
		"fragColor.g = color.g;\n"
		"fragColor.b = color.b;\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	Position_vec2 = glGetAttribLocation(program, "Position");
	TileCoord_ivec2 = glGetAttribLocation(program, "TileCoord");
	Color_vec4 = glGetAttribLocation(program, "Color");
	//Palette_int = glGetAttribLocation(program, "Palette");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	USE_TEXTURE_bool = glGetUniformLocation(program, "USE_TEXTURE");

	GLuint TILE_TABLE_usampler2D = glGetUniformLocation(program, "TILE_TABLE");
	//GLuint PALETTE_TABLE_sampler2D = glGetUniformLocation(program, "PALETTE_TABLE");

	//bind texture units indices to samplers:
	glUseProgram(program);
	glUniform1i(TILE_TABLE_usampler2D, 0);
	//glUniform1i(PALETTE_TABLE_sampler2D, 1);
	glUseProgram(0);

	GL_ERRORS();
}

PlayMode::PPUTileProgram::~PPUTileProgram() {
	if (program != 0) {
		glDeleteProgram(program);
		program = 0;
	}
}

//PPU data is streamed to the GPU (read: uploaded 'just in time') using a few buffers:
PlayMode::PPUDataStream::PPUDataStream() {

	//vertex_buffer_for_tile_program is a vertex array object that tells the GPU the layout of data in vertex_buffer:
	glGenVertexArrays(1, &vertex_buffer_for_tile_program);
	glBindVertexArray(vertex_buffer_for_tile_program);

	//vertex_buffer will (eventually) hold vertex data for drawing:
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	//Notice how this binding is attaching an integer input to a floating point attribute:
	glVertexAttribPointer(
		tile_program->Position_vec2, //attribute
		2, //size
		GL_INT, //type
		GL_FALSE, //normalized
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, Position) //offset
	);
	glEnableVertexAttribArray(tile_program->Position_vec2);

	//the "I" variant binds to an integer attribute:
	glVertexAttribIPointer(
		tile_program->TileCoord_ivec2, //attribute
		2, //size
		GL_INT, //type
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, TileCoord) //offset
	);
	glEnableVertexAttribArray(tile_program->TileCoord_ivec2);

	// Add color attribute
	glVertexAttribPointer(
		tile_program->Color_vec4, //attribute
		4, //size
		GL_FLOAT, //type
		GL_FALSE, //normalized
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, Color) //offset
	);
	glEnableVertexAttribArray(tile_program->Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glGenTextures(1, &tile_tex);
	glBindTexture(GL_TEXTURE_2D, tile_tex);
	//passing 'nullptr' to TexImage says "allocate memory but don't store anything there":
	// (textures will be uploaded later)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 26, 26, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	//make the texture have sharp pixels when magnified:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//when access past the edge, clamp to the edge:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	GL_ERRORS();
}

PlayMode::PPUDataStream::~PPUDataStream() {
	if (vertex_buffer_for_tile_program != 0) {
		glDeleteVertexArrays(1, &vertex_buffer_for_tile_program);
		vertex_buffer_for_tile_program = 0;
	}
	if (vertex_buffer != 0) {
		glDeleteBuffers(1, &vertex_buffer);
		vertex_buffer = 0;
	}
	if (tile_tex != 0) {
		glDeleteTextures(1, &tile_tex);
		tile_tex = 0;
	}
}
