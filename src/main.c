//
//  Binocle
//  Copyright(C)2015-2018 Valerio Santinelli
//

#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif
#include "binocle_camera.h"
#include "binocle_color.h"
#include "binocle_game.h"
#include "binocle_sdl.h"
#include "binocle_viewport_adapter.h"
#include "binocle_window.h"
#include <binocle_image.h>
#include <binocle_input.h>
#include <binocle_material.h>
#include <binocle_shader.h>
#include <binocle_sprite.h>
#include <binocle_texture.h>
#include <binocle_audio.h>
#include <binocle_atlas.h>

#define BINOCLE_MATH_IMPL
#include "binocle_bitmapfont.h"
#include "binocle_gd.h"
#include "binocle_log.h"
#include "binocle_math.h"
//#include "sys_config.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#define CUTE_TILED_IMPLEMENTATION
#include "cute_tiled.h"

//#define GAMELOOP 1
#define ATLAS_MAX_SUBTEXTURES 256
#define GRID 32
#define MAX_ELVES 4
#define MAX_SPAWNERS 3
#define MAX_PARTICLES 256
#define WITCH_COOLDOWN 60
#define MAX_COUNTDOWN_VOICE 5
#define MAX_BARRELS_SPAWNERS 10
#define MAX_BARRELS 20

#if defined(WIN32)
#define drand48() (rand() / (RAND_MAX + 1.0))
#define srand48 srand
#endif

typedef enum game_state_t {
  GAME_STATE_MENU,
  GAME_STATE_RUN,
  GAME_STATE_GAMEOVER,
  GAME_STATE_WITCH
} game_state_t;

typedef enum item_kind_t {
  ITEM_KIND_NONE,
  ITEM_KIND_TOY,
  ITEM_KIND_PACKAGE,
  ITEM_KIND_WRAP
} item_kind_t;

struct player_t {
  binocle_sprite sprite;
  kmVec2 pos;
  kmVec2 speed;
  float rot;
  bool dead;
};

struct entity_t {
  binocle_sprite sprite;
  binocle_sprite frozen_sprite;
  kmVec2 pos;
  float rot;
  kmVec2 scale;
  bool dead;
  float dx;
  float dy;
  float xr;
  float yr;
  int cx;
  int cy;
  float frict;
  int last_stable_y;
  bool on_ground;
  bool has_gravity;
  float fall_start_y;
  int dir;
  item_kind_t carried_item_kind;
  struct entity_t *carried_entity;
  float hei;
  bool locked; // needed for hero only
  float lock_cooldown; // needed for hero only
};

struct tile_t {
  int gid;
  binocle_sprite sprite;
};

struct layer_t {
  int tiles_gid[20*15];
};

struct spawner_t {
  item_kind_t item_kind;
  struct entity_t entity; // Used for physical rendering
  float cooldown;
  float cooldown_original;
};

struct witch_t {
  struct entity_t entity;
  float floating_duration;
  float floating_cooldown;
  float sacrifice_duration;
  float sacrifice_cooldown;
  bool sacrifice_done;
  float start_x;
  float start_y;
  float wander_ang;
};

struct particle_t {
  binocle_sprite *sprite;
  kmVec2 pos;
  kmVec2 speed;
  bool alive;
  float cooldown;
  kmVec2 scale;
};

struct countdown_voice_t {
  binocle_sound *sound;
  float cooldown_original;
  float cooldown;
  bool enabled;
};

struct barrel_t {
  struct entity_t entity;
  bool alive;
};

struct barrel_spawner_t {
  float pos_x;
  float pos_y;
  float cooldown;
  int dir;
};

// Window stuff
uint32_t design_width = 640;
uint32_t design_height = 480;

// Binocle internal stuff
binocle_window window;
binocle_input input;
binocle_viewport_adapter adapter;
binocle_camera camera;
binocle_gd gd;

// Fonts
binocle_bitmapfont *font = NULL;
binocle_image font_image;
binocle_texture font_texture;
binocle_material font_material;
binocle_sprite font_sprite;
kmVec2 font_sprite_pos;

// FPS stuff
char fps_buffer[10];

// Test stuff
kmAABB2 testRect;

// Game entities
uint32_t map_width_in_tiles = 20;
uint32_t map_height_in_tiles = 15;
binocle_sprite enemy;
kmVec2 enemy_pos;
float enemy_rot = 0;
long seed = 42;
binocle_color color_grey;
binocle_render_target screen_render_target;
binocle_render_target ui_buffer;
binocle_shader default_shader;
binocle_shader quad_shader;
binocle_shader ui_shader;
float time = 0;
binocle_audio audio;
binocle_music *music;
binocle_music *game_music;
binocle_texture player_texture;
binocle_texture texture;
int num_frames = 0;
float gravity = 0.09f;
struct player_t player;
binocle_texture atlas_texture;
binocle_subtexture atlas_subtextures[ATLAS_MAX_SUBTEXTURES];
int atlas_subtextures_num = 0;
binocle_sprite santa_sprite;
game_state_t game_state = GAME_STATE_MENU;
bool show_menu = false;
float scroller_x = 0.0f;
struct entity_t hero;
struct layer_t bg_layer;
struct layer_t walls_layer;
struct layer_t props_layer;
struct tile_t tileset[256];
binocle_texture tiles_texture;
struct entity_t elves[MAX_ELVES];
struct spawner_t spawners[MAX_SPAWNERS];
int score = 0;
binocle_material item_material;
float witch_countdown;
float witch_countdown_original = WITCH_COOLDOWN;
int packages_left;
int packages_left_original = 10;
binocle_material witch_material;
struct witch_t witch;
bool debug_enabled = false;
struct particle_t particles[MAX_PARTICLES];
binocle_sprite star_sprite;
binocle_sprite cloud_sprite;
binocle_sprite box_sprite;
binocle_sound sfx_santa_jump;
binocle_sound sfx_santa_freeze;
binocle_sound sfx_santa_pickup;
binocle_sound sfx_witch_laugh;
binocle_sound sfx_elf_freeze;
binocle_sound sfx_elf_pickup;
binocle_sound sfx_elf_throw;
binocle_sound sfx_go;
binocle_sound sfx_level_completed;
binocle_sound sfx_cd_5;
binocle_sound sfx_cd_4;
binocle_sound sfx_cd_3;
binocle_sound sfx_cd_2;
binocle_sound sfx_cd_1;
struct countdown_voice_t voice_countdowns[MAX_COUNTDOWN_VOICE];
char *binocle_data_dir = NULL;
float camera_shake_cooldown = 0.0f;
kmVec2 camera_shake_direction;
kmVec2 camera_shake_offset;
float camera_shake_intensity = 0.0f;
float camera_shake_degradation = 0.95f;
struct barrel_spawner_t barrels_spawners[MAX_BARRELS_SPAWNERS];
int barrels_spawners_number = 0;
struct barrel_t barrels[MAX_BARRELS];

// Nuklear
struct nk_context ctx;
struct nk_draw_null_texture nuklear_null;

int random_int(int min, int max)
{
  return (min + rand() % (max+1 - min));
}

float random_float(float min, float max) {
    return ((((float) rand()) / (float) RAND_MAX) * (max - min)) + min;
}

void reset_voice_countdowns(float witch_timer) {
  voice_countdowns[0].enabled = true;
  voice_countdowns[0].sound = &sfx_cd_5;
  voice_countdowns[0].cooldown_original = witch_timer - 5;
  voice_countdowns[0].cooldown = voice_countdowns[0].cooldown_original;

  voice_countdowns[1].enabled = true;
  voice_countdowns[1].sound = &sfx_cd_4;
  voice_countdowns[1].cooldown_original = witch_timer - 4;
  voice_countdowns[1].cooldown = voice_countdowns[1].cooldown_original;

  voice_countdowns[2].enabled = true;
  voice_countdowns[2].sound = &sfx_cd_3;
  voice_countdowns[2].cooldown_original = witch_timer - 3;
  voice_countdowns[2].cooldown = voice_countdowns[2].cooldown_original;

  voice_countdowns[3].enabled = true;
  voice_countdowns[3].sound = &sfx_cd_2;
  voice_countdowns[3].cooldown_original = witch_timer - 2;
  voice_countdowns[3].cooldown = voice_countdowns[3].cooldown_original;

  voice_countdowns[4].enabled = true;
  voice_countdowns[4].sound = &sfx_cd_1;
  voice_countdowns[4].cooldown_original = witch_timer - 1;
  voice_countdowns[4].cooldown = voice_countdowns[4].cooldown_original;
}


void build_scaling_viewport(int window_width, int window_height,
                            int design_width, int design_height,
                            kmAABB2 *viewport, float *inverse_multiplier, kmMat4 *scale_matrix) {
  int multiplier = 1;
  float scaleX = (float)window_width / (float)design_width;
  float scaleY = (float)window_height / (float)design_height;

  // find the multiplier that fits both the new width and height
  int maxScale = (int)scaleX < (int)scaleY ? (int)scaleX : (int)scaleY;
  if (maxScale > multiplier) {
    multiplier = maxScale;
  }

  // viewport origin translation
  float diffX =
    (window_width / 2.0f) - ((float)design_width * multiplier / 2.0f);
  float diffY =
    (window_height / 2.0f) - ((float)design_height * multiplier / 2.0f);

  // build the new viewport
  viewport->min.x = diffX;
  viewport->min.y = diffY;
  viewport->max.x = design_width * multiplier;
  viewport->max.y = design_height * multiplier;
  *inverse_multiplier = 1.0f / multiplier;

  // compute the scaling matrix
  float matMulX = (viewport->max.x - viewport->min.x)/design_width;
  float matMulY = (viewport->max.y - viewport->min.y)/design_height;
  kmMat4Identity(scale_matrix);
  kmMat4 trans_matrix;
  kmMat4Identity(&trans_matrix);
  kmMat4Translation(&trans_matrix, diffX, diffY, 0.0f);
  kmMat4 sc_matrix;
  kmMat4Identity(&sc_matrix);
  kmMat4Scaling(&sc_matrix, matMulX, matMulY, 1.0f);
  kmMat4Multiply(scale_matrix, &trans_matrix, &sc_matrix);
}

void init_gui() {
  nk_init_default(&ctx, 0);
  struct nk_font_atlas atlas;
  nk_font_atlas_init_default(&atlas);
  nk_font_atlas_begin(&atlas);
  const void *image; int w, h;
  image = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
  binocle_texture t = binocle_texture_from_image_data((unsigned char *)image, w, h);
  nk_font_atlas_end(&atlas, nk_handle_id((int)t.tex_id), &nuklear_null);
  nk_style_set_font(&ctx, &atlas.default_font->handle);
  //if (sdl.atlas.default_font)
  //  nk_style_set_font(&sdl.ctx, &sdl.atlas.default_font->handle);

}

void draw_gui() {
  enum {EASY, HARD};
  static int op = EASY;
  static float value = 0.6f;
  static int i =  20;

  if (game_state == GAME_STATE_MENU) {
    if (nk_begin(&ctx, "Main Menu", nk_rect(20, 50, design_width - 40, design_height - 100),
                 NK_WINDOW_BORDER)) {
      /* fixed widget pixel width */
      //nk_layout_row_static(&ctx, 30, 80, 1);
      nk_layout_row_dynamic(&ctx, 30, 1);
      nk_label(&ctx, "Santa frowns to town", NK_TEXT_CENTERED);
      char t1[2048];
      strcpy(t1, "Santa has been kidnapped and kept away in a tower by the evil witch of Halloween. She's jealous of Santa and wants to take all the gifts to children herself.");
      nk_text_wrap(&ctx, t1, strlen(t1));
      char t2[2048];
      strcpy(t2, "But she still needs Santa to get them ready. It's your mission, as Santa, to get the toys, put them in the packages and wrap them up and deliver to the elves");
      nk_text_wrap(&ctx, t2, strlen(t2));
      char t3[2048];
      strcpy(t3, "who will take care of filling the sled. You have to wrap up enough gifts or the witch will come and sacrifice an Christmas elf to punish you!");
      nk_text_wrap(&ctx, t3, strlen(t3));
      char t4[2048];
      strcpy(t4, "Use the arrow keys to move, UP to jump and SPACE to interact with the toys, boxes, wraps and elves. Pick up the toy, bring it to the box and put it in there.");
      nk_text_wrap(&ctx, t4, strlen(t4));
      char t5[2048];
      strcpy(t5, "Then wrap up the box and bring it to an elf. The elf will take care of delivering it to the sled. Hurry up!");
      nk_text_wrap(&ctx, t5, strlen(t5));
      if (nk_button_label(&ctx, "Start")) {
        player.dead = false;
        scroller_x = 0.0f;
        player.pos.x = roundf(design_width / 3.0f);
        player.pos.y = roundf(design_height / 2.0f);
        player.speed.y = 0;
        witch_countdown_original = WITCH_COOLDOWN;
        witch_countdown = witch_countdown_original;
        packages_left = packages_left_original;
        score = 0;
        for (int i = 0 ; i < MAX_ELVES ; i++) {
          elves[i].dead = false;
        }
        reset_voice_countdowns(witch_countdown);
        binocle_audio_play_music(&audio, game_music, true);
        binocle_audio_set_music_volume(64);
        game_state = GAME_STATE_RUN;
      }
      if (nk_button_label(&ctx, "Quit")) {
        input.quit_requested = true;
      }

      /* fixed widget window ratio width */
      /*
      nk_layout_row_dynamic(&ctx, 30, 2);
      if (nk_option_label(&ctx, "easy", op == EASY)) op = EASY;
      if (nk_option_label(&ctx, "hard", op == HARD)) op = HARD;
      */

      /* custom widget pixel width */
      /*
      nk_layout_row_begin(&ctx, NK_STATIC, 30, 2);
      {
        nk_layout_row_push(&ctx, 50);
        nk_label(&ctx, "Volume:", NK_TEXT_LEFT);
        nk_layout_row_push(&ctx, 110);
        nk_slider_float(&ctx, 0, &value, 1.0f, 0.1f);
      }
      nk_layout_row_end(&ctx);
      */
    }
    nk_end(&ctx);
  } else if (game_state == GAME_STATE_GAMEOVER) {
    if (nk_begin(&ctx, "Game Over", nk_rect(20, 50, design_width - 40, design_height - 100),
                 NK_WINDOW_BORDER)) {
      nk_layout_row_dynamic(&ctx, 30, 1);
      char sc[100];
      sprintf(sc, "Your score: %d", score);
      nk_label(&ctx, sc, NK_TEXT_CENTERED);
      if (nk_button_label(&ctx, "Continue")) {
        game_state = GAME_STATE_MENU;
      }
    }
    nk_end(&ctx);
  }
}

void draw_debug_gui() {
  if (nk_begin(&ctx, "Debug", nk_rect(20, 50, 200, design_height - 100),
               NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_SCALABLE)) {

    nk_layout_row_dynamic(&ctx, 30, 3);

    nk_label(&ctx, "Hero CX", NK_TEXT_CENTERED);
    nk_slider_int(&ctx, 0, &hero.cx, map_width_in_tiles, 1);
    static char cx[20];
    sprintf(cx, "%d", hero.cx);
    nk_label(&ctx, cx, NK_TEXT_CENTERED);

    nk_label(&ctx, "Hero CY", NK_TEXT_CENTERED);
    nk_slider_int(&ctx, 0, &hero.cy, map_height_in_tiles, 1);
    static char cy[20];
    sprintf(cy, "%d", hero.cy);
    nk_label(&ctx, cy, NK_TEXT_CENTERED);

    nk_label(&ctx, "Hero DX", NK_TEXT_CENTERED);
    nk_slider_float(&ctx, -10, &hero.dx, 10, 1);
    static char dx[20];
    sprintf(dx, "%2.3f", hero.dx);
    nk_label(&ctx, dx, NK_TEXT_CENTERED);

    nk_label(&ctx, "Hero DY", NK_TEXT_CENTERED);
    nk_slider_float(&ctx, -10, &hero.dy, 10, 1);
    static char dy[20];
    sprintf(dy, "%2.3f", hero.dy);
    nk_label(&ctx, dy, NK_TEXT_CENTERED);

    nk_label(&ctx, "Hero XR", NK_TEXT_CENTERED);
    nk_slider_float(&ctx, 0, &hero.xr, 1, 0.01f);
    static char xr[20];
    sprintf(xr, "%2.3f", hero.xr);
    nk_label(&ctx, xr, NK_TEXT_CENTERED);

    nk_label(&ctx, "Hero YR", NK_TEXT_CENTERED);
    nk_slider_float(&ctx, 0, &hero.yr, 1, 0.01f);
    static char yr[20];
    sprintf(yr, "%2.3f", hero.yr);
    nk_label(&ctx, yr, NK_TEXT_CENTERED);

  }
  nk_end(&ctx);
}

void render_gui(kmAABB2 viewport) {
  int max_vertex_buffer = 1024 * 512;
  int max_element_buffer = 1024 * 128;
  const struct nk_draw_command *cmd;
  const nk_draw_index *offset = NULL;
  struct nk_convert_config cfg = { 0 };
  static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct binocle_vpct, pos)},
    {NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(struct binocle_vpct, color)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct binocle_vpct, tex)},
    {NK_VERTEX_LAYOUT_END}
  };
  cfg.shape_AA = NK_ANTI_ALIASING_ON;
  cfg.line_AA = NK_ANTI_ALIASING_ON;
  cfg.vertex_layout = vertex_layout;
  cfg.vertex_size = sizeof(struct binocle_vpct);
  cfg.vertex_alignment = NK_ALIGNOF(struct binocle_vpct);
  cfg.circle_segment_count = 22;
  cfg.curve_segment_count = 22;
  cfg.arc_segment_count = 22;
  cfg.global_alpha = 1.0f;
  cfg.null = nuklear_null;
//
// setup buffers and convert
  struct nk_buffer cmds, verts, idx;
  void *vertices, *elements;
  vertices = malloc((size_t)max_vertex_buffer);
  elements = malloc((size_t)max_element_buffer);
  nk_buffer_init_default(&cmds);
  nk_buffer_init_fixed(&verts, vertices, (nk_size)max_vertex_buffer);
  nk_buffer_init_fixed(&idx, elements, (nk_size)max_element_buffer);
  nk_convert(&ctx, &cmds, &verts, &idx, &cfg);

  binocle_gd_set_render_target(ui_buffer);
  binocle_gd_apply_shader(&gd, ui_shader);
  binocle_gd_clear(binocle_color_new(0, 0, 0, 0));

  kmMat4 projectionMatrix = binocle_math_create_orthographic_matrix_off_center(viewport.min.x, viewport.max.x, viewport.max.y, viewport.min.y, -1000.0f, 1000.0f);
  kmMat4 viewMatrix;
  kmMat4Identity(&viewMatrix);
  kmMat4 modelMatrix;
  kmMat4Identity(&modelMatrix);

  glCheck(glActiveTexture(GL_TEXTURE0));

  GLuint vbo;
  GLuint ebo;
  glCheck(glGenBuffers(1, &vbo));
  glCheck(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  glCheck(glGenBuffers(1, &ebo));
  glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));

  glCheck(glEnableVertexAttribArray (gd.vertex_attribute));
  glCheck(glEnableVertexAttribArray (gd.color_attribute));
  glCheck(glEnableVertexAttribArray (gd.tex_coord_attribute));

  glCheck(glVertexAttribPointer(gd.vertex_attribute, 2, GL_FLOAT, GL_FALSE, sizeof(binocle_vpct), 0));
  glCheck(glVertexAttribPointer(gd.color_attribute, 4, GL_FLOAT, GL_FALSE, sizeof(binocle_vpct), (void *) (2 * sizeof(GLfloat))));
  glCheck(glVertexAttribPointer(gd.tex_coord_attribute, 2, GL_FLOAT, GL_FALSE, sizeof(binocle_vpct),(void *) (4 * sizeof(GLfloat) + 2 * sizeof(GLfloat))));

  glCheck(glUniformMatrix4fv(gd.projection_matrix_uniform, 1, GL_FALSE, projectionMatrix.mat));
  glCheck(glUniformMatrix4fv(gd.view_matrix_uniform, 1, GL_FALSE, viewMatrix.mat));
  glCheck(glUniformMatrix4fv(gd.model_matrix_uniform, 1, GL_FALSE, modelMatrix.mat));
  glCheck(glUniform1i(gd.image_uniform, 0));

  glCheck(glBufferData(GL_ARRAY_BUFFER, max_vertex_buffer, NULL, GL_STREAM_DRAW));
  glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_element_buffer, NULL, GL_STREAM_DRAW));

  glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, (size_t)max_vertex_buffer, vertices));
  glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (size_t)max_element_buffer, elements));
  free(vertices);
  free(elements);

//
// draw
  nk_draw_foreach(cmd, &ctx, &cmds) {
    if (!cmd->elem_count) continue;
    glCheck(glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id));
    glCheck(glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset));
    offset += cmd->elem_count;
  }
  nk_buffer_free(&cmds);
  nk_buffer_free(&verts);
  nk_buffer_free(&idx);

  nk_clear(&ctx);

  glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
  glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

  glCheck(glDeleteBuffers(1, &vbo));
  glCheck(glDeleteBuffers(1, &ebo));
}

void pass_input_to_gui(binocle_input *input) {
  nk_input_begin(&ctx);
  nk_input_motion(&ctx, input->mouseX, input->mouseY);
  nk_input_button(&ctx, NK_BUTTON_LEFT, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_LEFT]);
  nk_input_button(&ctx, NK_BUTTON_MIDDLE, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_MIDDLE]);
  nk_input_button(&ctx, NK_BUTTON_RIGHT, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_RIGHT]);
  nk_input_end(&ctx);
}

void build_bg(int *data, int data_count, int firstgid, int width, int height) {
  for (int h = 0 ; h < height ; h++) {
    for (int w = 0 ; w < width ; w++) {
      bg_layer.tiles_gid[h * width + w] = data[((height - 1) - h) * width + w] - firstgid;
    }
  }
}

void build_walls(int *data, int data_count, int firstgid, int width, int height) {
  for (int h = 0 ; h < height ; h++) {
    for (int w = 0 ; w < width ; w++) {
      walls_layer.tiles_gid[h * width + w] = data[((height - 1) - h) * width + w] - firstgid;
    }
  }
}

void build_props(int *data, int data_count, int firstgid, int width, int height) {
  for (int h = 0 ; h < height ; h++) {
    for (int w = 0 ; w < width ; w++) {
      props_layer.tiles_gid[h * width + w] = data[((height - 1) - h) * width + w] - firstgid;
    }
  }
}

void build_spawner(int index, float x, float y, item_kind_t item_kind) {
  spawners[index].entity.pos.x = x;
  spawners[index].entity.pos.y = (map_height_in_tiles - 1) * GRID - y;
  spawners[index].entity.cx = (int)(x/GRID);
  spawners[index].entity.cy = (int)(y/GRID);
  spawners[index].entity.xr = (spawners[index].entity.pos.x - spawners[index].entity.cx * GRID) / GRID;
  spawners[index].entity.yr = (spawners[index].entity.pos.y - spawners[index].entity.cy * GRID) / GRID;
  spawners[index].entity.has_gravity = false;
  spawners[index].item_kind = item_kind;
}

void build_barrels_spawner(float x, float y, int dir) {
  barrels_spawners[barrels_spawners_number].pos_x = x;
  barrels_spawners[barrels_spawners_number].pos_y = (map_height_in_tiles - 1) * GRID - y;
  barrels_spawners[barrels_spawners_number].cooldown = 3;
  barrels_spawners[barrels_spawners_number].dir = dir;
  barrels_spawners_number++;
}

void load_tilemap() {
  char filename[1024];
  sprintf(filename, "%s%s", binocle_data_dir, "map.json");
  char *json = NULL;
  size_t json_length = 0;
  if (!binocle_sdl_load_text_file(filename, &json, &json_length)) {
    return;
  }

  cute_tiled_map_t *map = cute_tiled_load_map_from_memory(json, json_length, 0);

  // get map width and height
  int w = map->width;
  int h = map->height;

  cute_tiled_tileset_t *tileset = map->tilesets;

  // loop over the map's layers
  cute_tiled_layer_t* layer = map->layers;
  while (layer)
  {
    int* data = layer->data;
    int data_count = layer->data_count;

    if (strcmp(layer->name.ptr, "bg") == 0) {
      build_bg(data, data_count, tileset->firstgid, w, h);
    } else if (strcmp(layer->name.ptr, "walls") == 0) {
      build_walls(data, data_count, tileset->firstgid, w, h);
    } else if (strcmp(layer->name.ptr, "props") == 0) {
      build_props(data, data_count, tileset->firstgid, w, h);
    } else if (strcmp(layer->name.ptr, "items") == 0) {
      cute_tiled_object_t *object = layer->objects;
      while (object) {
        if (strcmp(object->name.ptr, "toys") == 0) {
          build_spawner(0, object->x, object->y, ITEM_KIND_TOY);
        } else if (strcmp(object->name.ptr, "packs") == 0) {
          build_spawner(1, object->x, object->y, ITEM_KIND_PACKAGE);
        } else if (strcmp(object->name.ptr, "wraps") == 0) {
          build_spawner(2, object->x, object->y, ITEM_KIND_WRAP);
        } else if (strcmp(object->name.ptr, "barrels-l") == 0) {
          build_barrels_spawner(object->x, object->y, 1);
        } else if (strcmp(object->name.ptr, "barrels-r") == 0) {
          build_barrels_spawner(object->x, object->y, -1);
        }

        object = object->next;
      }
    }

    layer = layer->next;
  }

  cute_tiled_free_map(map);
  free(json); // TODO: this causes errors on Windows. Find out why

}

void spawn_particle(binocle_sprite *sprite, float x, float y, float cooldown, int num) {
  for (int i = 0 ; i < MAX_PARTICLES ; i++) {
    if (!particles[i].alive) {
      particles[i].alive = true;
      particles[i].sprite = sprite;
      particles[i].pos.x = x;
      particles[i].pos.y = y;
      particles[i].speed.x = random_float(-100, 100);
      particles[i].speed.y = random_float(-100, 100);
      particles[i].cooldown = cooldown;
      particles[i].scale.x = 1;
      particles[i].scale.y = 1;
      num--;
      if (num == 0) {
        break;
      }
    }
  }
}

void spawn_particle_with_target(binocle_sprite *sprite, float x, float y, float target_x, float target_y, float cooldown) {
  for (int i = 0 ; i < MAX_PARTICLES ; i++) {
    if (!particles[i].alive) {
      particles[i].alive = true;
      particles[i].sprite = sprite;
      particles[i].pos.x = x;
      particles[i].pos.y = y;
      particles[i].speed.x = (target_x - x)/cooldown;
      particles[i].speed.y = (target_y - y)/cooldown;
      particles[i].cooldown = cooldown;
      particles[i].scale.x = 1;
      particles[i].scale.y = 1;
      break;
    }
  }
}

void update_particles() {
  for (int i = 0 ; i < MAX_PARTICLES ; i++) {
    if (particles[i].alive) {
      if (particles[i].cooldown < 0) {
        particles[i].alive = false;
        continue;
      }

      particles[i].pos.x += particles[i].speed.x * (binocle_window_get_frame_time(&window) / 1000.0f);
      particles[i].pos.y += particles[i].speed.y * (binocle_window_get_frame_time(&window) / 1000.0f);

      particles[i].cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
    }
  }
}

bool level_has_any_collision(int cx, int cy) {
  if (cx < 0 || cy < 0) {
    return false;
  }
  if (walls_layer.tiles_gid[cy * map_width_in_tiles + cx] != -1) {
    return true;
  }
  return false;
}

bool level_has_hard_collision(int cx, int cy) {
  if (cx < 0 || cy < 0) {
    return false;
  }
  if (walls_layer.tiles_gid[cy * map_width_in_tiles + cx] != -1) {
    return true;
  }
  return false;
}

bool spawn_item(struct entity_t *entity, item_kind_t item_kind) {
  entity->rot = 0;
  entity->sprite = binocle_sprite_from_material(&item_material);
  if (item_kind == ITEM_KIND_TOY) {
    entity->sprite.subtexture = atlas_subtextures[9];
  } else if (item_kind == ITEM_KIND_PACKAGE) {
    entity->sprite.subtexture = atlas_subtextures[10];
  } else if (item_kind == ITEM_KIND_WRAP) {
    entity->sprite.subtexture = atlas_subtextures[2];
  }
  entity->sprite.origin.x = 0.5f * entity->sprite.subtexture.rect.max.x;
  entity->sprite.origin.y = 0.0f * entity->sprite.subtexture.rect.max.y;
  entity->dx = 0;
  entity->dy = 0;
  entity->xr = 0.5f;
  entity->yr = 1.0f;
  entity->cx = 0;
  entity->cy = 0;
  entity->frict = 0.8f;
  entity->has_gravity = false;
  entity->dir = 1;
  entity->scale.x = 1;
  entity->scale.y = 1;
  return true;
}

bool spawn_witch(struct entity_t *entity) {
  entity->rot = 0;
  entity->sprite = binocle_sprite_from_material(&witch_material);
  entity->sprite.subtexture = atlas_subtextures[13];
  entity->sprite.origin.x = 0.5f * entity->sprite.subtexture.rect.max.x;
  entity->sprite.origin.y = 0.5f * entity->sprite.subtexture.rect.max.y;
  entity->dx = 0;
  entity->dy = 0;
  entity->xr = 0.5f;
  entity->yr = 1.0f;
  entity->cx = 0;
  entity->cy = 0;
  entity->frict = 0.8f;
  entity->has_gravity = false;
  entity->dir = -1;
  entity->scale.x = 1;
  entity->scale.y = 1;
  return true;
}

/*
struct entity_t entity_new() {
  struct entity_t res = { 0 };
  res.hei = GRID;
  return res;
}
*/

void entity_set_position(struct entity_t *entity, float x, float y) {
  entity->pos.x = x;
  entity->pos.y = y;
  entity->cx = (int)(x/GRID);
  entity->cy = (int)(y/GRID);
  entity->xr = (entity->pos.x - entity->cx * GRID) / GRID;
  entity->yr = (entity->pos.y - entity->cy * GRID) / GRID;
}

void entity_set_grid_position(struct entity_t *entity, int cx, int cy) {
  entity->cx = cx;
  entity->cy = cy;
  entity->xr = 0.5f;
  entity->yr = 1.0f;
  entity->pos.x = (int64_t)((entity->cx + entity->xr) * GRID);
  entity->pos.y = (int64_t)((entity->cy + entity->yr) * GRID);
}

float entity_foot_x(struct entity_t *entity) {
  return (entity->cx + entity->xr) * GRID;
}

float entity_foot_y(struct entity_t *entity) {
  return (entity->cy + entity->yr) * GRID;
}

float entity_head_x(struct entity_t *entity) {
  return (entity->cx + entity->xr) * GRID;
}

float entity_head_y(struct entity_t *entity) {
  return (entity->cy + entity->yr) * GRID - entity->hei;
}

float entity_dist_px(struct entity_t *e1, struct entity_t *e2) {
  float dist_sqr = (entity_foot_x(e1) - entity_foot_x(e2)) * (entity_foot_x(e1) - entity_foot_x(e2)) +
    (entity_foot_y(e1) - entity_foot_y(e2)) * (entity_foot_y(e1) - entity_foot_y(e2));
  return sqrtf(dist_sqr);
}

void entity_update(struct entity_t *e) {
  float repel = 0.08f;
  float repelF = 0.6f;

  // X
  e->xr += e->dx;
  if (e->xr >= 0.9f && level_has_hard_collision(e->cx + 1, e->cy) ) {
    e->xr = 0.9f;
  }
  if (e->xr > 0.8f && level_has_hard_collision(e->cx + 1, e->cy) ) {
    e->dx *= repelF;
    e->dx -= repel;
  }

  if (e->xr <= 0.1f && level_has_hard_collision(e->cx - 1, e->cy) ) {
    e->xr = 0.1f;
  }
  if (e->xr < 0.2f && level_has_hard_collision(e->cx - 1,e->cy) ) {
    e->dx *= repelF;
    e->dx += repel;
  }

  while (e->xr > 1) {
    e->xr--;
    e->cx++;
  }
  while(e->xr < 0) {
    e->xr++;
    e->cx--;
  }
  e->dx *= e->frict;
  if (fabsf(e->dx) <= 0.01f ) {
    e->dx = 0;
  }

  // Y
  if (e->has_gravity && !e->on_ground) {
    e->dy -= gravity;
  }
  e->yr += e->dy;
  if (e->dy >= 0) {
    e->fall_start_y = e->pos.y;
  }
  if(e->yr < 0 && level_has_any_collision(e->cx, e->cy - 1) ) {
    e->dy = 0;
    e->yr = 0;
    //onLand( (e->pos.y-e->fall_start_y)/GRID );
  }
  if(e->yr >= 0.4f && level_has_hard_collision(e->cx, e->cy + 1) ) {
    e->dy = 0;
    e->yr = 0.4f;
  }
  if(e->yr > 0.8f && level_has_hard_collision(e->cx, e->cy + 1) ) {
    e->dy *= repelF;
    e->dy += repel;
  }

  while (e->yr > 1 ) {
    e->yr--;
    e->cy++;
  }
  while (e->yr < 0 ) {
    e->yr++;
    e->cy--;
  }
  e->dy *= e->frict;
  if (fabsf(e->dy) <= 0.01f) {
    e->dy = 0;
  }

  if(e->on_ground) {
    e->last_stable_y = e->cy;
  }

  e->on_ground = e->yr==0 && e->dy==0 && level_has_any_collision(e->cx, e->cy-1);

  e->pos.x = (int64_t)((e->cx + e->xr) * GRID);
  e->pos.y = (int64_t)((e->cy + e->yr) * GRID);
  e->scale.x = e->dir;
  e->scale.y = 1;
}

void elves_update() {
  float speed = 0.8f;
  for (int i = 0 ; i < MAX_ELVES ; i++) {
    if (elves[i].dead) {
      continue;
    }

    if (elves[i].carried_item_kind == ITEM_KIND_NONE) {
      if (elves[i].dir == 1) {
        elves[i].dx += speed * (binocle_window_get_frame_time(&window) / 1000.0f);
      } else {
        elves[i].dx -= speed * (binocle_window_get_frame_time(&window) / 1000.0f);
      }

      if (elves[i].cx == 1 && elves[i].dir == -1) {
        elves[i].dir = 1;
      } else if (elves[i].cx == map_width_in_tiles - 2 && elves[i].dir == 1) {
        elves[i].dir = -1;
      }
    } else if (elves[i].carried_item_kind == ITEM_KIND_WRAP) {
      elves[i].dir = 1;
      elves[i].dx += speed * (binocle_window_get_frame_time(&window) / 1000.0f);
      if (elves[i].cx == map_width_in_tiles - 2) {
        elves[i].carried_item_kind = ITEM_KIND_NONE;
        free(elves[i].carried_entity);
        elves[i].carried_entity = NULL;
        spawn_particle_with_target(&box_sprite, elves[i].pos.x, elves[i].pos.y, 20 * GRID, 5 * GRID, 0.5f);
        binocle_audio_play_sound(&audio, &sfx_elf_throw);
        score += 1;
        packages_left -= 1;
        if (packages_left < 0) {
          packages_left = 0;
        }
      }
    }

    binocle_sprite_play_animation(&elves[i].sprite, "elfWalk", false);
    binocle_sprite_update(&elves[i].sprite, (binocle_window_get_frame_time(&window) / 1000.0f));

  }
}

void kill_elf(struct entity_t *elf) {
  elf->dead = true;
  spawn_particle(&cloud_sprite, elf->pos.x, elf->pos.y, 1, 5);
  binocle_audio_play_sound(&audio, &sfx_elf_freeze);
}

void witch_update() {
  float center_x = (witch.entity.cx+witch.entity.xr) * GRID;
  float center_y = (witch.entity.cy+witch.entity.yr) * GRID;
  float a = atan2f(witch.start_y-center_y, witch.start_x-center_x);
  float s = 0.030f;//0.0030f;
  witch.entity.dx += cosf(a)*s*(binocle_window_get_frame_time(&window) / 1000.0f);
  witch.entity.dy += sinf(a)*s*(binocle_window_get_frame_time(&window) / 1000.0f);

  s = 0.015f;//0.0015f;
  witch.wander_ang += random_float(0.03f, 0.06f) * (binocle_window_get_frame_time(&window) / 1000.0);
  witch.entity.dx+= cosf(witch.wander_ang)*s*(binocle_window_get_frame_time(&window) / 1000.0f);
  witch.entity.dy+= sinf(witch.wander_ang)*s*(binocle_window_get_frame_time(&window) / 1000.0f);

  entity_update(&witch.entity);

  if (witch.floating_cooldown > 0) {
    witch.floating_cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
    return;
  }

  if (!witch.sacrifice_done) {
    // Sacrifice the elf
    for (int i = 0 ; i < MAX_ELVES ; i++) {
      if (!elves[i].dead) {
        kill_elf(&elves[i]);
        break;
      }
    }
    witch.sacrifice_done = true;
  }

  if (witch.sacrifice_cooldown > 0) {
    witch.sacrifice_cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
    return;
  }

  int elves_alive = 0;
  for (int i = 0 ; i < MAX_ELVES ; i++) {
    if (!elves[i].dead) {
      elves_alive++;
    }
  }

  if (elves_alive == 0) {
    game_state = GAME_STATE_GAMEOVER;
    binocle_audio_play_music(&audio, music, true);
    binocle_audio_set_music_volume(64);
    return;
  }

  // Back to game with one elf less
  witch_countdown = witch_countdown_original;
  binocle_audio_play_sound(&audio, &sfx_go);
  game_state = GAME_STATE_RUN;
}

void start_camera_shake() {
  camera_shake_cooldown = 3.0f;
  camera_shake_intensity = 15.0f;
  camera_shake_direction.x = 0;
  camera_shake_direction.y = 0;
}

void reset_camera() {
  binocle_camera_set_position(&camera, 0, 0);
}

void update_camera() {
  if (camera_shake_cooldown < 0) {
    reset_camera();
    return;
  }
  camera_shake_cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);

  if (fabsf(camera_shake_intensity) > 0) {
    camera_shake_offset.x = camera_shake_direction.x;
    camera_shake_offset.y = camera_shake_direction.y;

    if(camera_shake_offset.x != 0.0f || camera_shake_offset.y != 0.0f )
    {
      kmVec2Normalize(&camera_shake_offset, &camera_shake_offset);
    }
    else
    {
      camera_shake_offset.x = camera_shake_offset.x + random_float(0, 1) - 0.5f;
      camera_shake_offset.y = camera_shake_offset.y + random_float(0, 1) - 0.5f;
    }

    // TODO: this needs to be multiplied by camera zoom so that less shake gets applied when zoomed in
    camera_shake_offset.x *= camera_shake_intensity;
    camera_shake_offset.y *= camera_shake_intensity;
    camera_shake_intensity *= camera_shake_degradation;
    if(fabsf(camera_shake_intensity) <= 0.01f )
    {
      camera_shake_intensity = 0.0f;
      camera_shake_offset.x = 0.0f;
      camera_shake_offset.y = 0.0f;
      reset_camera();
    }
    binocle_camera_set_position(&camera, camera.position.x + (int)camera_shake_offset.x, camera.position.y + (int)camera_shake_offset.y);
  }
}

void spawn_barrel(float pos_x, float pos_y, int dir) {
  for (int i = 0 ; i < MAX_BARRELS ; i++) {
    if (!barrels[i].alive) {
      barrels[i].alive = true;
      barrels[i].entity.dir = dir;
      entity_set_position(&barrels[i].entity, pos_x, pos_y);
      binocle_sprite_play_animation(&barrels[i].entity.sprite, "barrelRoll", true);
      break;
    }
  }
}

void barrels_spawners_update() {
  for (int i = 0 ; i < barrels_spawners_number ; i++) {
    if (barrels_spawners[i].cooldown < 0) {
      spawn_barrel(barrels_spawners[i].pos_x, barrels_spawners[i].pos_y, barrels_spawners[i].dir);
      barrels_spawners[i].cooldown = 5;
    }
    barrels_spawners[i].cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
  }
}

void update_barrels() {
  float speed = 0.4f;
  for (int i = 0 ; i < MAX_BARRELS ; i++) {
    if (barrels[i].alive) {
      if (barrels[i].entity.dir == 1) {
        barrels[i].entity.dx += speed * (binocle_window_get_frame_time(&window) / 1000.0f);
      } else {
        barrels[i].entity.dx -= speed * (binocle_window_get_frame_time(&window) / 1000.0f);
      }
      if (barrels[i].entity.cx == 1 && barrels[i].entity.xr <= 0.2f && barrels[i].entity.dir == -1) {
        barrels[i].alive = false;
      } else if (barrels[i].entity.cx == map_width_in_tiles - 2 && barrels[i].entity.xr >= 0.8f && barrels[i].entity.dir == 1) {
        barrels[i].alive = false;
      }
      entity_update(&barrels[i].entity);
      binocle_sprite_update(&barrels[i].entity.sprite, (binocle_window_get_frame_time(&window) / 1000.0f));
    }
  }
}

void game_update() {
  entity_update(&hero);

  if (hero.carried_entity != NULL) {
    entity_set_position(hero.carried_entity, hero.pos.x, hero.pos.y);
  }

  for (int i = 0 ; i < MAX_ELVES ; i++) {
    entity_update((&elves[i]));
    if (elves[i].carried_entity != NULL) {
      entity_set_position(elves[i].carried_entity, elves[i].pos.x, elves[i].pos.y);
    }
  }

  for (int i = 0 ; i < MAX_SPAWNERS ; i++) {
    entity_update((&spawners[i].entity));
  }

  if (player.dead) {
    game_state = GAME_STATE_GAMEOVER;
    return;
  }

  float speed = 2.0f;

  if (game_state == GAME_STATE_WITCH) {
    // Ignore player input while displaying the witch
    witch_update();
    entity_update(&witch.entity);
    if (game_state == GAME_STATE_GAMEOVER || game_state == GAME_STATE_RUN) {
      return;
    }
  } else {
    if (!hero.locked) {
      if (binocle_input_is_key_pressed(input, KEY_RIGHT)) {
        hero.dx += speed * (binocle_window_get_frame_time(&window) / 1000.0f);
        hero.dir = 1;
      } else if (binocle_input_is_key_pressed(input, KEY_LEFT)) {
        hero.dx -= speed * (binocle_window_get_frame_time(&window) / 1000.0f);
        hero.dir = -1;
      }

      if (binocle_input_is_key_pressed(input, KEY_UP)) {
        if (hero.on_ground) {
          hero.dy = 30.0f * (binocle_window_get_frame_time(&window) / 1000.0f);
          hero.dx *= 1.2f;
          binocle_audio_play_sound(&audio, &sfx_santa_jump);
        }
      } else if (binocle_input_is_key_pressed(input, KEY_DOWN)) {
      }

      if (binocle_input_is_key_pressed(input, KEY_SPACE)) {
        // Interaction with spawners
        for (int i = 0 ; i < MAX_SPAWNERS ; i++) {
          if (hero.cx == spawners[i].entity.cx
              && hero.cy == spawners[i].entity.cy) {
            if (hero.carried_item_kind == ITEM_KIND_NONE && spawners[i].item_kind == ITEM_KIND_TOY) {
              hero.carried_item_kind = ITEM_KIND_TOY;
              hero.carried_entity = malloc(sizeof(struct entity_t));
              spawn_item(hero.carried_entity, ITEM_KIND_TOY);
              binocle_audio_play_sound(&audio, &sfx_santa_pickup);
            } else if (hero.carried_item_kind == ITEM_KIND_TOY && spawners[i].item_kind == ITEM_KIND_PACKAGE) {
              hero.carried_item_kind = ITEM_KIND_PACKAGE;
              free(hero.carried_entity);
              hero.carried_entity = malloc(sizeof(struct entity_t));
              spawn_item(hero.carried_entity, ITEM_KIND_PACKAGE);
              binocle_audio_play_sound(&audio, &sfx_santa_pickup);
            } else if (hero.carried_item_kind == ITEM_KIND_PACKAGE && spawners[i].item_kind == ITEM_KIND_WRAP) {
              hero.carried_item_kind = ITEM_KIND_WRAP;
              free(hero.carried_entity);
              hero.carried_entity = malloc(sizeof(struct entity_t));
              spawn_item(hero.carried_entity, ITEM_KIND_WRAP);
              binocle_audio_play_sound(&audio, &sfx_santa_pickup);
            }
          }
        }

        // Interaction with elves
        for (int i = 0 ; i < MAX_ELVES ; i++) {
          if (hero.cx == elves[i].cx
              && hero.cy == elves[i].cy) {
            if (hero.carried_item_kind == ITEM_KIND_WRAP && !elves[i].dead && elves[i].carried_item_kind == ITEM_KIND_NONE) {
              elves[i].carried_item_kind = ITEM_KIND_WRAP;
              elves[i].carried_entity = hero.carried_entity;
              hero.carried_item_kind = ITEM_KIND_NONE;
              hero.carried_entity = NULL;
              binocle_audio_play_sound(&audio, &sfx_elf_pickup);
            }
          }
        }
      }

      // Interaction with barrels
      for (int i = 0; i < MAX_BARRELS ; i++) {
        if (barrels[i].alive) {
          if (entity_dist_px(&hero, &barrels[i].entity) < 10) {
            hero.locked = true;
            hero.lock_cooldown = 1;
          }
        }
      }

    } else {
      if (hero.lock_cooldown < 0) {
        hero.locked = false;
        hero.lock_cooldown = 0;
      }
      hero.lock_cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
    }

    if (hero.locked) {
      binocle_sprite_play_animation(&hero.sprite, "heroFall", false);
    } else if (!hero.on_ground && hero.dy >0) {
      binocle_sprite_play_animation(&hero.sprite, "heroJump", false); // up
    } else if (!hero.on_ground && hero.dy < 0) {
      binocle_sprite_play_animation(&hero.sprite, "heroJump", false); // down
    } else if (fabs(hero.dx) >= 0.05f) {
      binocle_sprite_play_animation(&hero.sprite, "heroWalk", false);
    } else {
      binocle_sprite_play_animation(&hero.sprite, "heroIdle", false);
    }
  }

  binocle_sprite_update(&hero.sprite, (binocle_window_get_frame_time(&window) / 1000.0f));

  elves_update();

  barrels_spawners_update();

  update_barrels();

  update_particles();

  if (window.frame_time > 0) {
    float dt = (binocle_window_get_frame_time(&window) / 1000.0f);
    //rot += 50 * (binocle_window_get_frame_time(&window) / 1000.0f);
    //rot = (int64_t)rot % 360;
    if (player.speed.y > -300) {
      player.speed.y -= gravity * dt;
    }
    player.pos.y += player.speed.y * dt;

    if (player.pos.y > design_height) {
      player.pos.y = design_height;
    }

    if (player.pos.y < 0) {
      player.pos.y = 0;
    }

    player.rot = kmDegreesToRadians(kmMin(90, kmMax(-20, player.speed.y-140)));

    kmAABB2 player_collider;
    player_collider.min.x = player.pos.x;
    player_collider.min.y = player.pos.y;
    player_collider.max.x = player.pos.x + player.sprite.subtexture.rect.max.x;
    player_collider.max.y = player.pos.y + player.sprite.subtexture.rect.max.y;

    enemy_rot += 50 * (binocle_window_get_frame_time(&window) / 1000.0f);
    enemy_rot = (int64_t)enemy_rot % 360;

    binocle_sprite_update(&enemy, (binocle_window_get_frame_time(&window) / 1000.0f));

    scroller_x += 20.0f * dt;

  }

  for (int i = 0 ; i < MAX_COUNTDOWN_VOICE ; i++) {
    if (voice_countdowns[i].enabled && voice_countdowns[i].cooldown < 0) {
      binocle_audio_play_sound(&audio, voice_countdowns[i].sound);
      voice_countdowns[i].enabled = false;
      continue;
    }
    voice_countdowns[i].cooldown -= (binocle_window_get_frame_time(&window) / 1000.0f);
  }

  if (game_state != GAME_STATE_WITCH) {
    witch_countdown -= (binocle_window_get_frame_time(&window) / 1000.0f);

    if (witch_countdown < 0) {
      if (packages_left > 0) {
        witch_countdown = 0;
        spawn_witch(&witch.entity);
        //entity_set_grid_position(&witch.entity, 19, 5);
        entity_set_grid_position(&witch.entity, 10, 5);
        witch.wander_ang = 0;
        witch.start_x = witch.entity.pos.x;
        witch.start_y = witch.entity.pos.y;
        witch.floating_cooldown = 5;
        witch.sacrifice_cooldown = 5;
        witch.sacrifice_done = false;
        spawn_particle(&star_sprite, witch.entity.pos.x, witch.entity.pos.y, 2, 10);
        binocle_audio_play_sound(&audio, &sfx_witch_laugh);
        binocle_audio_play_sound(&audio, &sfx_santa_freeze);
        start_camera_shake();
        game_state = GAME_STATE_WITCH;
      } else {
        packages_left = packages_left_original;
        witch_countdown_original = witch_countdown_original * 0.9f;
        witch_countdown = witch_countdown_original;
        spawn_particle(&star_sprite, design_width / 2.0f, design_height / 2.0f, 5, 20);
        binocle_audio_play_sound(&audio, &sfx_level_completed);
        reset_voice_countdowns(witch_countdown);
      }
    }
  }

  update_camera();

}

void game_render() {
  kmAABB2 vp_design = {
    .min.x = 0, .min.y = 0, .max.x = design_width, .max.y = design_height};
  kmMat4 identity_mat;
  kmMat4Identity(&identity_mat);

  // Player
  // binocle_sprite_draw(player, &gd, (int64_t)player_pos.x,
  // (int64_t)player_pos.y, binocle_camera_get_viewport(camera), rot *
  // (float)M_PI / 180.0f, 2);
  kmVec2 double_scale;
  double_scale.x = 2;
  double_scale.y = 2;
  binocle_sprite_draw(player.sprite, &gd, (int64_t)player.pos.x, (int64_t)player.pos.y,
                      vp_design, player.rot * (float)M_PI / 180.0f, double_scale, &camera);

  // Enemy
  // binocle_sprite_draw(enemy, &gd, (int64_t)enemy_pos.x, (int64_t)enemy_pos.y,
  // binocle_camera_get_viewport(camera), enemy_rot * (float)M_PI / 180.0f, 2);
  binocle_sprite_draw(enemy, &gd, (int64_t)enemy_pos.x, (int64_t)enemy_pos.y,
                      vp_design, enemy_rot * (float)M_PI / 180.0f, double_scale, &camera);


  kmVec2 scale;
  scale.x = 1;
  scale.y = 1;

  // Background
  for (int h = 0 ; h < map_height_in_tiles ; h++) {
    for (int w = 0 ; w < map_width_in_tiles ; w++ ) {
      if (bg_layer.tiles_gid[h * map_width_in_tiles + w] != -1) {
        binocle_sprite_draw(tileset[bg_layer.tiles_gid[h * map_width_in_tiles + w]].sprite, &gd, w * 32, h * 32,
                            vp_design, 0, scale, &camera);
      }
    }
  }

  // Walls
  for (int h = 0 ; h < map_height_in_tiles ; h++) {
    for (int w = 0 ; w < map_width_in_tiles ; w++ ) {
      if (walls_layer.tiles_gid[h * map_width_in_tiles + w] != -1) {
        binocle_sprite_draw(tileset[walls_layer.tiles_gid[h * map_width_in_tiles + w]].sprite, &gd, w * 32, h * 32, vp_design, 0, scale, &camera);
      }
    }
  }

  // Props
  for (int h = 0 ; h < map_height_in_tiles ; h++) {
    for (int w = 0 ; w < map_width_in_tiles ; w++ ) {
      if (props_layer.tiles_gid[h * map_width_in_tiles + w] != -1) {
        binocle_sprite_draw(tileset[props_layer.tiles_gid[h * map_width_in_tiles + w]].sprite, &gd, w * 32, h * 32,
                            vp_design, 0, scale, &camera);
      }
    }
  }

  // Spawners
  for (int i = 0 ; i < MAX_SPAWNERS ; i++) {
    binocle_sprite_draw(spawners[i].entity.sprite, &gd, (int64_t)spawners[i].entity.pos.x, (int64_t)spawners[i].entity.pos.y,
                        vp_design, 0, spawners[i].entity.scale, &camera);
  }

  // Elves
  for (int i = 0 ; i < MAX_ELVES ; i++) {
    if (!elves[i].dead) {
      binocle_sprite_draw(elves[i].sprite, &gd, (int64_t)elves[i].pos.x, (int64_t)elves[i].pos.y,
                          vp_design, 0, elves[i].scale, &camera);
      if (elves[i].carried_entity != NULL) {
        binocle_sprite_draw(elves[i].carried_entity->sprite, &gd, (int64_t)elves[i].carried_entity->pos.x, (int64_t)elves[i].carried_entity->pos.y,
                            vp_design, 0, elves[i].carried_entity->scale, &camera);
      }
    } else {
      binocle_sprite_draw(elves[i].frozen_sprite, &gd, (int64_t)elves[i].pos.x, (int64_t)elves[i].pos.y,
                          vp_design, 0, elves[i].scale, &camera);
    }
  }

  // Witch
  if (game_state == GAME_STATE_WITCH) {
    binocle_sprite_draw(witch.entity.sprite, &gd, (int64_t)witch.entity.pos.x, (int64_t)witch.entity.pos.y,
                        vp_design, 0, witch.entity.scale, &camera);
    char s[100];
    sprintf(s, "You ran out of time! I'll sacrifice an elf!");
    binocle_bitmapfont_draw_string(font, s, 24, &gd, witch.entity.pos.x - 12 * GRID,
                                   witch.entity.pos.y, vp_design,
                                   binocle_color_new(0.0f/255.0f, 166.0f/255.0f, 81.0f/255.0f, 1.0f), identity_mat);
  }

  // Barrels
  for (int i = 0 ; i < MAX_BARRELS ; i++) {
    if (barrels[i].alive) {
      binocle_sprite_draw(barrels[i].entity.sprite, &gd, (int64_t)barrels[i].entity.pos.x, (int64_t)barrels[i].entity.pos.y,
                          vp_design, 0, barrels[i].entity.scale, &camera);
    }
  }

  // Particles
  for (int i = 0 ; i < MAX_PARTICLES ; i++) {
    if (particles[i].alive) {
      binocle_sprite_draw(*particles[i].sprite, &gd, (int64_t)particles[i].pos.x, (int64_t)particles[i].pos.y,
                          vp_design, 0, particles[i].scale, &camera);
    }
  }

  // Santa

  if (game_state == GAME_STATE_WITCH) {
    binocle_sprite_draw(hero.frozen_sprite, &gd, (int64_t)hero.pos.x, (int64_t)hero.pos.y,
                        vp_design, 0, hero.scale, &camera);
  } else {
    binocle_sprite_draw(hero.sprite, &gd, (int64_t)hero.pos.x, (int64_t)hero.pos.y,
                        vp_design, 0, hero.scale, &camera);
  }
  if (hero.carried_entity != NULL) {
    binocle_sprite_draw(hero.carried_entity->sprite, &gd, (int64_t)hero.carried_entity->pos.x, (int64_t)hero.carried_entity->pos.y + 32,
                        vp_design, 0, hero.carried_entity->scale, &camera);
  }



}

void main_loop() {
  binocle_window_begin_frame(&window);
  binocle_input_update(&input);
  pass_input_to_gui(&input);

  if (input.resized) {
    kmVec2 oldWindowSize = {.x = window.width, .y = window.height};
    window.width = (uint32_t)input.newWindowSize.x;
    window.height = (uint32_t)input.newWindowSize.y;
    binocle_viewport_adapter_reset(&adapter, oldWindowSize,
                                   input.newWindowSize);
    binocle_camera_force_matrix_update(&camera);
    input.resized = false;
  }

  switch(game_state) {
    case GAME_STATE_MENU:
      show_menu = true;
      break;
    case GAME_STATE_RUN:
    case GAME_STATE_WITCH:
      game_update();
      break;
    case GAME_STATE_GAMEOVER:
      break;
    default:
      break;
  }


  // Clear screen
  // binocle_window_clear(&window);

  // Set the main render target
  binocle_gd_set_render_target(screen_render_target);
  // binocle_gd_apply_viewport(binocle_camera_get_viewport(camera));
  kmAABB2 vp_design = {
    .min.x = 0, .min.y = 0, .max.x = design_width, .max.y = design_height};
  kmMat4 identity_mat;
  kmMat4Identity(&identity_mat);
  binocle_gd_apply_viewport(vp_design);
  binocle_gd_clear(binocle_color_new(253/255, 44/255, 13/255, 1));

  binocle_gd_apply_shader(&gd, default_shader);
  // Test rect
  // binocle_gd_draw_rect(&gd, testRect, binocle_color_white(),
  // binocle_camera_get_viewport(camera),
  // binocle_camera_get_transform_matrix(&camera));
  //binocle_gd_draw_rect(&gd, testRect, binocle_color_white(), vp_design, identity_mat);

  if (game_state == GAME_STATE_RUN || game_state == GAME_STATE_WITCH) {
    game_render();
  }


  // GUI
  if (game_state == GAME_STATE_MENU || game_state == GAME_STATE_GAMEOVER) {
    draw_gui();
  } else {
    if (debug_enabled) {
      draw_debug_gui();
    }
  }
  render_gui(vp_design);

  // Score and FPS
  // binocle_bitmapfont_draw_string(font, "SCORE: 0", 32, &gd, 10,
  // window.height-36, binocle_camera_get_viewport(camera),
  // binocle_color_black(), binocle_camera_get_transform_matrix(&camera));
  char score_string[100];
  sprintf(score_string, "SCORE: %d   TIME LEFT: %2.0f   PACKAGES LEFT: %d", score, witch_countdown, packages_left);
  binocle_bitmapfont_draw_string(font, score_string, 32, &gd, 10,
                                 design_height - 36, vp_design,
                                 binocle_color_white(), identity_mat);
  if (debug_enabled) {
    uint64_t fps = binocle_window_get_fps(&window);
    snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %llu", fps);
  }
  // binocle_bitmapfont_draw_string(font, fps_buffer, 32, &gd,
  // window.width-16*7, window.height-36, binocle_camera_get_viewport(camera),
  // binocle_color_black(), binocle_camera_get_transform_matrix(&camera));
  binocle_bitmapfont_draw_string(
    font, fps_buffer, 32, &gd, design_width - 16 * 7, design_height - 36,
    vp_design, binocle_color_black(), identity_mat);


  {
    kmAABB2 vp;
    float multiplier = 1;
    kmMat4 camera_transform_mat;
    kmMat4Identity(&camera_transform_mat);
    build_scaling_viewport(window.width, window.height, design_width,
                           design_height, &vp, &multiplier, &camera_transform_mat);
    kmMat4Identity(&camera_transform_mat);

    binocle_gd_set_render_target(screen_render_target);
    binocle_gd_apply_shader(&gd, quad_shader);
    binocle_gd_set_uniform_float(quad_shader, "time", time);
    binocle_gd_set_uniform_float2(quad_shader, "resolution", design_width,
                                  design_height);
    binocle_gd_set_uniform_mat4(quad_shader, "transform", camera_transform_mat);
    binocle_gd_set_uniform_float2(quad_shader, "scale", multiplier, multiplier);
    binocle_gd_set_uniform_float2(quad_shader, "viewport", vp.min.x, vp.min.y);
    binocle_gd_set_uniform_render_target_as_texture(quad_shader, "texture", ui_buffer);
    binocle_gd_draw_quad(quad_shader);
  }

  binocle_gd_apply_shader(&gd, quad_shader);
  // kmAABB2 vp = {.min.x = (window.width-design_width)/2, .min.y =
  // (window.height - design_height)/2, .max.x = design_width, .max.y =
  // design_height};
  kmAABB2 vp;
  float multiplier = 1;
  kmMat4 camera_transform_mat;
  kmMat4Identity(&camera_transform_mat);
  build_scaling_viewport(window.width, window.height, design_width,
                         design_height, &vp, &multiplier, &camera_transform_mat);
  kmMat4Identity(&camera_transform_mat);
  binocle_gd_apply_viewport(vp);
  binocle_gd_set_uniform_float(quad_shader, "time", time);
  binocle_gd_set_uniform_float2(quad_shader, "resolution", design_width,
                                design_height);
  // kmMat4Translation(&camera_transform_mat, 1.0f -
  // (float)design_width/(float)window.width, 1.0f -
  // (float)design_height/(float)window.height, 0);
  binocle_gd_set_uniform_mat4(quad_shader, "transform", camera_transform_mat);
  binocle_gd_set_uniform_float2(quad_shader, "scale", multiplier, multiplier);
  binocle_gd_set_uniform_float2(quad_shader, "viewport", vp.min.x, vp.min.y);
  binocle_gd_draw_quad_to_screen(quad_shader, screen_render_target);

  time += (binocle_window_get_frame_time(&window) / 1000.0);

  // Blit screen
  binocle_window_refresh(&window);
  binocle_window_end_frame(&window);
  // binocle_log_info("Player position: %f %f", player_pos.x, player_pos.y);

#ifdef __EMSCRIPTEN__
  if (input.quit_requested) {
    emscripten_cancel_main_loop();
  }
#endif
  num_frames++;
}

void init_fonts() {
  char font_filename[1024];
  sprintf(font_filename, "%s%s", binocle_data_dir, "minecraftia.fnt");
  font = binocle_bitmapfont_from_file(font_filename, true);

  char font_image_filename[1024];
  sprintf(font_image_filename, "%s%s", binocle_data_dir, "minecraftia.png");
  font_image = binocle_image_load(font_image_filename);
  font_texture = binocle_texture_from_image(font_image);
  font_material = binocle_material_new();
  font_material.texture = &font_texture;
  font_material.shader = &default_shader;
  font->material = &font_material;
  font_sprite = binocle_sprite_from_material(&font_material);
  font_sprite_pos.x = 0;
  font_sprite_pos.y = -256;
}

void destroy_fonts() { binocle_bitmapfont_destroy(font); }

int main(int argc, char *argv[]) {
  // Init the RNG
  srand48(seed);
  fps_buffer[0] = '\0';
  color_grey = binocle_color_new(0.3f, 0.3f, 0.3f, 1);
  // Init SDL
  binocle_sdl_init();
  // Create the window
  window = binocle_window_new(design_width, design_height, "Santa frowns to town");
  binocle_window_set_minimum_size(&window, design_width, design_height);
  // Updates the window size in case we're on mobile and getting a forced
  // dimension
  uint32_t real_width = 0;
  uint32_t real_height = 0;
  binocle_window_get_real_size(&window, &real_width, &real_height);
  /*
  if (real_width > real_height) {
    uint32_t tmp = real_width;
    real_width = real_height;
    real_height = tmp;
  }
  window.width = real_width;
  window.height = real_height;
   */
  // Default background color
  binocle_window_set_background_color(&window, binocle_color_azure());
  // Default adapter to use with the main camera
  adapter = binocle_viewport_adapter_new(
    window, BINOCLE_VIEWPORT_ADAPTER_KIND_SCALING,
    BINOCLE_VIEWPORT_ADAPTER_SCALING_TYPE_PIXEL_PERFECT, real_width,
    real_height, window.original_width, window.original_height);
  // Main camera
  camera = binocle_camera_new(&adapter);
  // Init the input manager
  input = binocle_input_new();

#if defined(__EMSCRIPTEN__)
  binocle_data_dir = malloc(1024);
  sprintf(binocle_data_dir, "/Users/tanis/Documents/ld43-binocle/assets/");
#else
  char *base_path = SDL_GetBasePath();
  if (base_path) {
    binocle_data_dir = base_path;
  } else {
    binocle_data_dir = SDL_strdup("./");
  }
#endif


  char filename[1024];
  sprintf(filename, "%s%s", binocle_data_dir, "heli.png");
  binocle_log_info("Loading %s", filename);
  binocle_image image = binocle_image_load(filename);
  texture = binocle_texture_from_image(image);
  binocle_shader_init_defaults();
  char vert[1024];
  sprintf(vert, "%s%s", binocle_data_dir, "default.vert");
  char frag[1024];
  sprintf(frag, "%s%s", binocle_data_dir, "default.frag");
  default_shader = binocle_shader_load_from_file(vert, frag);
  binocle_material material = binocle_material_new();
  material.texture = &texture;
  material.shader = &default_shader;
  player.rot = 0;
  player.sprite = binocle_sprite_from_material(&material);
  player.sprite.origin.x = 0.5f * player.sprite.material->texture->width;
  player.sprite.origin.y = 0.5f * player.sprite.material->texture->height;
  player.pos.x = roundf(design_width/3.0f);
  player.pos.y = roundf(design_height/2.0f);
  player.speed.x = 0;
  player.speed.y = 0;

  // Load the default quad shader
  sprintf(vert, "%s%s", binocle_data_dir, "screen.vert");
  sprintf(frag, "%s%s", binocle_data_dir, "screen.frag");
  quad_shader = binocle_shader_load_from_file(vert, frag);

  // Load the UI shader
  sprintf(vert, "%s%s", binocle_data_dir, "default.vert");
  sprintf(frag, "%s%s", binocle_data_dir, "default.frag");
  ui_shader = binocle_shader_load_from_file(vert, frag);


  sprintf(filename, "%s%s", binocle_data_dir, "testlibgdx.png");
  binocle_image player_image = binocle_image_load(filename);
  player_texture = binocle_texture_from_image(player_image);
  binocle_material player_material = binocle_material_new();
  player_material.texture = &player_texture;
  player_material.shader = &default_shader;
  enemy = binocle_sprite_from_material(&player_material);
  enemy.origin.x = 0.5f * 16;
  enemy.origin.y = 0.5f * 16;
  enemy_pos.x = 150;
  enemy_pos.y = 150;

  binocle_subtexture sub1 =
    binocle_subtexture_with_texture(&player_texture, 0, 0, 16, 16);
  binocle_sprite_frame f1 = binocle_sprite_frame_from_subtexture(&sub1);
  binocle_subtexture sub2 =
    binocle_subtexture_with_texture(&player_texture, 16, 0, 16, 16);
  binocle_sprite_frame f2 = binocle_sprite_frame_from_subtexture(&sub2
    );
  binocle_subtexture sub3 =
    binocle_subtexture_with_texture(&player_texture, 32, 0, 16, 16);
  binocle_sprite_frame f3 = binocle_sprite_frame_from_subtexture(
    &sub3);

  binocle_sprite_add_frame(&enemy, f1);
  binocle_sprite_add_frame(&enemy, f2);
  binocle_sprite_add_frame(&enemy, f3);

  int frames[3] = {0, 1, 2};

  binocle_sprite_add_animation_with_frames(&enemy, 0, true, 1, frames, 3);
  binocle_sprite_play(&enemy, 0, true);

  // Load the sprite atlas with all the entities
  sprintf(filename, "%s%s", binocle_data_dir, "entities.png");
  binocle_image atlas_image = binocle_image_load(filename);
  atlas_texture = binocle_texture_from_image(atlas_image);
  sprintf(filename, "%s%s", binocle_data_dir, "entities.json");
  binocle_atlas_load_texturepacker(filename, &atlas_texture, atlas_subtextures, &atlas_subtextures_num);

  // Create the material for items
  item_material = binocle_material_new();
  item_material.texture = &atlas_texture;
  item_material.shader = &default_shader;

  // Create the material for the witch
  witch_material = binocle_material_new();
  witch_material.texture = &atlas_texture;
  witch_material.shader = &default_shader;

  // Create the player
  binocle_material hero_material = binocle_material_new();
  hero_material.texture = &atlas_texture;
  hero_material.shader = &default_shader;
  //hero = entity_new();
  hero.hei = GRID;
  hero.rot = 0;
  hero.sprite = binocle_sprite_from_material(&hero_material);
  hero.sprite.subtexture = atlas_subtextures[0];
  hero.sprite.origin.x = 0.5f * hero.sprite.subtexture.rect.max.x;
  hero.sprite.origin.y = 0.0f * hero.sprite.subtexture.rect.max.y;
  hero.dx = 0;
  hero.dy = 0;
  hero.xr = 0.5f;
  hero.yr = 1.0f;
  hero.cx = 7;
  hero.cy = 5;
  hero.frict = 0.8f;
  hero.has_gravity = true;
  hero.dir = 1;

  hero.frozen_sprite = binocle_sprite_from_material(&hero_material);
  hero.frozen_sprite.subtexture = atlas_subtextures[26];
  hero.frozen_sprite.origin.x = 0.5f * hero.frozen_sprite.subtexture.rect.max.x;
  hero.frozen_sprite.origin.y = 0.0f * hero.frozen_sprite.subtexture.rect.max.y;

  //binocle_sprite_create_animation(&hero.sprite, "heroTest", "tiles_00.png,tiles_17.png,tiles_26.png,tiles_27.png", "0-1,2:3,3:2,0-2:3", true, atlas_subtextures, atlas_subtextures_num);
  binocle_sprite_create_animation(&hero.sprite, "heroIdle", "tiles_00.png,tiles_17.png", "0-1:0.7", true, atlas_subtextures, atlas_subtextures_num);
  binocle_sprite_create_animation(&hero.sprite, "heroWalk", "tiles_18.png,tiles_19.png", "0-1:0.3", true, atlas_subtextures, atlas_subtextures_num);
  binocle_sprite_create_animation(&hero.sprite, "heroJump", "tiles_00.png", "0", false, atlas_subtextures, atlas_subtextures_num);
  binocle_sprite_create_animation(&hero.sprite, "heroFall", "tiles_00.png,tiles_38.png,tiles_39.png", "0-2:0.1", false, atlas_subtextures, atlas_subtextures_num);
  binocle_sprite_play_animation(&hero.sprite, "heroIdle", false);

  // Create the elves
  binocle_material elves_material = binocle_material_new();
  elves_material.texture = &atlas_texture;
  elves_material.shader = &default_shader;
  for (int i = 0 ; i < MAX_ELVES ; i++) {
    //elves[i] = entity_new();
    elves[i].hei = GRID;
    elves[i].rot = 0;
    elves[i].sprite = binocle_sprite_from_material(&elves_material);
    elves[i].sprite.subtexture = atlas_subtextures[1];
    elves[i].sprite.origin.x = 0.5f * elves[i].sprite.subtexture.rect.max.x;
    elves[i].sprite.origin.y = 0.0f * elves[i].sprite.subtexture.rect.max.y;
    elves[i].dx = 0;
    elves[i].dy = 0;
    elves[i].xr = 0.5f;
    elves[i].yr = 1.0f;
    elves[i].cx = (int)(drand48() * (map_width_in_tiles - 2) + 1);
    elves[i].cy = 5;
    elves[i].frict = 0.8f;
    elves[i].has_gravity = true;
    elves[i].dir = random_int(0, 1) == 0 ? -1 : 1;

    elves[i].frozen_sprite = binocle_sprite_from_material(&elves_material);
    elves[i].frozen_sprite.subtexture = atlas_subtextures[33];
    elves[i].frozen_sprite.origin.x = 0.5f * elves[i].frozen_sprite.subtexture.rect.max.x;
    elves[i].frozen_sprite.origin.y = 0.0f * elves[i].frozen_sprite.subtexture.rect.max.y;

    binocle_sprite_create_animation(&elves[i].sprite, "elfWalk", "tiles_21.png,tiles_22.png", "0-1:0.2", true, atlas_subtextures, atlas_subtextures_num);
    binocle_sprite_play_animation(&elves[i].sprite, "elfWalk", false);
  }

  // Create the spawners
  binocle_material spawner_material = binocle_material_new();
  spawner_material.texture = &atlas_texture;
  spawner_material.shader = &default_shader;
  for (int i = 0 ; i < MAX_SPAWNERS ; i++) {
    //spawners[i].entity = entity_new();
    spawners[i].entity.hei = GRID;
    spawners[i].entity.rot = 0;
    spawners[i].entity.sprite = binocle_sprite_from_material(&spawner_material);
    spawners[i].entity.sprite.subtexture = atlas_subtextures[9+i];
    spawners[i].entity.sprite.origin.x = 0.5f * spawners[i].entity.sprite.subtexture.rect.max.x;
    spawners[i].entity.sprite.origin.y = 0.0f * spawners[i].entity.sprite.subtexture.rect.max.y;
    spawners[i].entity.dx = 0;
    spawners[i].entity.dy = 0;
    spawners[i].entity.xr = 0.5f;
    spawners[i].entity.yr = 1.0f;
    spawners[i].entity.cx = 0;
    spawners[i].entity.cy = 0;
    spawners[i].entity.frict = 0.8f;
    spawners[i].entity.has_gravity = true;
    spawners[i].entity.dir = 1;
    spawners[i].entity.scale.x = 1;
    spawners[i].entity.scale.y = 1;
  }
  
  
  // Create the tileset
  sprintf(filename, "%s%s", binocle_data_dir, "tiles.png");
  binocle_image tiles_image = binocle_image_load(filename);
  tiles_texture = binocle_texture_from_image(tiles_image);
  binocle_material tileset_material = binocle_material_new();
  tileset_material.texture = &tiles_texture;
  tileset_material.shader = &default_shader;
  for (int i = 0 ; i < 256 ; i++) {
    tileset[i].gid = i;
    tileset[i].sprite = binocle_sprite_from_material(&tileset_material);
    tileset[i].sprite.subtexture = binocle_subtexture_with_texture(&tiles_texture, 32*i, 0, 32, 32);
  }

  // Create the star
  binocle_material star_material = binocle_material_new();
  star_material.texture = &atlas_texture;
  star_material.shader = &default_shader;
  star_sprite = binocle_sprite_from_material(&star_material);
  star_sprite.subtexture = atlas_subtextures[23];
  star_sprite.origin.x = 0.5f * star_sprite.subtexture.rect.max.x;
  star_sprite.origin.y = 0.5f * star_sprite.subtexture.rect.max.y;

  // Create the cloud
  binocle_material cloud_material = binocle_material_new();
  cloud_material.texture = &atlas_texture;
  cloud_material.shader = &default_shader;
  cloud_sprite = binocle_sprite_from_material(&cloud_material);
  cloud_sprite.subtexture = atlas_subtextures[24];
  cloud_sprite.origin.x = 0.5f * cloud_sprite.subtexture.rect.max.x;
  cloud_sprite.origin.y = 0.5f * cloud_sprite.subtexture.rect.max.y;

  // Create the box that's being thrown in the sled
  binocle_material box_material = binocle_material_new();
  box_material.texture = &atlas_texture;
  box_material.shader = &default_shader;
  box_sprite = binocle_sprite_from_material(&box_material);
  box_sprite.subtexture = atlas_subtextures[2];
  box_sprite.origin.x = 0.5f * box_sprite.subtexture.rect.max.x;
  box_sprite.origin.y = 0.5f * box_sprite.subtexture.rect.max.y;

  // Create the barrels (pooling)
  binocle_material barrels_material = binocle_material_new();
  barrels_material.texture = &atlas_texture;
  barrels_material.shader = &default_shader;
  for (int i = 0 ; i < MAX_BARRELS ; i++) {
    //barrels[i].entity = entity_new();
    barrels[i].entity.hei = GRID;
    barrels[i].entity.rot = 0;
    barrels[i].entity.sprite = binocle_sprite_from_material(&barrels_material);
    barrels[i].entity.sprite.subtexture = atlas_subtextures[1];
    barrels[i].entity.sprite.origin.x = 0.5f * barrels[i].entity.sprite.subtexture.rect.max.x;
    barrels[i].entity.sprite.origin.y = 0.0f * barrels[i].entity.sprite.subtexture.rect.max.y;
    barrels[i].entity.dx = 0;
    barrels[i].entity.dy = 0;
    barrels[i].entity.xr = 0.5f;
    barrels[i].entity.yr = 1.0f;
    barrels[i].entity.cx = 0;
    barrels[i].entity.cy = 0;
    barrels[i].entity.frict = 0.8f;
    barrels[i].entity.has_gravity = true;
    barrels[i].entity.dir = 1;

    binocle_sprite_create_animation(&barrels[i].entity.sprite, "barrelRoll", "tiles_34.png,tiles_35.png,tiles_36.png,tiles_37.png", "0-3:0.3", true, atlas_subtextures, atlas_subtextures_num);
  }

  init_fonts();

  testRect.min.x = 0;
  testRect.min.y = 0;
  testRect.max.x = design_width;
  testRect.max.y = design_height;

  load_tilemap();

  gd = binocle_gd_new();
  binocle_gd_init(&gd);

  // Create the GUI render target
  ui_buffer = binocle_gd_create_render_target(design_width, design_height, false, GL_RGBA);

  init_gui();

  // Audio has some issues with emscripten at the moment
//#if !defined __EMSCRIPTEN__
  audio = binocle_audio_new();
  sprintf(filename, "%s%s", binocle_data_dir, "maintheme.ogg");
  music = binocle_audio_load_music(&audio, filename);
  binocle_audio_play_music(&audio, music, true);
  binocle_audio_set_music_volume(64);

  sprintf(filename, "%s%s", binocle_data_dir, "xmas.ogg");
  game_music = binocle_audio_load_music(&audio, filename);

  sprintf(filename, "%s%s", binocle_data_dir, "santa_jump.ogg");
  sfx_santa_jump = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_santa_jump)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "santa_freeze.ogg");
  sfx_santa_freeze = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_santa_freeze)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "santa_pickup.ogg");
  sfx_santa_pickup = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_santa_pickup)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "witch_laugh.ogg");
  sfx_witch_laugh = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_witch_laugh)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "elf_freeze.ogg");
  sfx_elf_freeze = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_elf_freeze)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "elf_pickup.ogg");
  sfx_elf_pickup = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_elf_pickup)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "elf_throw.ogg");
  sfx_elf_throw = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_elf_throw)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "go.ogg");
  sfx_go = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_go)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "you_win.ogg");
  sfx_level_completed = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_level_completed)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "cd_5.ogg");
  sfx_cd_5 = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_cd_5)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "cd_4.ogg");
  sfx_cd_4 = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_cd_4)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "cd_3.ogg");
  sfx_cd_3 = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_cd_3)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "cd_2.ogg");
  sfx_cd_2 = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_cd_2)) {
    binocle_log_error("Error loading sound %s", filename);
  }
  sprintf(filename, "%s%s", binocle_data_dir, "cd_1.ogg");
  sfx_cd_1 = binocle_sound_new();
  if (!binocle_audio_load_sound(&audio, filename, &sfx_cd_1)) {
    binocle_log_error("Error loading sound %s", filename);
  }
//#endif

  // Create the main render target (screen)
  screen_render_target = binocle_gd_create_render_target(
    design_width, design_height, false, GL_RGBA);

#ifdef GAMELOOP
  binocle_game_run(window, input);
#else
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(main_loop, 0, 1);
#else
  while (!input.quit_requested) {
    main_loop();
  }
#endif
  binocle_log_info("Quit requested");
#endif
  destroy_fonts();
  binocle_audio_destroy(&audio);
  binocle_sdl_exit();

  return 0;
}
