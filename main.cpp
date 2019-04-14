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
    uint32_t window_width = 1200, window_height = 800;
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

    // Simulation params
    uint32_t world_width = 480, world_height = 480, world_depth = 480;
    //uint32_t size = 640 ;
    //uint32_t world_width = size, world_height = size, world_depth = size;
    //uint32_t world_width = 250, world_height = 250, world_depth = 250;
    float spawn_radius = 50.0f;
    const int NUM_PARTICLES = 500000;
    //#define _2D
#ifdef _2D
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

    // Textures for the simulation
    Texture2D trail_tex_A = graphics::get_texture2D(NULL, world_width, world_height, DXGI_FORMAT_R32_FLOAT, 4);
    Texture2D trail_tex_B = graphics::get_texture2D(NULL, world_width, world_height, DXGI_FORMAT_R32_FLOAT, 4);
    Texture2D occ_tex = graphics::get_texture2D(NULL, world_width, world_height, DXGI_FORMAT_R32_UINT, 4);

    // Particles setup
    float *particles_x = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_y = memory::alloc_heap<float>(NUM_PARTICLES);
    float *particles_theta = memory::alloc_heap<float>(NUM_PARTICLES);

    auto update_particles = [&world_width, &world_height](float *px, float *py, float *pt, int count, float spawn_radius) {
        for (int i = 0; i < count; ++i) {
            float angle = random::uniform(0, math::PI2);
            float radius = random::uniform(0, 1);
            radius = math::sqrt(radius) * spawn_radius;
            px[i] = math::sin(angle) * radius + world_width / 2.0f;
            py[i] = math::cos(angle) * radius + world_height / 2.0f;
            pt[i] = random::uniform(0, math::PI2);
        }
    };
    update_particles(particles_x, particles_y, particles_theta, NUM_PARTICLES, spawn_radius);

    // Set up buffer containing particle data
    StructuredBuffer particles_buffer_x = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_x, particles_x);
    StructuredBuffer particles_buffer_y = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_y, particles_y);
    StructuredBuffer particles_buffer_theta = graphics::get_structured_buffer(sizeof(float), NUM_PARTICLES);
    graphics::update_structured_buffer(&particles_buffer_theta, particles_theta);

    Mesh quad_mesh = graphics::get_mesh(quad_vertices, quad_vertices_count, quad_vertices_stride, NULL, 0, 0);
#else
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

    // Textures for the simulation
    Texture3D trail_tex_A = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R16_FLOAT, 2);
    Texture3D trail_tex_B = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R16_FLOAT, 2);
    Texture3D occ_tex = graphics::get_texture3D(NULL, world_width, world_height, world_depth, DXGI_FORMAT_R32_UINT, 4);

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

    auto update_particles = [&world_width, &world_height, &world_depth](float *px, float *py, float *pz, float *pp, float *pt, int count, float spawn_radius) {
        for (int i = 0; i < count; ++i) {
            float phi = random::uniform(0, math::PI2);
            float theta = math::acos(2 * random::uniform(0, 1.0) - 1);
            float radius = random::uniform(0, 1);
            radius = math::pow(radius, 1.0f/3.0f) * spawn_radius;
            px[i] = math::sin(phi) * math::sin(theta) * radius + world_width / 2.0f;
            py[i] = math::cos(theta) * radius + world_height / 2.0f;
            pz[i] = math::cos(phi) * math::sin(theta) * radius + world_depth / 2.0f;
            pp[i] = math::acos(2 * random::uniform(0, 1.0) - 1);
            pt[i] = random::uniform(0, math::PI2);
        }
    };
    update_particles(particles_x, particles_y, particles_z, particles_phi, particles_theta, NUM_PARTICLES, spawn_radius);

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
    Mesh quad_mesh = graphics::get_mesh(super_quad_vertices, quad_vertices_count * world_depth, super_quad_vertices_stride, NULL, 0, 0);

    // Set up 3D rendering matrices buffer.
    float aspect_ratio = float(window_width) / float(window_height);
    Matrix4x4 projection_matrix = math::get_perspective_projection_dx_rh(math::deg2rad(60.0f), aspect_ratio, 0.01f, 10.0f);
    float azimuth = 0.0f;
    float polar = math::PIHALF;
    float radius = 2.0f;
    Vector3 eye_pos = Vector3(math::cos(azimuth) * math::sin(polar), math::cos(polar), math::sin(azimuth) * math::sin(polar)) * radius;
    Matrix4x4 view_matrix = math::get_look_at(eye_pos, Vector3(0,0,0), Vector3(0,1,0));

    struct MatrixBuffer {
        Matrix4x4 projection;
        Matrix4x4 view;
        Matrix4x4 model;
        int texcoord_map;
        int show_grid;
        int filler2;
        int filler3;
    };
    MatrixBuffer matrices = {};
    matrices.projection = projection_matrix;
    matrices.view = view_matrix;
    matrices.model = math::get_identity();

    ConstantBuffer matrix_buffer = graphics::get_constant_buffer(sizeof(MatrixBuffer));
    graphics::update_constant_buffer(&matrix_buffer, &matrices);
    graphics::set_constant_buffer(&matrix_buffer, 4);

#endif
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
        int filler;
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
        int(world_width),
        int(world_height),
        int(world_depth),
    };
    ConstantBuffer config_buffer = graphics::get_constant_buffer(sizeof(Config));

    Timer timer = timer::get();
    timer::start(&timer);

    // Render loop
    bool is_running = true;
    bool show_ui = false;
    bool run_mold = true;
    bool turning_camera = false;
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
            #ifndef _2D
            radius -= input::mouse_scroll_delta() * 0.1f;
            if (input::mouse_left_button_down()) {
                Vector2 dm = input::mouse_delta_position();
                azimuth += dm.x * 0.003f;
                polar -= dm.y * 0.003f;
                polar = math::clamp(polar, 0.02f, math::PI);
            }
            if (turning_camera) {
                azimuth += 0.01f;
            }
            Vector3 eye_pos = Vector3(math::cos(azimuth) * math::sin(polar), math::cos(polar), math::sin(azimuth) * math::sin(polar)) * radius;
            Matrix4x4 view_matrix = math::get_look_at(eye_pos, Vector3(0,0,0), Vector3(0,1,0));
            matrices.view = view_matrix;

            float dir_x_mag = math::abs(eye_pos.x);
            float dir_y_mag = math::abs(eye_pos.y);
            float dir_z_mag = math::abs(eye_pos.z);
            float eye_dir_max = math::max(dir_x_mag, math::max(dir_y_mag, dir_z_mag));
            if (eye_dir_max == dir_x_mag) {
                matrices.model = math::get_rotation(math::PIHALF, Vector3(0, 1, 0));
                matrices.texcoord_map = 2;
            } else if (eye_dir_max == dir_y_mag) {
                matrices.model = math::get_rotation(-math::PIHALF, Vector3(1, 0, 0));
                matrices.texcoord_map = 1;
            } else {
                matrices.model = math::get_identity();
                matrices.texcoord_map = 0;
            }

            graphics::update_constant_buffer(&matrix_buffer, &matrices);
            if (input::key_pressed(KeyCode::F4)) matrices.show_grid = !matrices.show_grid;
            if (input::key_pressed(KeyCode::F5)) turning_camera = !turning_camera;
            #endif
            if (input::key_pressed(KeyCode::ESC)) is_running = false; 
            if (input::key_pressed(KeyCode::F1)) show_ui = !show_ui; 
            if (input::key_pressed(KeyCode::F3)) run_mold = !run_mold;
            if (input::key_pressed(KeyCode::F2))
            {
                // Reset particles + trails + occupancy map
                #ifdef _2D
                update_particles(particles_x, particles_y, particles_theta, NUM_PARTICLES, spawn_radius);
                #else
                update_particles(particles_x, particles_y, particles_z, particles_phi, particles_theta, NUM_PARTICLES, spawn_radius);
                graphics::update_structured_buffer(&particles_buffer_z, particles_z);
                graphics::update_structured_buffer(&particles_buffer_phi, particles_phi);
                #endif
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
            float clear_tex[4] = {0, 0, 0, 0};
            graphics_context->context->ClearUnorderedAccessViewUint(occ_tex.ua_view, clear_tex_uint);
            graphics::set_texture_compute(&occ_tex, 1);
            if (is_a) {
                graphics::set_texture_compute(&trail_tex_A, 0);
            } else {
                graphics::set_texture_compute(&trail_tex_B, 0);
            }
            #ifdef _2D
            graphics::set_structured_buffer(&particles_buffer_x, 2);
            graphics::set_structured_buffer(&particles_buffer_y, 3);
            graphics::set_structured_buffer(&particles_buffer_theta, 4);
            #else
            graphics::set_structured_buffer(&particles_buffer_x, 2);
            graphics::set_structured_buffer(&particles_buffer_y, 3);
            graphics::set_structured_buffer(&particles_buffer_z, 4);
            graphics::set_structured_buffer(&particles_buffer_phi, 5);
            graphics::set_structured_buffer(&particles_buffer_theta, 6);
            #endif
            graphics::run_compute(10, 10, 5);
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
            #ifdef _2D
            graphics::run_compute(world_width / 8, world_height / 10, 1);
            #else
            graphics::run_compute(world_width / 8, world_height / 8, world_depth / 8);
            #endif
            graphics::unset_texture_compute(0);
            graphics::unset_texture_compute(1);
        }

        // Render trail map
        {
            graphics::set_render_targets_viewport(&render_target_window);
            graphics::clear_render_target(&render_target_window, 0.0f, 0.0f, 0.0f, 1);

            graphics::set_vertex_shader(&vertex_shader);
            graphics::set_pixel_shader(&pixel_shader);
            if (is_a) {
                graphics::set_texture(&trail_tex_B, 0);
            } else {
                graphics::set_texture(&trail_tex_A, 0);
            }
            graphics::set_texture_sampler(&tex_sampler, 0);
            #ifdef _2D
            graphics::draw_mesh(&quad_mesh);
            #else
            matrices.model = math::get_rotation(math::PIHALF, Vector3(0, 1, 0));
            matrices.texcoord_map = 2;
            graphics::update_constant_buffer(&matrix_buffer, &matrices);
            graphics::draw_mesh(&quad_mesh);
            
            matrices.model = math::get_rotation(-math::PIHALF, Vector3(1, 0, 0));
            matrices.texcoord_map = 1;
            graphics::update_constant_buffer(&matrix_buffer, &matrices);
            graphics::draw_mesh(&quad_mesh);

            matrices.model = math::get_identity();
            matrices.texcoord_map = 0;
            graphics::update_constant_buffer(&matrix_buffer, &matrices);
            graphics::draw_mesh(&quad_mesh);
            #endif
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
    graphics::release(&config_buffer);
    graphics::release(&particles_buffer_x);
    graphics::release(&particles_buffer_y);
    graphics::release(&particles_buffer_theta);
    #ifndef _2D
    graphics::release(&particles_buffer_z);
    graphics::release(&particles_buffer_phi);
    graphics::release(&matrix_buffer);
    #endif

    //graphics::show_live_objects();

    graphics::release();

    return 0;
}