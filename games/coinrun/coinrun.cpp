#include "../../cenv/cenv.h"

#include <cmath>
#include <iostream>

#include "tilemap.h"
#include "common_systems.h"

const int version = 100;
const bool show_log = false;

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

const int window_width = 800;
const int window_height = 800;

const float game_zoom = 0.35f; // Base game zoom level

std::mt19937 rng;

SDL_Surface* window_target; // Main render window
SDL_Surface* obs_target; // Observation target
SDL_Renderer* window_renderer;
SDL_Renderer* obs_renderer;

// Masking for SDL surfaces
uint32_t rmask, gmask, bmask, amask;

float dt = 1.0f / 20.0f; // 20 fps

// Systems
std::shared_ptr<System_Sprite_Render> sprite_render;
std::shared_ptr<System_Tilemap> tilemap;
std::shared_ptr<System_Mob_AI> mob_ai;
std::shared_ptr<System_Hazard> hazard;
std::shared_ptr<System_Goal> goal;
std::shared_ptr<System_Agent> agent;
std::shared_ptr<System_Particles> particles;

System_Tilemap::Config tilemap_config;
int current_map_theme = 0;

// Big list of different background images
std::vector<std::string> background_names {
    "assets/platform_backgrounds/alien_bg.png",
    "assets/platform_backgrounds/another_world_bg.png",
    "assets/platform_backgrounds/back_cave.png",
    "assets/platform_backgrounds/caverns.png",
    "assets/platform_backgrounds/cyberpunk_bg.png",
    "assets/platform_backgrounds/parallax_forest.png",
    "assets/platform_backgrounds/scifi_bg.png",
    "assets/platform_backgrounds/scifi2_bg.png",
    "assets/platform_backgrounds/living_tissue_bg.png",
    "assets/platform_backgrounds/airadventurelevel1.png",
    "assets/platform_backgrounds/airadventurelevel2.png",
    "assets/platform_backgrounds/airadventurelevel3.png",
    "assets/platform_backgrounds/airadventurelevel4.png",
    "assets/platform_backgrounds/cave_background.png",
    "assets/platform_backgrounds/blue_desert.png",
    "assets/platform_backgrounds/blue_grass.png",
    "assets/platform_backgrounds/blue_land.png",
    "assets/platform_backgrounds/blue_shroom.png",
    "assets/platform_backgrounds/colored_desert.png",
    "assets/platform_backgrounds/colored_grass.png",
    "assets/platform_backgrounds/colored_land.png",
    "assets/platform_backgrounds/colored_shroom.png",
    "assets/platform_backgrounds/landscape1.png",
    "assets/platform_backgrounds/landscape2.png",
    "assets/platform_backgrounds/landscape3.png",
    "assets/platform_backgrounds/landscape4.png",
    "assets/platform_backgrounds/battleback1.png",
    "assets/platform_backgrounds/battleback2.png",
    "assets/platform_backgrounds/battleback3.png",
    "assets/platform_backgrounds/battleback4.png",
    "assets/platform_backgrounds/battleback5.png",
    "assets/platform_backgrounds/battleback6.png",
    "assets/platform_backgrounds/battleback7.png",
    "assets/platform_backgrounds/battleback8.png",
    "assets/platform_backgrounds/battleback9.png",
    "assets/platform_backgrounds/battleback10.png",
    "assets/platform_backgrounds/sunrise.png",
    "assets/platform_backgrounds_2/beach1.png",
    "assets/platform_backgrounds_2/beach2.png",
    "assets/platform_backgrounds_2/beach3.png",
    "assets/platform_backgrounds_2/beach4.png",
    "assets/platform_backgrounds_2/fantasy1.png",
    "assets/platform_backgrounds_2/fantasy2.png",
    "assets/platform_backgrounds_2/fantasy3.png",
    "assets/platform_backgrounds_2/fantasy4.png",
    "assets/platform_backgrounds_2/candy1.png",
    "assets/platform_backgrounds_2/candy2.png",
    "assets/platform_backgrounds_2/candy3.png",
    "assets/platform_backgrounds_2/candy4.png"
};

std::vector<Asset_Texture> background_textures;

int current_background_index = 0;
float current_background_offset_x = 0.0f;

int current_agent_theme = 0;

// Forward declarations
void render_game(bool is_obs);
void reset();

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
    render_data.value_buffer_height = window_height;
    render_data.value_buffer_width = window_width;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(window_width * window_height * 3 * sizeof(uint8_t));

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

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_Init(SDL_INIT_VIDEO);

    IMG_Init(IMG_INIT_PNG);

    window_target = SDL_CreateRGBSurface(0, window_width, window_height, 32, rmask, gmask, bmask, amask);
    obs_target = SDL_CreateRGBSurface(0, obs_width, obs_height, 32, rmask, gmask, bmask, amask);

    window_renderer = SDL_CreateSoftwareRenderer(window_target);
    obs_renderer = SDL_CreateSoftwareRenderer(obs_target);

    gr.window_renderer = window_renderer;
    gr.obs_renderer = obs_renderer;

    // Seed RNG
    rng.seed(seed);

    // Register components
    c.register_component<Component_Transform>();
    c.register_component<Component_Collision>();
    c.register_component<Component_Dynamics>();
    c.register_component<Component_Sprite>();
    c.register_component<Component_Animation>();
    c.register_component<Component_Hazard>();
    c.register_component<Component_Goal>();
    c.register_component<Component_Mob_AI>();
    c.register_component<Component_Agent>(); // Player
    c.register_component<Component_Particles>();

    // Sprite rendering system
    sprite_render = c.register_system<System_Sprite_Render>();
    Signature sprite_render_signature;
    sprite_render_signature.set(c.get_component_type<Component_Sprite>()); // Operate only on sprites
    c.set_system_signature<System_Sprite_Render>(sprite_render_signature);

    // Tile map setup
    tilemap = c.register_system<System_Tilemap>();
    Signature tilemap_signature{ 0 }; // Operates on nothing
    c.set_system_signature<System_Tilemap>(tilemap_signature);

    tilemap->init();

    // Mob AI setup
    mob_ai = c.register_system<System_Mob_AI>();
    Signature mob_ai_signature;
    mob_ai_signature.set(c.get_component_type<Component_Mob_AI>()); // Operate only on mobs
    c.set_system_signature<System_Mob_AI>(mob_ai_signature);

    // Hazard system setup
    hazard = c.register_system<System_Hazard>();
    Signature hazard_signature;
    hazard_signature.set(c.get_component_type<Component_Hazard>()); // Operate only on hazards
    c.set_system_signature<System_Hazard>(hazard_signature);

    // Goal system setup
    goal = c.register_system<System_Goal>();
    Signature goal_signature;
    goal_signature.set(c.get_component_type<Component_Goal>()); // Operate only on goals
    c.set_system_signature<System_Goal>(goal_signature);

    // Agent system setup
    agent = c.register_system<System_Agent>();
    Signature agent_signature;
    agent_signature.set(c.get_component_type<Component_Agent>()); // Operate only on mobs
    c.set_system_signature<System_Agent>(agent_signature);

    agent->init();

    // Particle system setup
    particles = c.register_system<System_Particles>();
    Signature particles_signature;
    particles_signature.set(c.get_component_type<Component_Particles>()); // Operate only on particles
    c.set_system_signature<System_Particles>(particles_signature);

    particles->init();

    // Load backgrounds
    background_textures.resize(background_names.size());

    for (int i = 0; i < background_names.size(); i++)
        background_textures[i].load(background_names[i]);

    // Reset spawns entities while generating map
    reset();

    return 0; // No error
}

int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size) {
    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            rng.seed(options[i].value.i);
        }
    }

    reset();

    render_game(true);

    // Grab observation
    SDL_LockSurface(obs_target);

    uint8_t* pixels = (uint8_t*)obs_target->pixels;

    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 4 * (y + obs_height * x)];
        }

    SDL_UnlockSurface(obs_target);

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    // Render and grab pixels
    render_game(true);

    // Grab observation
    SDL_LockSurface(obs_target);

    uint8_t* pixels = (uint8_t*)obs_target->pixels;

    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 4 * (y + obs_height * x)];
        }

    SDL_UnlockSurface(obs_target);

    int action = 0;

    // Parse actions
    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);

        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            assert(actions[i].value_buffer_size == 1);

            action = actions[i].value_buffer.i[0];
        }
    }

    // Update systems
    mob_ai->update(dt);
    std::pair<bool, bool> result = agent->update(dt, hazard, goal, action);
    particles->update(dt);
    sprite_render->update(dt);

    step_data.reward.f = result.second * 10.0f;

    step_data.terminated = !result.first || result.second;
    step_data.truncated = false;

    return 0; // No error
}

int32_t cenv_render() {
    render_game(false);

    // Grab pixels
    SDL_LockSurface(window_target);

    uint8_t* pixels = (uint8_t*)window_target->pixels;

    for (int x = 0; x < window_width; x++)
        for (int y = 0; y < window_height; y++) {
            render_data.value_buffer.b[0 + 3 * (y + window_height * x)] = pixels[0 + 4 * (y + window_height * x)];
            render_data.value_buffer.b[1 + 3 * (y + window_height * x)] = pixels[1 + 4 * (y + window_height * x)];
            render_data.value_buffer.b[2 + 3 * (y + window_height * x)] = pixels[2 + 4 * (y + window_height * x)];
        }

    SDL_UnlockSurface(window_target);

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

    SDL_DestroyRenderer(window_renderer);
    SDL_DestroyRenderer(obs_renderer);

    SDL_FreeSurface(window_target);
    SDL_FreeSurface(obs_target);
}

// Rendering
void render_game(bool is_obs) {
    // If obs, set render to obs target
    gr.rendering_obs = is_obs;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());
    SDL_SetRenderDrawColor(gr.get_renderer(), 255, 255, 255, 255);

    int width = is_obs ? obs_width : window_width;
    int height = is_obs ? obs_height : window_height;

    gr.camera_scale = game_zoom * static_cast<float>(width) / static_cast<float>(obs_width);
    gr.camera_size = (Vector2){ static_cast<float>(width), static_cast<float>(height) };

    // Draw background image
    Asset_Texture* background = &background_textures[current_background_index];

    float background_aspect = static_cast<float>(background->width) / static_cast<float>(background->height);
    float extra_width = background_aspect - 1.0f; // 1 for game world aspect, which is 64x64 tiles
    
    gr.render_texture(background, Vector2{ -current_background_offset_x * extra_width, 0.0f }, 64.0f * unit_to_pixels / background->height);

    sprite_render->render(negative_z);
    tilemap->render(current_map_theme);
    particles->render();
    sprite_render->render(positive_z);
    agent->render(current_agent_theme);
}

void reset() {
    c.clear_entities();

    tilemap->regenerate(rng, tilemap_config);

    // Determine background (themeing)
    std::uniform_int_distribution<int> background_dist(0, background_textures.size() - 1);

    current_background_index = background_dist(rng);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    current_background_offset_x = dist01(rng);

    // Spawn the player (agent)
    Entity e = c.create_entity();

    Vector2 pos{ 1.5f, tilemap->get_height() - 1 - 1.0f };

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -1.0f, 1.0f, 1.0f } });
    c.add_component(e, Component_Dynamics{});
    c.add_component(e, Component_Agent{});

    // Determine themes
    std::uniform_int_distribution<int> agent_theme_dist(0, agent_themes.size() - 1);

    current_agent_theme = agent_theme_dist(rng);

    std::uniform_int_distribution<int> map_theme_dist(0, wall_themes.size() - 1);

    current_map_theme = map_theme_dist(rng);
}
