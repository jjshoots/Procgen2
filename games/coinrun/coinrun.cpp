#include "../../cenv/cenv.h"

#include <raylib.h>
#include <rlgl.h>

#include <cmath>
#include <iostream>

#include "tilemap.h"
#include "common_systems.h"

const int version = 100;
const bool show_log = true;

// ---------------------- CEnv Interface ----------------------

// Define globals from cenv
cenv_make_data make_data;
cenv_reset_data reset_data;
cenv_step_data step_data;
cenv_render_data render_data;

// Shared value between different datas (optional)
cenv_key_value observation;

// ---------------------- Game ----------------------

const int obs_width = 64;
const int obs_height = 64;
const int num_actions = 15;

const int screen_width = 800;
const int screen_height = 800;

std::mt19937 rng;
Camera2D camera;

// Systems
std::shared_ptr<System_Sprite_Render> sprite_render;
std::shared_ptr<System_Tilemap> tilemap;

System_Tilemap::Config tilemap_config;

// Forward declarations
void render_game();
void copy_render_to_obs();
void reset();

void handle_log_do_nothing(int logLevel, const char *text, va_list args) {
    // Do nothing
}

int32_t cenv_get_env_version() {
    return version;
}

int32_t cenv_make(const char* render_mode, cenv_option* options, int32_t options_size) {
    // ---------------------- CEnv Interface ----------------------
    
    // Allocate make data
    make_data.observation_spaces_size = 1;
    make_data.observation_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.observation_spaces[0].key = "screen";
    make_data.observation_spaces[0].value_type = CENV_SPACE_TYPE_BOX;
    make_data.observation_spaces[0].value_buffer_size = 2; // Low and high

    make_data.observation_spaces[0].value_buffer.f = (float*)malloc(make_data.observation_spaces[0].value_buffer_size * sizeof(float));

    // Low and high
    make_data.observation_spaces[0].value_buffer.f[0] = 0.0f;
    make_data.observation_spaces[0].value_buffer.f[1] = 255.0f;

    make_data.action_spaces_size = 1;
    make_data.action_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.action_spaces[0].key = "action";
    make_data.action_spaces[0].value_type = CENV_SPACE_TYPE_MULTI_DISCRETE;
    make_data.action_spaces[0].value_buffer_size = 1;

    make_data.action_spaces[0].value_buffer.i = (int32_t*)malloc(sizeof(int32_t));
    make_data.action_spaces[0].value_buffer.i[0] = num_actions;

    // Allocate observations once and re-use (doesn't resize dynamically)
    observation.key = "screen";
    observation.value_type = CENV_VALUE_TYPE_BYTE;
    observation.value_buffer_size = obs_width * obs_height * 3;
    observation.value_buffer.b = (uint8_t*)malloc(obs_width * obs_height * 3 * sizeof(uint8_t));

    // Reset data
    reset_data.observations_size = 1;
    reset_data.observations = &observation;
    reset_data.infos_size = 0;
    reset_data.infos = NULL;

    // Step data
    step_data.observations_size = 1;
    step_data.observations = &observation;
    step_data.reward.f = 0.0f;
    step_data.terminated = false;
    step_data.truncated = false;
    step_data.infos_size = 0;
    step_data.infos = NULL;

    // Frame
    render_data.value_type = CENV_VALUE_TYPE_BYTE;
    render_data.value_buffer_height = screen_height;
    render_data.value_buffer_width = screen_width;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(screen_width * screen_height * 3 * sizeof(uint8_t));

    unsigned int seed = time(nullptr);

    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            seed = options[i].value.i;
        }
    }

    // ---------------------- Game ----------------------
    
    if (!show_log)
        SetTraceLogCallback(&handle_log_do_nothing);

    // Game
    InitWindow(screen_width, screen_height, "CoinRun");

    SetTargetFPS(0); // Uncapped

    // Seed RNG
    rng.seed(seed);

    // Initialize camera
    camera = { 0 };
    camera.zoom = 1.0f;
    camera.offset = (Vector2){ screen_width * 0.5f, screen_height * 0.5f };

    std::cout << "Registering components..." << std::endl;

    // Register components
    c.register_component<Component_Transform>();
    c.register_component<Component_Collision>();
    c.register_component<Component_Dynamics>();
    c.register_component<Component_Sprite>();
    c.register_component<Component_Hazard>();
    c.register_component<Component_Goal>();
    c.register_component<Component_Sweeper>();

    // Sprite rendering system
    sprite_render = c.register_system<System_Sprite_Render>();
    Signature sprite_render_signature;
    sprite_render_signature.set(c.get_component_type<Component_Sprite>());
    c.set_system_signature<System_Sprite_Render>(sprite_render_signature);

    // Tile map setup
    tilemap = c.register_system<System_Tilemap>();
    Signature tilemap_signature{ 0 }; // Operates on nothing
    c.set_system_signature<System_Tilemap>(tilemap_signature);

    tilemap->init();
    tilemap->regenerate(rng, tilemap_config);

    return 0; // No error
}

int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size) {
    reset();

    render_game();

    copy_render_to_obs();

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    render_game();

    copy_render_to_obs();

    float speed = 20.0f;

    if (IsKeyDown(KEY_A))
        camera.target.x -= speed;
    else if (IsKeyDown(KEY_D))
        camera.target.x += speed;

    if (IsKeyDown(KEY_S))
        camera.target.y += speed;
    else if (IsKeyDown(KEY_W))
        camera.target.y -= speed;

    if (GetKeyPressed() == KEY_R)
        reset();

    // Set observation from last render
    step_data.reward.f = 0.0f;

    step_data.terminated = false;

    return 0; // No error
}

int32_t cenv_render() {
    // Copy pixels
    Image screen = LoadImageFromScreen();

    ImageFormat(&screen, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    uint8_t* pixels = (uint8_t*)screen.data;

    // Reformat here if needed
    for (int x = 0; x < screen_width; x++)
        for (int y = 0; y < screen_height; y++) {
            render_data.value_buffer.b[0 + 3 * (y + screen_height * x)] = pixels[0 + 3 * (y + screen_height * x)];
            render_data.value_buffer.b[1 + 3 * (y + screen_height * x)] = pixels[1 + 3 * (y + screen_height * x)];
            render_data.value_buffer.b[2 + 3 * (y + screen_height * x)] = pixels[2 + 3 * (y + screen_height * x)];
        }

    UnloadImage(screen);

    return 0; // No error
}

void cenv_close() {
    // ---------------------- CEnv Interface ----------------------
    
    // Dealloc make data
    for (int i = 0; i < make_data.observation_spaces_size; i++)
        free(make_data.observation_spaces[i].value_buffer.f);

    free(make_data.observation_spaces);

    for (int i = 0; i < make_data.action_spaces_size; i++)
        free(make_data.action_spaces[i].value_buffer.i);

    free(make_data.action_spaces);

    // Observations
    free(observation.value_buffer.b);

    // Frame
    free(render_data.value_buffer.b);
    
    // ---------------------- Game ----------------------
    
    CloseWindow();
}

// Rendering
void render_game() {
    sprite_render->update();

    // Render here
    BeginDrawing();
        BeginMode2D(camera);

            ClearBackground(BLACK);

            Rectangle camera_aabb;
            camera_aabb.x = (camera.target.x - camera.zoom * screen_width * 0.5f) * pixels_to_unit;
            camera_aabb.y = (camera.target.y - camera.zoom * screen_height * 0.5f) * pixels_to_unit;
            camera_aabb.width = screen_width * camera.zoom * pixels_to_unit;
            camera_aabb.height = screen_height * camera.zoom * pixels_to_unit;

            sprite_render->render(camera_aabb, negative_z);
            tilemap->render(camera_aabb, 0);
            sprite_render->render(camera_aabb, positive_z);

        EndMode2D();
    EndDrawing();
}

void copy_render_to_obs() {
    Image screen = LoadImageFromScreen();

    ImageFormat(&screen, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    Image obsImg = ImageCopy(screen);

    ImageResize(&obsImg, obs_width, obs_height);

    uint8_t* pixels = (uint8_t*)obsImg.data;

    // Set observation, reformat if needed
    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 3 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 3 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 3 * (y + obs_height * x)];
        }

    UnloadImage(obsImg);
    UnloadImage(screen);
}

void reset() {
    c.destroy_all_entities();

    tilemap->regenerate(rng, tilemap_config);
}
