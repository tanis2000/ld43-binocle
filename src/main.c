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
#include "sys_config.h"

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

#if defined(WIN32)
#define drand48() (rand() / (RAND_MAX + 1.0))
#define srand48 srand
#endif

typedef enum game_state_t {
  GAME_STATE_MENU,
  GAME_STATE_RUN,
  GAME_STATE_GAMEOVER
} game_state_t;

struct wall {
  kmAABB2 slices_original[200];
  kmAABB2 slices[200];
} wall;

struct player_t {
  binocle_sprite sprite;
  kmVec2 pos;
  kmVec2 speed;
  float rot;
  bool dead;
};

uint32_t design_width = 240;
uint32_t design_height = 320;
binocle_window window;
binocle_input input;
binocle_viewport_adapter adapter;
binocle_camera camera;
binocle_gd gd;
binocle_bitmapfont *font = NULL;
binocle_image font_image;
binocle_texture font_texture;
binocle_material font_material;
binocle_sprite font_sprite;
kmVec2 font_sprite_pos;
char fps_buffer[10];
kmAABB2 testRect;
binocle_sprite enemy;
kmVec2 enemy_pos;
float enemy_rot = 0;
struct wall top_wall;
struct wall bottom_wall;
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
binocle_texture player_texture;
binocle_texture texture;
int num_frames = 0;
float gravity = 200.0f; //500.0f;
struct player_t player;
binocle_texture atlas_texture;
binocle_subtexture atlas_subtextures[ATLAS_MAX_SUBTEXTURES];
int atlas_subtextures_num = 0;
binocle_sprite heart_sprite;
game_state_t game_state = GAME_STATE_MENU;
bool show_menu = false;
float scroller_x = 0.0f;

// Nuklear
struct nk_context ctx;
struct nk_draw_null_texture nuklear_null;

void build_scaling_viewport(int window_width, int window_height,
                            int design_width, int design_height,
                            kmAABB2 *viewport, float *inverse_multiplier) {
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
}

void build_walls() {
  for (int i = 0; i < 200; i++) {
    bottom_wall.slices_original[i].min.x = i * 16;
    bottom_wall.slices_original[i].min.y = 0;
    bottom_wall.slices_original[i].max.x = i * 16 + 16;
    bottom_wall.slices_original[i].max.y = (float)drand48() * 50;

    top_wall.slices_original[i].min.x = i * 16;
    top_wall.slices_original[i].min.y = design_height - (float)drand48() * 50;
    top_wall.slices_original[i].max.x = i * 16 + 16;
    top_wall.slices_original[i].max.y = design_height;
  }
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
      nk_label(&ctx, "Helirun", NK_TEXT_CENTERED);
      if (nk_button_label(&ctx, "Start")) {
        player.dead = false;
        scroller_x = 0.0f;
        player.pos.x = roundf(design_width / 3.0f);
        player.pos.y = roundf(design_height / 2.0f);
        player.speed.y = 0;
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
      nk_label(&ctx, "Your score:", NK_TEXT_CENTERED);
      if (nk_button_label(&ctx, "Continue")) {
        game_state = GAME_STATE_MENU;
      }
    }
    nk_end(&ctx);
  }
}

void render_gui() {
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

  kmMat4 projectionMatrix = binocle_math_create_orthographic_matrix_off_center(0.0f, design_width, design_height, 0.0f, -1000.0f, 1000.0f);
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
}

void pass_input_to_gui(binocle_input *input) {
  nk_input_begin(&ctx);
  nk_input_motion(&ctx, input->mouseX, input->mouseY);
  nk_input_button(&ctx, NK_BUTTON_LEFT, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_LEFT]);
  nk_input_button(&ctx, NK_BUTTON_MIDDLE, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_MIDDLE]);
  nk_input_button(&ctx, NK_BUTTON_RIGHT, input->mouseX, input->mouseY, input->currentMouseButtons[MOUSE_RIGHT]);
  nk_input_end(&ctx);
}

void load_tilemap() {
  char filename[1024];
  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "tilemap.json");
  char *json = NULL;
  size_t json_length = 0;
  if (!binocle_sdl_load_text_file(filename, json, &json_length)) {
    return;
  }

  cute_tiled_map_t *map = cute_tiled_load_map_from_memory(json, json_length, 0);

  // get map width and height
  int w = map->width;
  int h = map->height;

  // loop over the map's layers
  cute_tiled_layer_t* layer = map->layers;
  while (layer)
  {
    int* data = layer->data;
    int data_count = layer->data_count;

    // do something with the tile data
    //UserFunction_HandleTiles(data, data_count);

    layer = layer->next;
  }

  cute_tiled_free_map(map);
  free(json);

}

void game_update() {
  if (player.dead) {
    game_state = GAME_STATE_GAMEOVER;
    return;
  }
  /*
  if (binocle_input_is_key_pressed(input, KEY_RIGHT) ||
      binocle_input_is_touch_down(input, 0)) {
    player_pos.x += 50 * (1.0 / window.frame_time);
  } else if (binocle_input_is_key_pressed(input, KEY_LEFT)) {
    player_pos.x -= 50 * (1.0 / window.frame_time);
  }

  if (binocle_input_is_key_pressed(input, KEY_UP) ||
      binocle_input_is_touch_down(input, 0)) {
    player_pos.y += 50 * (1.0 / window.frame_time);
  } else if (binocle_input_is_key_pressed(input, KEY_DOWN)) {
    player_pos.y -= 50 * (1.0 / window.frame_time);
  }
   */

  if (binocle_input_is_key_pressed(input, KEY_SPACE) ||
      binocle_input_is_touch_down(input, 0)) {
    player.speed.y = 160;
  }


  if (window.frame_time > 0) {
    float dt = 1.0f / window.frame_time;
    //rot += 50 * (1.0 / window.frame_time);
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

    for (int i = 0; i < 200; i++) {
      bottom_wall.slices[i].min.x = bottom_wall.slices_original[i].min.x - scroller_x;
      bottom_wall.slices[i].min.y = bottom_wall.slices_original[i].min.y;
      bottom_wall.slices[i].max.x = bottom_wall.slices_original[i].max.x - scroller_x;
      bottom_wall.slices[i].max.y = bottom_wall.slices_original[i].max.y;

      top_wall.slices[i].min.x = top_wall.slices_original[i].min.x - scroller_x;
      top_wall.slices[i].min.y = top_wall.slices_original[i].min.y;
      top_wall.slices[i].max.x = top_wall.slices_original[i].max.x - scroller_x;
      top_wall.slices[i].max.y = top_wall.slices_original[i].max.y;

      if (kmAABB2ContainsAABB(&bottom_wall.slices[i], &player_collider)) {
        player.dead = true;
        break;
      }
      if (kmAABB2ContainsAABB(&top_wall.slices[i], &player_collider)) {
        player.dead = true;
        break;
      }
    }


    enemy_rot += 50 * (1.0 / window.frame_time);
    enemy_rot = (int64_t)enemy_rot % 360;

    binocle_sprite_update(&enemy, (1.0f / window.frame_time));

    scroller_x += 20.0f * dt;

  }
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
  binocle_sprite_draw(player.sprite, &gd, (int64_t)player.pos.x, (int64_t)player.pos.y,
                      vp_design, player.rot * (float)M_PI / 180.0f, 2);

  // Enemy
  // binocle_sprite_draw(enemy, &gd, (int64_t)enemy_pos.x, (int64_t)enemy_pos.y,
  // binocle_camera_get_viewport(camera), enemy_rot * (float)M_PI / 180.0f, 2);
  binocle_sprite_draw(enemy, &gd, (int64_t)enemy_pos.x, (int64_t)enemy_pos.y,
                      vp_design, enemy_rot * (float)M_PI / 180.0f, 2);

  // Walls
  for (int i = 0; i < 200; i++) {
    // binocle_gd_draw_rect(&gd, bottom_wall.slices[i], color_grey,
    // binocle_camera_get_viewport(camera),
    // binocle_camera_get_transform_matrix(&camera)); binocle_gd_draw_rect(&gd,
    // top_wall.slices[i], color_grey, binocle_camera_get_viewport(camera),
    // binocle_camera_get_transform_matrix(&camera));
    binocle_gd_apply_shader(&gd, default_shader);
    binocle_gd_draw_rect(&gd, bottom_wall.slices[i], color_grey, vp_design,
                         identity_mat);
    binocle_gd_draw_rect(&gd, top_wall.slices[i], color_grey, vp_design,
                         identity_mat);
  }

  // Heart
  /*
  binocle_sprite_draw(heart_sprite, &gd, 20, 20,
                      vp_design, 0, 1);
                      */
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
  binocle_gd_clear(binocle_color_azure());

  binocle_gd_apply_shader(&gd, default_shader);
  // Test rect
  // binocle_gd_draw_rect(&gd, testRect, binocle_color_white(),
  // binocle_camera_get_viewport(camera),
  // binocle_camera_get_transform_matrix(&camera));
  //binocle_gd_draw_rect(&gd, testRect, binocle_color_white(), vp_design, identity_mat);

  if (game_state == GAME_STATE_RUN) {
    game_render();
  }


  // GUI
  if (show_menu) {
    draw_gui();
    render_gui();
  }

  // Score and FPS
  // binocle_bitmapfont_draw_string(font, "SCORE: 0", 32, &gd, 10,
  // window.height-36, binocle_camera_get_viewport(camera),
  // binocle_color_black(), binocle_camera_get_transform_matrix(&camera));
  binocle_bitmapfont_draw_string(font, "SCORE: 0", 32, &gd, 10,
                                 design_height - 36, vp_design,
                                 binocle_color_black(), identity_mat);
  uint64_t fps = binocle_window_get_fps(&window);
  snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %llu", fps);
  // binocle_bitmapfont_draw_string(font, fps_buffer, 32, &gd,
  // window.width-16*7, window.height-36, binocle_camera_get_viewport(camera),
  // binocle_color_black(), binocle_camera_get_transform_matrix(&camera));
  binocle_bitmapfont_draw_string(
    font, fps_buffer, 32, &gd, design_width - 16 * 7, design_height - 36,
    vp_design, binocle_color_black(), identity_mat);


  {
    kmAABB2 vp;
    float multiplier = 1;
    build_scaling_viewport(window.width, window.height, design_width,
                           design_height, &vp, &multiplier);
    kmMat4 camera_transform_mat;
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
  build_scaling_viewport(window.width, window.height, design_width,
                         design_height, &vp, &multiplier);
  binocle_gd_apply_viewport(vp);
  binocle_gd_set_uniform_float(quad_shader, "time", time);
  binocle_gd_set_uniform_float2(quad_shader, "resolution", design_width,
                                design_height);
  kmMat4 camera_transform_mat;
  kmMat4Identity(&camera_transform_mat);
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
  sprintf(font_filename, "%s%s", BINOCLE_DATA_DIR, "minecraftia.fnt");
  font = binocle_bitmapfont_from_file(font_filename, true);

  char font_image_filename[1024];
  sprintf(font_image_filename, "%s%s", BINOCLE_DATA_DIR, "minecraftia.png");
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
  color_grey = binocle_color_new(0.3, 0.3, 0.3, 1);
  // Init SDL
  binocle_sdl_init();
  // Create the window
  window = binocle_window_new(design_width, design_height, "Helirun");
  binocle_window_set_minimum_size(&window, design_width, design_height);
  // Updates the window size in case we're on mobile and getting a forced
  // dimension
  uint32_t real_width = 0;
  uint32_t real_height = 0;
  binocle_window_get_real_size(&window, &real_width, &real_height);
  if (real_width > real_height) {
    uint32_t tmp = real_width;
    real_width = real_height;
    real_height = tmp;
  }
  window.width = real_width;
  window.height = real_height;
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
  char filename[1024];
  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "heli.png");
  binocle_log_info("Loading %s", filename);
  binocle_image image = binocle_image_load(filename);
  texture = binocle_texture_from_image(image);
  binocle_shader_init_defaults();
  char vert[1024];
  sprintf(vert, "%s%s", BINOCLE_DATA_DIR, "default.vert");
  char frag[1024];
  sprintf(frag, "%s%s", BINOCLE_DATA_DIR, "default.frag");
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
  sprintf(vert, "%s%s", BINOCLE_DATA_DIR, "screen.vert");
  sprintf(frag, "%s%s", BINOCLE_DATA_DIR, "screen.frag");
  quad_shader = binocle_shader_load_from_file(vert, frag);

  // Load the UI shader
  sprintf(vert, "%s%s", BINOCLE_DATA_DIR, "default.vert");
  sprintf(frag, "%s%s", BINOCLE_DATA_DIR, "default.frag");
  ui_shader = binocle_shader_load_from_file(vert, frag);


  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "testlibgdx.png");
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

  // Load a sprite atlas
  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "testatlas.png");
  binocle_image atlas_image = binocle_image_load(filename);
  atlas_texture = binocle_texture_from_image(atlas_image);
  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "testatlas.json");
  binocle_atlas_load_texturepacker(filename, &atlas_texture, atlas_subtextures, &atlas_subtextures_num);

  // Create a heart sprite from the sprite atlas
  binocle_material heart_material = binocle_material_new();
  heart_material.texture = &atlas_texture;
  heart_material.shader = &default_shader;
  heart_sprite = binocle_sprite_from_material(&heart_material);
  heart_sprite.subtexture = atlas_subtextures[8];

  init_fonts();

  testRect.min.x = 0;
  testRect.min.y = 0;
  testRect.max.x = design_width;
  testRect.max.y = design_height;

  build_walls();

  gd = binocle_gd_new();
  binocle_gd_init(&gd);

  // Create the GUI render target
  ui_buffer = binocle_gd_create_render_target(design_width, design_height, false, GL_RGBA);

  init_gui();

  // Audio has some issues with emscripten at the moment
//#if !defined __EMSCRIPTEN__
  audio = binocle_audio_new();
  sprintf(filename, "%s%s", BINOCLE_DATA_DIR, "8bit.ogg");
  music = binocle_audio_load_music(&audio, filename);
  binocle_audio_play_music(&audio, music, true);
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
  binocle_sdl_exit();

  return 0;
}
