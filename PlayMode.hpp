#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>
#include "Actions.hpp"

#include <vector>
#include <deque>

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
	int font_size = 24;
	static inline glm::u8vec4 default_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	static inline glm::u8vec4 alt_color = glm::u8vec4(0xff, 0xff, 0xc0, 0xff);

	int scroll_x = 0;
	int scroll_y = 0;


	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	glm::vec3 get_leg_tip_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

	// David
	enum Turn {
		PLAYER,
		ENEMY
	} turn;
	float turn_time;
	bool player_done;
	bool enemy_done;
	bool turn_done;
	void take_turn();
	void init_compiler();
	void execute_player_statement(float time_left);
	void execute_enemy_statement(float time_left);
	Compiler player_compiler;
	Compiler enemy_compiler;
	Compiler::Executable *player_exe;
	Compiler::Statement *player_statement;
	Compiler::Executable *enemy_exe;
	Compiler::Statement *enemy_statement;

	// David Text Test
	bool left_shift;
	bool right_shift;
	std::vector<std::string> code;
	int code_line;
	int line_pos;

	// Helper functions
	int drawText(std::string text, glm::vec2 position, size_t width, glm::u8vec4 color = default_color, bool cursor_line = false);
	void drawTriangleStrip(const std::vector<PPUDataStream::Vertex>& triangle_strip);
};
