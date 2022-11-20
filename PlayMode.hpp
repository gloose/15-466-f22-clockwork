#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>
#include "Animation.hpp"
#include "Actions.hpp"
#include "Compiler.hpp"

#include <vector>
#include <deque>
#include <array>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;


	//----- Graphics objects adapted from PPU466 -----

	glm::u8vec3 background_color = glm::u8vec3(0x00, 0x00, 0x00);
	enum : uint32_t {
		ScreenWidth = 1280,
		ScreenHeight = 720
	};

	//In order to implement the PPU466 on modern graphics hardware, a fancy, special purpose tile-drawing shader is used:
	struct PPUTileProgram {
		PPUTileProgram();
		~PPUTileProgram();

		GLuint program = 0;

		//Attribute (per-vertex variable) locations:
		GLuint Position_vec2 = -1U;
		GLuint TileCoord_ivec2 = -1U;
		GLuint Color_vec4 = -1U;

		//Uniform (per-invocation variable) locations:
		GLuint OBJECT_TO_CLIP_mat4 = -1U;
		GLuint USE_TEXTURE_bool = -1U;

		//Textures bindings:
		//TEXTURE0 - the tile table (as a 128x128 R8UI texture)
	};

	//PPU data is streamed to the GPU (read: uploaded 'just in time') using a few buffers:
	struct PPUDataStream {
		PPUDataStream();
		~PPUDataStream();

		//vertex format for convenience:
		struct Vertex {
			Vertex(glm::ivec2 const& Position_, glm::ivec2 const& TileCoord_, glm::u8vec4 Color_)
				: Position(Position_), TileCoord(TileCoord_), Color(glm::vec4(Color_) / 255.f) { }
			Vertex(glm::ivec2 const& Position_, glm::u8vec4 Color_)
				: Position(Position_), TileCoord(glm::ivec2(0, 0)), Color(glm::vec4(Color_) / 255.f) { }
			//I generally make class members lowercase, but I make an exception here because
			// I use uppercase for vertex attributes in shader programs and want to match.
			glm::ivec2 Position;
			glm::ivec2 TileCoord;
			glm::vec4 Color;
		};

		//vertex buffer that will store data stream:
		GLuint vertex_buffer = 0;

		//vertex array object that maps tile program attributes to vertex storage:
		GLuint vertex_buffer_for_tile_program = 0;

		//texture object that will store tile table:
		GLuint tile_tex = 0;
	};


	// Properties of the font used in the game
	FT_Library ft_library;
	FT_Face ft_face;
	hb_font_t* hb_font;
	uint32_t char_top = 1;
	uint32_t char_bottom = 1;
	uint32_t char_width = 1;
	uint32_t char_height = 1;
	uint32_t min_char = 32;
	uint32_t max_char = 126;
	uint32_t num_chars = max_char - min_char + 1;
	int font_size = 16;
	static inline glm::u8vec4 default_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	static inline glm::u8vec4 alt_color = glm::u8vec4(0xff, 0xff, 0xc0, 0xff);
	static inline glm::u8vec4 cur_line_color = glm::u8vec4(0xff, 0xff, 0xc0, 0xff);
	static inline glm::u8vec4 execute_line_color = glm::u8vec4(0xff, 0xff, 0xc0, 0xff);
	static inline glm::u8vec4 default_line_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	int scroll_x = 0;
	int scroll_y = 0;


	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} lshift, rshift, lctrl, rctrl;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	
	//camera:
	Scene::Camera *camera = nullptr;

	// David
	enum Turn {
		PLAYER,
		ENEMY
	} turn;
	float turn_time;
	float player_time;
	float enemy_time;
	bool player_done;
	bool enemy_done;
	bool turn_done;
	void take_turn();
	void init_compiler();
	void create_levels();
	void next_level();
	void reset_level();
	void execute_player_statement();
	void execute_enemy_statement();
	Compiler player_compiler;
	Compiler enemy_compiler;
	Compiler::Executable *player_exe;
	Compiler::Statement *player_statement;
	Compiler::Executable *enemy_exe;
	Compiler::Statement *enemy_statement;
	std::vector<Object*> player_units;
	std::vector<std::vector<Object*>> enemy_units;
	std::vector<std::string> level_enemy_code;
	std::vector<std::string> level_guidance;
	int current_level;
	bool level_lost;
	bool level_won;
	bool compile_failed;

	Object* brawler;
	Object* caster;
	Object* ranger;
	Object* healer;

	glm::ivec2 prompt_pos = glm::ivec2(10, ScreenHeight - 190);
	glm::ivec2 prompt_size = glm::ivec2(400, 180);
	glm::ivec2 input_pos = glm::ivec2(10, 100);
	glm::ivec2 input_size = glm::ivec2(prompt_size.x, prompt_pos.y - input_pos.y - 10);
	glm::ivec2 error_pos = glm::ivec2(10, 10);
	glm::ivec2 error_size = glm::ivec2(prompt_size.x, input_pos.y - 20);
	glm::ivec2 worldbox_pos = glm::ivec2(input_pos.x + input_size.x + 20, 20);
	glm::ivec2 worldbox_size = glm::ivec2(ScreenWidth - (input_pos.x + input_size.x) - 40, ScreenHeight - 40);
	glm::ivec2 text_margin = glm::ivec2(10, -10);

	glm::mat4 world_to_screen;

	glm::ivec2 health_bar_size = glm::vec2(100, 10);

	int obj_info_box_width = 200;

	std::string autofill_suggestion = "";
	int autofill_word_offset = 0;
	int autofill_word_end = 0;
	Object* autofill_user;

	// Helper functions
	glm::ivec2 drawText(std::string text, glm::vec2 position, size_t width, glm::u8vec4 color = default_color, bool cursor_line = false);
	glm::ivec2 drawTextLarge(std::string text, glm::vec2 position, size_t width, int large_font_size, glm::u8vec4 color_large = default_color, bool cursor_line_large = false);
	void drawVertexArray(GLenum mode, const std::vector<PPUDataStream::Vertex>& vertex_array, bool use_texture);
	void setMesh(Scene::Drawable* drawable, std::string mesh);
	Object* makeObject(std::string name, std::string model_name = "");
	void energyTransforms();
	void drawRectangle(glm::ivec2 pos, glm::ivec2 size, glm::u8vec4 color, bool filled);
	void drawThickRectangleOutline(glm::ivec2 pos, glm::ivec2 size, glm::u8vec4 color, int thickness);
	glm::vec2 worldToScreen(glm::vec3 pos);
	void drawHealthBar(Object* unit);
	void updateAutofillSuggestion();
	bool isObject(std::string name);
	Object* getObject(std::string name);
	std::vector<hb_glyph_position_t> getGlyphPositions(std::string text, size_t offset = 0);
	glm::ivec2 getPositionInText(std::string text, glm::vec2 position, size_t index, size_t offset = 0);
	void drawObjectInfoBox(Object* obj);
	bool autofill();
	bool isPlayer(Object* obj);

	//begin of the text rendering
	size_t line_index = 0;
	size_t cur_cursor_pos = 0;
	std::vector< std::string > text_buffer;
	int execution_line_index = -1;
	size_t max_line_length = 400;
	size_t max_line_chars = 40;
	size_t max_lines = 25;
	std::string cur_str;
	void move_up();
	void move_down();
	void move_right();
	void move_left();
	void line_break();
	void delete_text();
	void insert(std::string cur_letter);
	void render();
	bool game_start = false;
	bool game_end = false;

	Scene::Transform *monster = nullptr; 
	Scene::Transform *monster_sclera = nullptr;
	Scene::Transform *monster_tentacle_4 = nullptr;
	Scene::Transform *monster_tentacle_3 = nullptr;
	Scene::Transform *monster_tentacle_2 = nullptr;
	Scene::Transform *monster_tentacle_1 = nullptr;
	Scene::Transform *monster_tentacle_0 = nullptr;
	Scene::Transform *monster_pupil = nullptr;
	//end of the text rendering
};
