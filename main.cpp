#include "platform.h"
#include "graphics.h"
#include "file_system.h"
#include "math2.h"
#include "memory.h"
#include "ui.h"
#include "font.h"
#include "input.h"
#include "random.h"
#include <cassert>

float quad_vertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f,
    -1.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 1.0f,

    -1.0f, -1.0f, 0.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, -1.0f, 0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f,
};

uint32_t quad_vertices_stride = sizeof(float) * 6;
uint32_t quad_vertices_count = 6;

int main(int argc, char **argv)
{
    // Set up window
    uint32_t window_width = 1200 / 2, window_height = 800 / 2;
 	Window window = platform::get_window("Physarum", window_width, window_height);
    assert(platform::is_window_valid(&window));

    // Init graphics
    graphics::init();
    graphics::init_swap_chain(&window);

    font::init();
    ui::init((float)window_width, (float)window_height);
    ui::set_input_responsive(true);

    // Create window render target
	RenderTarget render_target_window = graphics::get_render_target_window();
    assert(graphics::is_ready(&render_target_window));
    DepthBuffer depth_buffer = graphics::get_depth_buffer(window_width, window_height);
    assert(graphics::is_ready(&depth_buffer));
    graphics::set_render_targets_viewport(&render_target_window, &depth_buffer);

    // Vertex shader
    File vertex_shader_file = file_system::read_file("vertex_shader.hlsl"); 
    VertexShader vertex_shader = graphics::get_vertex_shader_from_code((char *)vertex_shader_file.data, vertex_shader_file.size);
    file_system::release_file(vertex_shader_file);
    assert(graphics::is_ready(&vertex_shader));

    // Pixel shader
    File pixel_shader_file = file_system::read_file("pixel_shader.hlsl"); 
    PixelShader pixel_shader = graphics::get_pixel_shader_from_code((char *)pixel_shader_file.data, pixel_shader_file.size);
    file_system::release_file(pixel_shader_file);
    assert(graphics::is_ready(&pixel_shader));

    // Particle system shader
    File compute_shader_file = file_system::read_file("particle_shader.hlsl");
    ComputeShader compute_shader = graphics::get_compute_shader_from_code((char *)compute_shader_file.data, compute_shader_file.size);
    file_system::release_file(compute_shader_file);
    assert(graphics::is_ready(&compute_shader));

    // Decay/diffusion shader
    File decay_compute_shader_file = file_system::read_file("decay_shader.hlsl");
    ComputeShader decay_compute_shader = graphics::get_compute_shader_from_code((char *)decay_compute_shader_file.data, decay_compute_shader_file.size);
    file_system::release_file(decay_compute_shader_file);
    assert(graphics::is_ready(&decay_compute_shader));

    // Simulation params
    uint32_t world_width = 1200, world_height = 800;
    float spawn_radius = 200.0f;
    const int NUM_PARTICLES = 50000;

    struct Config {
        float sense_spread;;
        float sense_distance;
        float turn_angle;
        float move_distance;
        float deposit_value;
        float decay_factor;
        float collision;
        float center_attraction;
    };

    Config config = {
        0.22f,
        20.0f,
        0.41f,
        1.0f,
        5.0f,
        0.32f,
        0.0f,
        0.5f,
    };
    ConstantBuffer config_buffer = graphics::get_constant_buffer(sizeof(Config));

    // Textures for the simulation
    Texture trail_tex_A = graphics::get_texture(NULL, world_width, world_height, DXGI_FORMAT_R32_FLOAT, 4);
    Texture trail_tex_B = graphics::get_texture(NULL, world_width, world_height, DXGI_FORMAT_R32_FLOAT, 4);
    Texture occ_tex = graphics::get_texture(NULL, world_width, world_height, DXGI_FORMAT_R32_UINT, 4);
    TextureSampler tex_sampler = graphics::get_texture_sampler();
    bool is_a = true;

    // Particles setup
    struct Particle {
        float x; float y; float theta; float info;
    };
    Particle *particles = memory::alloc_heap<Particle>(NUM_PARTICLES);

    auto update_particles = [&world_width, &world_height](Particle *particles, int count, float spawn_radius) {
        for (int i = 0; i < count; ++i) {
            float angle = random::uniform(0, math::PI2);
            float radius = random::uniform(0, 1);
            radius = math::sqrt(radius) * spawn_radius;
            particles[i].x = math::sin(angle) * radius + world_width / 2.0f;
            particles[i].y = math::cos(angle) * radius + world_height / 2.0f;
            particles[i].theta = random::uniform(0, math::PI2);
            particles[i].info = 0.0f;
        }
    };
    update_particles(particles, NUM_PARTICLES, spawn_radius);

    // Set up buffer containing particle data
    StructuredBuffer particles_buffer = graphics::get_structured_buffer(sizeof(Particle), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer, particles);

    Mesh quad_mesh = graphics::get_mesh(quad_vertices, quad_vertices_count, quad_vertices_stride, NULL, 0, 0);

    Timer timer = timer::get();
    timer::start(&timer);

    // Render loop
    bool is_running = true;
    bool show_ui = false;
    while(is_running)
    {
        printf("%f\n", timer::checkpoint(&timer));
        input::reset();
    
        // Event loop
        Event event;
        while(platform::get_event(&event))
        {
            input::register_event(&event);

            // Check if close button pressed
            switch(event.type)
            {
                case EventType::EXIT:
                {
                    is_running = false;
                }
                break;
            }
        }

        // React to inputs
        {
            if (input::key_pressed(KeyCode::ESC)) is_running = false; 
            if (input::key_pressed(KeyCode::F1)) show_ui = !show_ui; 
            if (input::key_pressed(KeyCode::F2))
            {
                // Reset particles + trails + occupancy map
                update_particles(particles, NUM_PARTICLES, spawn_radius);

                graphics::update_structured_buffer(&particles_buffer, particles);
                float clear_tex[4] = {0, 0, 0, 0};
                graphics_context->context->ClearUnorderedAccessViewFloat(trail_tex_A.ua_view, clear_tex);
                graphics_context->context->ClearUnorderedAccessViewFloat(trail_tex_B.ua_view, clear_tex);
                uint32_t clear_tex_uint[4] = {0, 0, 0, 0};
                graphics_context->context->ClearUnorderedAccessViewUint(occ_tex.ua_view, clear_tex_uint);
            }
        }

        // Update simulation config
        graphics::update_constant_buffer(&config_buffer, &config);
        graphics::set_constant_buffer(&config_buffer, 0);

        // Particle simulation 
        {
            graphics::set_compute_shader(&compute_shader);
            uint32_t clear_tex_uint[4] = {0, 0, 0, 0};
            float clear_tex[4] = {0, 0, 0, 0};
            graphics_context->context->ClearUnorderedAccessViewUint(occ_tex.ua_view, clear_tex_uint);
            graphics::set_texture_compute(&occ_tex, 3);
            if (is_a) {
                graphics::set_texture_compute(&trail_tex_A, 0);
            } else {
                graphics::set_texture_compute(&trail_tex_B, 0);
            }
            graphics::set_structured_buffer(&particles_buffer, 2);
            graphics::run_compute(50, 10, 10);
            graphics::unset_texture_compute(0);
            graphics::unset_texture_compute(1);
            graphics::unset_texture_compute(3);
        }

        // Decay/diffusion
        {
            graphics::set_compute_shader(&decay_compute_shader);
            if (is_a) {
                graphics::set_texture_compute(&trail_tex_A, 0);
                graphics::set_texture_compute(&trail_tex_B, 1);
            } else {
                graphics::set_texture_compute(&trail_tex_B, 0);
                graphics::set_texture_compute(&trail_tex_A, 1);
            }
            graphics::run_compute(world_width, world_height, 1);
            graphics::unset_texture_compute(0);
            graphics::unset_texture_compute(1);
        }

        // Render trail map
        {
            graphics::set_render_targets_viewport(&render_target_window, &depth_buffer);
            graphics::clear_render_target(&render_target_window, 0.1f, 0.6f, 0.1f, 1);
            graphics::clear_depth_buffer(&depth_buffer);

            graphics::set_vertex_shader(&vertex_shader);
            graphics::set_pixel_shader(&pixel_shader);
            if (is_a) {
                graphics::set_texture(&trail_tex_B, 0);
            } else {
                graphics::set_texture(&trail_tex_A, 0);
            }
            
            graphics::set_texture_sampler(&tex_sampler, 0);
            graphics::draw_mesh(&quad_mesh);

            graphics::unset_texture(0);
        }

        // UI
        if (show_ui) {
            graphics::set_render_targets_viewport(&render_target_window);

            Panel panel = ui::start_panel("", Vector2(10, 10.0f), 420.0f);

            ui::add_slider(&panel, "SENSE SPREAD", &config.sense_spread, 0.0, math::PI);
            ui::add_slider(&panel, "SENSE DISTANCE", &config.sense_distance, 0.0, 40.0);
            ui::add_slider(&panel, "TURN ANGLE", &config.turn_angle, 0.0, math::PI);
            ui::add_slider(&panel, "MOVE DISTANCE", &config.move_distance, 0.0, 20.0);
            ui::add_slider(&panel, "DEPOSIT VALUE", &config.deposit_value, 0.0, 5.0);
            ui::add_slider(&panel, "DECAY FACTOR", &config.decay_factor, 0.0, 1.0);
            ui::add_slider(&panel, "SPAWN RADIUS", &spawn_radius, 20.0f, world_height / 2.0f);
            ui::add_slider(&panel, "CENTER ATTRACTION", &config.center_attraction, 0.0, 5.0);
            bool collision = config.collision > 0.0f;
            ui::add_toggle(&panel, "COLLISION", &collision);
            config.collision = collision ? 1.0f : 0.0f;

            ui::end_panel(&panel);
            ui::end();
        }

        is_a = !is_a;
        graphics::swap_frames();
    }

    ui::release();

    graphics::release(&render_target_window);
    graphics::release(&depth_buffer);
    graphics::release(&pixel_shader);
    graphics::release(&vertex_shader);
    graphics::release(&compute_shader);
    graphics::release(&decay_compute_shader);
    graphics::release(&quad_mesh);
    graphics::release(&trail_tex_A);
    graphics::release(&trail_tex_B);
    graphics::release(&occ_tex);
    graphics::release(&tex_sampler);
    graphics::release(&particles_buffer);
    graphics::release(&config_buffer);

    //graphics::show_live_objects();

    graphics::release();

    return 0;
}