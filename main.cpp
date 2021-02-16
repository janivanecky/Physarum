#include "platform.h"
#include "graphics.h"
#include "file_system.h"
#include "maths.h"
#include "memory.h"
#include "ui.h"
#include "ui_draw.h"
#include "font.h"
#include "input.h"
#include <cassert>
#include <mmsystem.h>
#include <stdio.h>
#define MIDI_DEFINE
#include "midi.h"

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
    int midi_error = midi::init();
    bool use_midi = true;
    if (midi_error != MIDI_OK) {
        use_midi = false;
    }

    // Set up window
    uint32_t window_width = 1400, window_height = 800;
    HWND window = platform::get_window("Physarum", window_width, window_height);
    assert(platform::is_window_valid(window));

    // Init graphics
    graphics::init();
    graphics::init_swap_chain(window, window_width, window_height);

    ui_draw::init((float)window_width, (float)window_height);
    ui::set_input_responsive(true);

    // Create window render target
	RenderTarget render_target_window = graphics::get_render_target_window();
    assert(graphics::is_ready(&render_target_window));
    DepthBuffer depth_buffer = graphics::get_depth_buffer(window_width, window_height);
    assert(graphics::is_ready(&depth_buffer));
    graphics::set_render_targets_viewport(&render_target_window, &depth_buffer);

    // Simulation params
    uint32_t world_width = 480, world_height = 480, world_depth = 480;
    float spawn_radius = 50.0f;
    const int NUM_PARTICLES = 100000;

    // DoF rendering shader for rendering trail.
    File draw_compute_shader_file_trail = file_system::read_file("dof_shader_trail.hlsl");
    ComputeShader draw_compute_shader_trail = graphics::get_compute_shader_from_code((char *)draw_compute_shader_file_trail.data, draw_compute_shader_file_trail.size);
    file_system::release_file(draw_compute_shader_file_trail);
    assert(graphics::is_ready(&draw_compute_shader_trail));

    // DoF rendering shader for rendering individual particles.
    File draw_compute_shader_file_particle = file_system::read_file("dof_shader_particle.hlsl");
    ComputeShader draw_compute_shader_particle = graphics::get_compute_shader_from_code((char *)draw_compute_shader_file_particle.data, draw_compute_shader_file_particle.size);
    file_system::release_file(draw_compute_shader_file_particle);
    assert(graphics::is_ready(&draw_compute_shader_particle));

    // DoF rendering shader for rendering particle pairs.
    File draw_compute_shader_file_particle_pair = file_system::read_file("dof_shader_particle_pair.hlsl");
    ComputeShader draw_compute_shader_particle_pair = graphics::get_compute_shader_from_code((char *)draw_compute_shader_file_particle_pair.data, draw_compute_shader_file_particle_pair.size);
    file_system::release_file(draw_compute_shader_file_particle_pair);
    assert(graphics::is_ready(&draw_compute_shader_particle_pair));

    // Shader for blitting from uint to float texture.
    File blit_compute_shader_file = file_system::read_file("blit_shader.hlsl");
    ComputeShader blit_compute_shader = graphics::get_compute_shader_from_code((char *)blit_compute_shader_file.data, blit_compute_shader_file.size);
    file_system::release_file(blit_compute_shader_file);
    assert(graphics::is_ready(&blit_compute_shader));

    // Vertex shader
    File vertex_shader_file = file_system::read_file("vertex_shader_3d.hlsl");
    VertexShader vertex_shader = graphics::get_vertex_shader_from_code((char *)vertex_shader_file.data, vertex_shader_file.size);
    file_system::release_file(vertex_shader_file);
    assert(graphics::is_ready(&vertex_shader));

    // Pixel shader
    File pixel_shader_file = file_system::read_file("pixel_shader_3d.hlsl");
    PixelShader pixel_shader = graphics::get_pixel_shader_from_code((char *)pixel_shader_file.data, pixel_shader_file.size);
    file_system::release_file(pixel_shader_file);
    assert(graphics::is_ready(&pixel_shader));

    // Particle system shader
    File compute_shader_file = file_system::read_file("particle_shader_3d.hlsl");
    ComputeShader compute_shader = graphics::get_compute_shader_from_code((char *)compute_shader_file.data, compute_shader_file.size);
    file_system::release_file(compute_shader_file);
    assert(graphics::is_ready(&compute_shader));

    // Decay/diffusion shader
    File decay_compute_shader_file = file_system::read_file("decay_shader_3d.hlsl");
    ComputeShader decay_compute_shader = graphics::get_compute_shader_from_code((char *)decay_compute_shader_file.data, decay_compute_shader_file.size);
    file_system::release_file(decay_compute_shader_file);
    assert(graphics::is_ready(&decay_compute_shader));

    // Vertex shader for displaying textures.
    vertex_shader_file = file_system::read_file("vertex_shader.hlsl");
    VertexShader vertex_shader_2d = graphics::get_vertex_shader_from_code((char *)vertex_shader_file.data, vertex_shader_file.size);
    file_system::release_file(vertex_shader_file);
    assert(graphics::is_ready(&vertex_shader_2d));

    // Pixel shader for displaying textures.
    pixel_shader_file = file_system::read_file("pixel_shader.hlsl");
    PixelShader pixel_shader_2d = graphics::get_pixel_shader_from_code((char *)pixel_shader_file.data, pixel_shader_file.size);
    file_system::release_file(pixel_shader_file);
    assert(graphics::is_ready(&pixel_shader_2d));

    // Textures for the simulation
    Texture3D trail_tex_A = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R16_FLOAT, 2);
    Texture3D trail_tex_B = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R16_FLOAT, 2);
    Texture3D occ_tex = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R32_UINT, 4);
    Texture2D display_tex = graphics::get_texture2D(NULL, window_width, window_height, DXGI_FORMAT_R32_FLOAT, 4);
    Texture2D display_tex_uint = graphics::get_texture2D(NULL, window_width, window_height, DXGI_FORMAT_R32_UINT, 4);

	graphics::set_blend_state(BlendType::ALPHA);

    // Particles setup
    struct Particle3D {
        float x; float y; float z; float phi; float theta; float pad1; float pad2; float pad3;
    };
    Particle3D *particles = memory::alloc_heap<Particle3D>(NUM_PARTICLES);
    float *particles_x = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_y = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_z = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_phi = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_theta = memory::alloc_heap<float>(NUM_PARTICLES);
    unsigned int *particles_pair = memory::alloc_heap<unsigned int>(NUM_PARTICLES);

    auto update_particles = [&world_width, &world_height, &world_depth](float *px, float *py, float *pz, float *pp, float *pt, unsigned int *pb, int count, float spawn_radius) {
        for (int i = 0; i < count; ++i) {
            float phi = math::random_uniform(0, math::PI2);
            float theta = math::acos(2 * math::random_uniform(0, 1.0) - 1);
            float radius = math::random_uniform(0, 1);
            radius = math::pow(radius, 1.0f/3.0f) * spawn_radius;
            px[i] = math::sin(phi) * math::sin(theta) * radius + world_width / 2.0f;
            py[i] = math::cos(theta) * radius + world_height / 2.0f;
            pz[i] = math::cos(phi) * math::sin(theta) * radius + world_depth / 2.0f;
            pp[i] = math::acos(2 * math::random_uniform(0, 1.0) - 1);
            pt[i] = math::random_uniform(0, math::PI2);
            pb[i] = 100000000; // particle ID 100 million means no pair.
        }
    };
    update_particles(particles_x, particles_y, particles_z, particles_phi, particles_theta, particles_pair, NUM_PARTICLES, spawn_radius);

    // Set up buffer containing particle data
    StructuredBuffer particles_buffer_x = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_x, particles_x);
    StructuredBuffer particles_buffer_y = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_y, particles_y);
    StructuredBuffer particles_buffer_z = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_z, particles_z);
    StructuredBuffer particles_buffer_phi = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_phi, particles_phi);
    StructuredBuffer particles_buffer_theta = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_theta, particles_theta);
    StructuredBuffer particles_buffer_pair = graphics::get_structured_buffer(sizeof(unsigned int), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_pair, particles_pair);

    // Set up 3D texture quad mesh.
    float super_quad_vertices_template[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f,

        -1.0f, -1.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
    };

    float *super_quad_vertices = memory::alloc_heap<float>(sizeof(super_quad_vertices_template) / sizeof(float) * world_depth);
    float z_step = 2.0f / (float)world_depth;
    for (int z = 0; z < int(world_depth); z++) {
        float *dst = super_quad_vertices + sizeof(super_quad_vertices_template) / sizeof(float) * z;
        float *src = super_quad_vertices_template;
        memcpy(dst, src, sizeof(super_quad_vertices_template));
        dst[2] = -1.0f + z_step * z;
        dst[9] = -1.0f + z_step * z;
        dst[16] = -1.0f + z_step * z;
        dst[23] = -1.0f + z_step * z;
        dst[30] = -1.0f + z_step * z;
        dst[37] = -1.0f + z_step * z;
        dst[6] =  1.0f - z_step * z * 0.5f;
        dst[13] = 1.0f - z_step * z * 0.5f;
        dst[20] = 1.0f - z_step * z * 0.5f;
        dst[27] = 1.0f - z_step * z * 0.5f;
        dst[34] = 1.0f - z_step * z * 0.5f;
        dst[41] = 1.0f - z_step * z * 0.5f;
    }
    int super_quad_vertices_stride = sizeof(float) * 7;
    Mesh super_quad_mesh = graphics::get_mesh(super_quad_vertices, quad_vertices_count * world_depth, super_quad_vertices_stride, NULL, 0, 0);
    Mesh quad_mesh = graphics::get_mesh(quad_vertices, quad_vertices_count, quad_vertices_stride, NULL, 0, 0);

    // Set up 3D rendering matrices buffer.
    float aspect_ratio = float(window_width) / float(window_height);
    Matrix4x4 projection_matrix = math::get_perspective_projection_dx_rh(math::deg2rad(60.0f), aspect_ratio, 0.01f, 10.0f);
    float azimuth = 0.0f;
    float polar = math::PIHALF;
    float radius = 2.0f;
    Vector3 eye_pos = Vector3(math::cos(azimuth) * math::sin(polar), math::cos(polar), math::sin(azimuth) * math::sin(polar)) * radius;
    Matrix4x4 view_matrix = math::get_look_at(eye_pos, Vector3(0,0,0), Vector3(0,1,0));

    struct RenderingSettings {
        Matrix4x4 projection;
        Matrix4x4 view;
        Matrix4x4 model;

        int texcoord_map;
        int show_grid;
        float dof_size;
        float dof_distribution;

        float focal_distance;
        float focal_depth;
        int iterations;
        float break_distance;

        float world_width;
        float world_height;
        float world_depth;
        float screen_width;

        float screen_height;
        float sample_weight;
        int filler1;
        int filler2;
    };

    RenderingSettings rendering_settings = {};
    rendering_settings.projection = projection_matrix;
    rendering_settings.view = view_matrix;
    rendering_settings.model = math::get_identity();
    rendering_settings.dof_size = 0.1f;
    rendering_settings.dof_distribution = 1.0f;
    rendering_settings.focal_distance = 2.0f;
    rendering_settings.focal_depth = 1.0f;
    rendering_settings.iterations = 1;
    rendering_settings.break_distance = 10;
    rendering_settings.world_width = world_width;
    rendering_settings.world_height = world_height;
    rendering_settings.world_depth = world_depth;
    rendering_settings.screen_width = window_width;
    rendering_settings.screen_height = window_height;
    rendering_settings.sample_weight = 1.0f / 32.0f;

    ConstantBuffer rendering_settings_buffer = graphics::get_constant_buffer(sizeof(RenderingSettings));
    graphics::update_constant_buffer(&rendering_settings_buffer, &rendering_settings);
    graphics::set_constant_buffer(&rendering_settings_buffer, 4);


    TextureSampler tex_sampler = graphics::get_texture_sampler();
    bool is_a = true;

    struct Config {
        float sense_spread;;
        float sense_distance;
        float turn_angle;
        float move_distance;

        float deposit_value;
        float decay_factor;
        float collision;
        float center_attraction;

        int world_width;
        int world_height;
        int world_depth;
        float move_sense_coef;

        float move_sense_offset;
        int filler1;
        int filler2;
        int filler3;
    };

    Config config = {
        0.48f,
        23.0f,
        0.63f,
        2.77f,
        5.0f,
        0.32f,
        0.0f,
        1.0f,
        int(world_width),
        int(world_height),
        int(world_depth),
        0.0f,
        1.0f,
    };
    ConstantBuffer config_buffer = graphics::get_constant_buffer(sizeof(Config));

    Timer timer = timer::get();
    timer::start(&timer);

    // Render loop
    bool is_running = true;
    bool show_ui = false;
    bool run_mold = true;
    bool turning_camera = false;
    bool render_dof = false;
    enum DofType {
        TRAIL,
        PARTICLES,
        PARTICLE_PAIRS
    };
    DofType dof_type = DofType::TRAIL;
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
            if(!ui::is_registering_input()) {
                radius -= input::mouse_scroll_delta() * 0.1f;

                if (input::mouse_left_button_down()) {
                    float dmx = input::mouse_delta_position_x();
                    float dmy = input::mouse_delta_position_y();
                    azimuth += dmx * 0.003f;
                    polar -= dmy * 0.003f;
                    polar = math::clamp(polar, 0.02f, math::PI);
                }
            }
            if (turning_camera) {
                azimuth += 0.01f;
            }
            Vector3 eye_pos = Vector3(
                math::cos(azimuth) * math::sin(polar),
                math::cos(polar),
                math::sin(azimuth) * math::sin(polar)
            ) * radius;
            rendering_settings.view = math::get_look_at(eye_pos, Vector3(0,0,0), Vector3(0,1,0));;

            if (input::key_pressed(KeyCode::F4)) rendering_settings.show_grid = !rendering_settings.show_grid;
            if (input::key_pressed(KeyCode::F5)) turning_camera = !turning_camera;
            if (input::key_pressed(KeyCode::ESC)) is_running = false;
            if (input::key_pressed(KeyCode::F1)) show_ui = !show_ui;
            if (input::key_pressed(KeyCode::F3)) run_mold = !run_mold;
            if (input::key_pressed(KeyCode::F9)) render_dof = !render_dof;
            if (input::key_pressed(KeyCode::F2)) {
                // Reset particles + trails + occupancy map
                update_particles(particles_x, particles_y, particles_z, particles_phi, particles_theta, particles_pair, NUM_PARTICLES, spawn_radius);
                graphics::update_structured_buffer(&particles_buffer_z, particles_z);
                graphics::update_structured_buffer(&particles_buffer_phi, particles_phi);
                graphics::update_structured_buffer(&particles_buffer_x, particles_x);
                graphics::update_structured_buffer(&particles_buffer_y, particles_y);
                graphics::update_structured_buffer(&particles_buffer_theta, particles_theta);
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
        if (run_mold)
        {
            is_a = !is_a;
            graphics::set_compute_shader(&compute_shader);
            uint32_t clear_tex_uint[4] = {0, 0, 0, 0};
            graphics_context->context->ClearUnorderedAccessViewUint(occ_tex.ua_view, clear_tex_uint);
            graphics::set_texture_compute(&occ_tex, 1);
            if (is_a) {
                graphics::set_texture_compute(&trail_tex_A, 0);
            } else {
                graphics::set_texture_compute(&trail_tex_B, 0);
            }
            graphics::set_structured_buffer(&particles_buffer_x, 2);
            graphics::set_structured_buffer(&particles_buffer_y, 3);
            graphics::set_structured_buffer(&particles_buffer_z, 4);
            graphics::set_structured_buffer(&particles_buffer_phi, 5);
            graphics::set_structured_buffer(&particles_buffer_theta, 6);
            graphics::run_compute(10, 10, 1);
            graphics::unset_texture_compute(0);
            graphics::unset_texture_compute(1);
        }

        // Decay/diffusion
        if (run_mold)
        {
            graphics::set_compute_shader(&decay_compute_shader);
            if (is_a) {
                graphics::set_texture_compute(&trail_tex_A, 0);
                graphics::set_texture_compute(&trail_tex_B, 1);
            } else {
                graphics::set_texture_compute(&trail_tex_B, 0);
                graphics::set_texture_compute(&trail_tex_A, 1);
            }
            graphics::run_compute(world_width / 8, world_height / 8, world_depth / 8);
            graphics::unset_texture_compute(0);
            graphics::unset_texture_compute(1);
        }

        // Rendering
        {
            graphics::set_render_targets_viewport(&render_target_window);
            graphics::clear_render_target(&render_target_window, 0.0f, 0.0f, 0.0f, 1);

            if(render_dof) {
                uint32_t clear_tex_uint[4] = {0, 0, 0, 0};
                graphics_context->context->ClearUnorderedAccessViewUint(display_tex_uint.ua_view, clear_tex_uint);

                graphics::update_constant_buffer(&rendering_settings_buffer, &rendering_settings);
                graphics::set_texture_compute(&display_tex_uint, 1);

                if (dof_type == DofType::TRAIL) {
                    graphics::set_compute_shader(&draw_compute_shader_trail);
                    if (is_a) {
                        graphics::set_texture_compute(&trail_tex_B, 0);
                    } else {
                        graphics::set_texture_compute(&trail_tex_A, 0);
                    }
                    graphics::run_compute(world_width / 2, world_height / 2, world_depth / 2);
                } else {
                    if (dof_type == DofType::PARTICLES) {
                        graphics::set_compute_shader(&draw_compute_shader_particle);
                    } else {
                        graphics::set_compute_shader(&draw_compute_shader_particle_pair);
                        graphics::set_structured_buffer(&particles_buffer_pair, 6);
                    }
                    graphics::run_compute(10, 10, 1);
                }

                graphics::set_compute_shader(&blit_compute_shader);
                graphics::set_texture_compute(&display_tex_uint, 0);
                graphics::set_texture_compute(&display_tex, 1);
                graphics::run_compute(window_width, window_height, 1);

                graphics::unset_texture_compute(0);
                graphics::unset_texture_compute(1);

                graphics::set_vertex_shader(&vertex_shader_2d);
                graphics::set_pixel_shader(&pixel_shader_2d);
                if (is_a) {
                    graphics::set_texture(&trail_tex_B, 0);
                } else {
                    graphics::set_texture(&trail_tex_A, 0);
                }
                graphics::set_texture(&display_tex, 0);
                graphics::set_texture_sampler(&tex_sampler, 0);
                graphics::draw_mesh(&quad_mesh);
                graphics::unset_texture(0);
            } else {
                graphics::set_vertex_shader(&vertex_shader);
                graphics::set_pixel_shader(&pixel_shader);

                if (is_a) {
                    graphics::set_texture(&trail_tex_B, 0);
                } else {
                    graphics::set_texture(&trail_tex_A, 0);
                }
                graphics::set_texture_sampler(&tex_sampler, 0);

                rendering_settings.model = math::get_rotation(math::PIHALF, Vector3(0, 1, 0));
                rendering_settings.texcoord_map = 2;
                graphics::update_constant_buffer(&rendering_settings_buffer, &rendering_settings);
                graphics::draw_mesh(&super_quad_mesh);

                rendering_settings.model = math::get_rotation(-math::PIHALF, Vector3(1, 0, 0));
                rendering_settings.texcoord_map = 1;
                graphics::update_constant_buffer(&rendering_settings_buffer, &rendering_settings);
                graphics::draw_mesh(&super_quad_mesh);

                rendering_settings.model = math::get_identity();
                rendering_settings.texcoord_map = 0;
                graphics::update_constant_buffer(&rendering_settings_buffer, &rendering_settings);
                graphics::draw_mesh(&super_quad_mesh);
                graphics::unset_texture(0);
            }
        }

        // UI
        if (show_ui) {
            graphics::set_render_targets_viewport(&render_target_window);

            Panel panel = ui::start_panel("", Vector2(10.0f, 10.0f));

            if (use_midi) {
                config.sense_spread = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_0) * math::PIHALF;
            }
            float ss = math::rad2deg(config.sense_spread);
            ui::add_slider(&panel, "SENSE SPREAD", &ss, 0.0, 90.0);
            config.sense_spread = math::deg2rad(ss);

            if (use_midi) {
                config.sense_distance = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_1) * 100.0f;
            }
            ui::add_slider(&panel, "SENSE DISTANCE", &config.sense_distance, 0.0, 100.0);

            if (use_midi) {
                config.turn_angle = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_2) * math::PIHALF;
            }
            float ts = math::rad2deg(config.turn_angle);
            ui::add_slider(&panel, "TURN ANGLE", &ts, 0.0, 90.0);
            config.turn_angle = math::deg2rad(ts);

            if (use_midi) {
                config.move_distance = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_3) * 20.0f;
            }
            ui::add_slider(&panel, "MOVE DISTANCE", &config.move_distance, 0.0, 20.0);

            if (use_midi) {
                config.deposit_value = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_4) * 5.0;
            }
            ui::add_slider(&panel, "DEPOSIT VALUE", &config.deposit_value, 0.0, 5.0);

            if (use_midi) {
                config.decay_factor = midi::get_controller_state(AKAI_MIDIMIX_SLIDER_5) * 1.0;
            }
            ui::add_slider(&panel, "DECAY FACTOR", &config.decay_factor, 0.0, 1.0);
            ui::add_slider(&panel, "SPAWN RADIUS", &spawn_radius, 20.0f, world_height / 2.0f);
            ui::add_slider(&panel, "CENTER ATTRACTION", &config.center_attraction, 0.0, 5.0);
            ui::add_slider(&panel, "MOVE SENSE COEF", &config.move_sense_coef, -1.0, 1.0);
            ui::add_slider(&panel, "MOVE SENSE OFFSET", &config.move_sense_offset, 0.0, 1.0);
            bool collision = config.collision > 0.0f;
            ui::add_toggle(&panel, "COLLISION", &collision);
            config.collision = collision ? 1.0f : 0.0f;

            ui::add_toggle(&panel, "DoF RENDERING", &render_dof);
            ui::end_panel(&panel);

            Vector4 panel_rect = ui::get_panel_rect(&panel);
            if(render_dof) {
                Panel panel = ui::start_panel("", Vector2(panel_rect.x, panel_rect.y + panel_rect.w + 10));

                bool dof_trail = dof_type == DofType::TRAIL;
                bool dof_particles = dof_type == DofType::PARTICLES;
                bool dof_particle_pairs = dof_type == DofType::PARTICLE_PAIRS;

                bool changed = ui::add_toggle(&panel, "TRAIL", &dof_trail);
                if (changed && dof_trail) {
                    dof_type = DofType::TRAIL;
                }

                changed = ui::add_toggle(&panel, "PARTICLES", &dof_particles);
                if (changed && dof_particles) {
                    dof_type = DofType::PARTICLES;
                }

                changed = ui::add_toggle(&panel, "PARTICLE_PAIRS", &dof_particle_pairs);
                if (changed && dof_particle_pairs) {
                    dof_type = DofType::PARTICLE_PAIRS;
                }

                ui::add_slider(&panel, "DOF SIZE", &rendering_settings.dof_size, 0.0, 2.0);
                ui::add_slider(&panel, "DOF DISTRIBUTION", &rendering_settings.dof_distribution, 0.0, 4.0);
                ui::add_slider(&panel, "FOCAL DISTANCE", &rendering_settings.focal_distance, 0.0, 10.0);
                ui::add_slider(&panel, "FOCAL DEPTH", &rendering_settings.focal_depth, 0.0, 5.0);
                ui::add_slider(&panel, "SAMPLE WEIGHT", &rendering_settings.sample_weight, 0.0, .06);
                float i = rendering_settings.iterations;
                ui::add_slider(&panel, "ITERATION", &i, 0.0, 2500);
                rendering_settings.iterations = int(i);
                ui::add_slider(&panel, "PAIR BREAK DISTANCE", &rendering_settings.break_distance, 0.0, 512);
                ui::end_panel(&panel);
            }

            ui::end_frame();
        }

        graphics::swap_frames();
    }

    graphics::release(&render_target_window);
    graphics::release(&depth_buffer);
    graphics::release(&pixel_shader);
    graphics::release(&vertex_shader);
    graphics::release(&pixel_shader_2d);
    graphics::release(&vertex_shader_2d);
    graphics::release(&draw_compute_shader_trail);
    graphics::release(&draw_compute_shader_particle);
    graphics::release(&draw_compute_shader_particle_pair);
    graphics::release(&blit_compute_shader);
    graphics::release(&compute_shader);
    graphics::release(&decay_compute_shader);
    graphics::release(&quad_mesh);
    graphics::release(&super_quad_mesh);
    graphics::release(&trail_tex_A);
    graphics::release(&trail_tex_B);
    graphics::release(&display_tex);
    graphics::release(&display_tex_uint);
    graphics::release(&occ_tex);
    graphics::release(&tex_sampler);
    graphics::release(&config_buffer);
    graphics::release(&particles_buffer_x);
    graphics::release(&particles_buffer_y);
    graphics::release(&particles_buffer_theta);
    graphics::release(&particles_buffer_z);
    graphics::release(&particles_buffer_phi);
    graphics::release(&particles_buffer_pair);
    graphics::release(&rendering_settings_buffer);

    //graphics::show_live_objects();

    graphics::release();

    midi::release();

    return 0;
}