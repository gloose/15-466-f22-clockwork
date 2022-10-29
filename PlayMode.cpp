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

#define MAX_LINE_LENGTH 50
#define MAX_LINES 20

Load< PlayMode::PPUTileProgram > tile_program(LoadTagEarly); //will 'new PPUTileProgram()' by default
Load< PlayMode::PPUDataStream > data_stream(LoadTagDefault);

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);


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
	turn = Turn::PLAYER;
	left_shift = false;
	right_shift = false;
	init_compiler();

	code.push_back("");
	code_line = 0;
	line_pos = 0;
	get_action_string() = "";
	get_effect_string() = "";
}

PlayMode::~PlayMode() {
}

void PlayMode::init_compiler() {
	Compiler::Object *warrior = new Compiler::Object("WARRIOR");
	warrior->addAction("ATTACK", attack_function, 1.0f);
	warrior->addAction("DEFEND", defend_function, 1.0f);
	warrior->addProperty("HEALTH_MAX", 100);
	warrior->addProperty("HEALTH", 100);
	warrior->addProperty("DEFENSE", 2);
	warrior->addProperty("ALIVE", 1);

	Compiler::Object *wizard = new Compiler::Object("WIZARD");
	wizard->addAction("FREEZE", freeze_function, 1.5f);
	wizard->addAction("BURN", burn_function, 1.5f);
	wizard->addProperty("HEALTH_MAX", 200);
	wizard->addProperty("HEALTH", 200);
	wizard->addProperty("DEFENSE", 1);
	wizard->addProperty("ALIVE", 1);
	wizard->addProperty("DEFENDED", 0); // Defended by the warrior

	Compiler::Object *archer = new Compiler::Object("ARCHER");
	archer->addAction("SHOOT", shoot_function, 0.5f);
	archer->addProperty("HEALTH_MAX", 50);
	archer->addProperty("HEALTH", 50);
	archer->addProperty("DEFENSE", 1);
	archer->addProperty("ALIVE", 1);
	archer->addProperty("DEFENDED", 0);
	archer->addProperty("ARROWS", 20);

	Compiler::Object *healer = new Compiler::Object("HEALER");
	healer->addAction("HEAL", heal_function, 1.0f);
	healer->addProperty("HEALTH_MAX", 50);
	healer->addProperty("HEALTH", 50);
	healer->addProperty("DEFENSE", 1);
	healer->addProperty("ALIVE", 1);
	healer->addProperty("DEFENDED", 0);

	Compiler::Object *fargoth = new Compiler::Object("FARGOTH");
	fargoth->addAction("ATTACK", attack_function, 1.0f);
	fargoth->addAction("DEFEND", defend_function, 1.0f);
	fargoth->addProperty("HEALTH_MAX", 150);
	fargoth->addProperty("HEALTH", 150);
	fargoth->addProperty("DEFENSE", 1);
	fargoth->addProperty("ALIVE", 1);
	fargoth->addProperty("PRESENT", 1);

	Compiler::Object *rupol = new Compiler::Object("RUPOL");
	rupol->addAction("ATTACK", attack_function, 1.0f);
	rupol->addAction("DEFEND", defend_function, 1.0f);
	rupol->addProperty("HEALTH_MAX", 150);
	rupol->addProperty("HEALTH", 150);
	rupol->addProperty("DEFENSE", 1);
	rupol->addProperty("ALIVE", 1);
	rupol->addProperty("PRESENT", 1);

	player_units.push_back(warrior);
	player_units.push_back(wizard);
	player_units.push_back(healer);
	player_units.push_back(archer);
	
	enemy_units.push_back(fargoth);
	enemy_units.push_back(rupol);

	player_compiler.addObject(warrior);
	enemy_compiler.addObject(warrior);
	player_compiler.addObject(wizard);
	enemy_compiler.addObject(wizard);
	player_compiler.addObject(archer);
	enemy_compiler.addObject(archer);
	player_compiler.addObject(healer);
	enemy_compiler.addObject(healer);
	player_compiler.addObject(fargoth);
	enemy_compiler.addObject(fargoth);
	player_compiler.addObject(rupol);
	enemy_compiler.addObject(rupol);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (!turn_done) {
		return false;
	}

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'A');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_b) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'B');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_c) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'C');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_d) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'D');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_e) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'E');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_f) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'F');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_g) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'G');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_h) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'H');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_i) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'I');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_j) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'J');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_k) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'K');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_l) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'L');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_m) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'M');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_n) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'N');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_o) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'O');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_p) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'P');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_q) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'Q');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_r) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'R');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_s) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'S');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_t) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'T');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_u) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'U');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_v) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'V');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_w) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'W');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_x) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'X');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_y) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'Y');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_z) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, 'Z');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_1) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '1');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_2) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '2');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_3) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '3');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_4) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '4');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_5) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '5');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_6) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '6');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_7) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '7');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_8) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '8');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_9) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				if (left_shift || right_shift) {
					code[code_line].insert(code[code_line].begin() + line_pos, '(');
				} else {
					code[code_line].insert(code[code_line].begin() + line_pos, '9');
				}
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_0) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				if (left_shift || right_shift) {
					code[code_line].insert(code[code_line].begin() + line_pos, ')');
				} else {
					code[code_line].insert(code[code_line].begin() + line_pos, '0');
				}
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_PERIOD) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				if (left_shift || right_shift) {
					code[code_line].insert(code[code_line].begin() + line_pos, '>');
				}
				else {
					code[code_line].insert(code[code_line].begin() + line_pos, '.');
				}
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_COMMA && (left_shift || right_shift)) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '<');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_EQUALS) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, '=');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			if (code[code_line].size() < MAX_LINE_LENGTH) {
				code[code_line].insert(code[code_line].begin() + line_pos, ' ');
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_BACKSPACE) {
			if (line_pos > 0) {
				code[code_line].erase(line_pos - 1, 1);
				line_pos--;
				return true;
			} else if (code_line > 0) {
				code.erase(code.begin() + code_line);
				code_line--;
				line_pos = (int)code[code_line].size();
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			if (line_pos > 0) {
				line_pos--;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			if (line_pos < code[code_line].size()) {
				line_pos++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_UP) {
			if (code_line > 0) {
				code_line--;
				if (line_pos > code[code_line].size()) {
					line_pos = (int)code[code_line].size();
				}
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			if (code_line < code.size() - 1) {
				code_line++;
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_RSHIFT) {
			right_shift = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			left_shift = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			if (!right_shift && !left_shift && code.size() < MAX_LINES && line_pos == code[code_line].size()) {
				code.insert(code.begin() + code_line + 1, "");
				code_line++;
				line_pos = 0;
				return true;
			} else if (left_shift || right_shift) {
				std::cout << "Submitted!\n";
				player_exe = player_compiler.compile(code);
				player_statement = player_exe->next();
				enemy_exe = enemy_compiler.compile("enemy-test.txt");
				enemy_statement = enemy_exe->next();
				player_done = false;
				enemy_done = false;
				turn_done = false;
				return true;
			}
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_RSHIFT) {
			right_shift = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			left_shift = false;
			return true;
		}
	}
	return false;
}

void PlayMode::execute_player_statement(float time_left) {
	float time = player_statement->duration;
	if (time_left >= time) {
		std::cout << "Executing statement.\n";
		player_statement->execute();
		bool enemies_alive = false;
		bool players_alive = false;
		for (auto& enemy : enemy_units) {
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
			get_effect_string() = "All player units have fallen...";
			player_done = true;
			enemy_done = true;
			game_lost = true;
			return;
		}
		if (!enemies_alive) {
			get_effect_string() = "All enemy units have been slain!";
			player_done = true;
			enemy_done = true;
			game_won = true;
			return;
		}
		player_statement = player_exe->next();
		if (player_statement == nullptr) {
			player_done = true;
		} else {
			execute_player_statement(time_left - time);
		}
	} else {
		player_statement->duration -= time_left;
	}
}

void PlayMode::execute_enemy_statement(float time_left) {
	float time = enemy_statement->duration;
	if (time_left >= time) {
		std::cout << "Executing statement.\n";
		enemy_statement->execute();
		bool enemies_alive = false;
		bool players_alive = false;
		for (auto& enemy : enemy_units) {
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
			get_effect_string() = "All player units have fallen...";
			player_done = true;
			enemy_done = true;
			game_lost = true;
			return;
		}
		if (!enemies_alive) {
			get_effect_string() = "All enemy units have been slain!";
			player_done = true;
			enemy_done = true;
			game_won = true;
			return;
		}
		enemy_statement = enemy_exe->next();
		if (enemy_statement == nullptr) {
			enemy_done = true;
		} else {
			execute_enemy_statement(time_left - time);
		}
	} else {
		enemy_statement->duration -= time_left;
	}
}

void PlayMode::take_turn() {
	if (turn == Turn::PLAYER) {
		std::cout << "Player taking turn.\n";
		execute_player_statement(1.0f);
		if (!enemy_done) {
			turn = Turn::ENEMY;
		}
	} else {
		std::cout << "Enemy taking turn.\n";
		execute_enemy_statement(1.0f);
		// If both are done, we want to switch control to the player for the next turn
		if (enemy_done || !player_done) {
			turn = Turn::PLAYER;
		}
	}
}

void PlayMode::update(float elapsed) {
	if (!turn_done) {
		if (!player_done || !enemy_done) {
			if (turn_time <= 0.0f) {
				take_turn();
				turn_time = 1.0f;
			} else {
				turn_time -= elapsed;
			}
		} else {
			if (turn_time <= 0.0f && !game_won && !game_lost) {
				left_shift = false;
				right_shift = false;
				turn_done = true;
				code.clear();
				code.push_back("");
				code_line = 0;
				line_pos = 0;
				get_action_string() = "";
				get_effect_string() = "";
			} else {
				turn_time -= elapsed;
			}
		}
	}

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}


int PlayMode::drawText(std::string text, glm::vec2 position, size_t width, glm::u8vec4 color, bool cursor_line) {
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

	if (start_line == text.size() && cursor_line) {
		drawText("|", position, width);
	}

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
		double current_y = position.y - line_num * font_size;
		for (size_t i = 0; i < len; i++)
		{
			if (cursor_line && i == line_pos) {
				drawText("|", glm::vec2(current_x - 5., current_y + font_size), width);
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
			draw_tile(glm::ivec2((int)(current_x + pos[i].x_offset / 64.), (int)(current_y + pos[i].y_offset / 64.)), (uint8_t)text[start_line + i] - (uint8_t)min_char, color);
			
			// Advance position
			current_x += pos[i].x_advance / 64.;
			current_y += pos[i].y_advance / 64.;
			
			// Line break on overflow (may be necessary if there are no spaces)
			if (current_x + char_width > position.x + width || i == len - 1) {
				start_line = start_line + i + 1;
				break;
			}
			
		}
		if (cursor_line && line_pos == text.size()) {
			drawText("|", glm::vec2(current_x - 5., current_y + font_size), width);
		}
	}

	drawTriangleStrip(triangle_strip);
	
	return (int)(line_num * font_size);
}


void PlayMode::drawTriangleStrip(const std::vector<PPUDataStream::Vertex>& triangle_strip) {
	// Upload vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, data_stream->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(decltype(triangle_strip[0])) * triangle_strip.size(), triangle_strip.data(), GL_STREAM_DRAW);
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
	}

	// bind texture units to proper texture objects:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, data_stream->tile_tex);

	//now that the pipeline is configured, trigger drawing of triangle strip:
	glDrawArrays(GL_TRIANGLE_STRIP, 0, GLsizei(triangle_strip.size()));

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


void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

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

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	// scene.draw(*camera);

	{ //use DrawLines to overlay the cursor:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("l",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xFF, 0xFF, 0xFF, 0xFF));
	}

	// Draw text using proper text rendering
	int x = 20;
	int y = ScreenHeight - 20;
	int w = 500;
	for (size_t i = 0; i < code.size(); i++) {
		drawText(code[i], glm::vec2(x, y - font_size * i), w, glm::u8vec4(0xFF, 0xFF, 0xFF, 0xFF), i == code_line);
	}
	drawText(get_action_string(), glm::vec2(ScreenWidth / 2, 100), w);
	drawText(get_effect_string(), glm::vec2(ScreenWidth / 2, 50), w);

	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
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
		"in vec2 tileCoord;\n"
		"out vec4 fragColor;\n"
		"in vec4 color;\n"
		"void main() {\n"
		"fragColor = texelFetch(TILE_TABLE, ivec2(tileCoord), 0);\n"
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
