RWTexture3D<half> tex_in: register(u0);
RWTexture3D<uint> tex_occ: register(u1);

RWStructuredBuffer<float> particles_x: register(u2);
RWStructuredBuffer<float> particles_y: register(u3);
RWStructuredBuffer<float> particles_z: register(u4);
RWStructuredBuffer<float> particles_phi: register(u5);
RWStructuredBuffer<float> particles_theta: register(u6);

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

cbuffer ConfigBuffer : register(b0)
{
    float sense_spread;
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
};

float3 rotate(float3 v, float3 a, float angle) {
    float3 result = cos(angle) * v + sin(angle) * (cross(a, v)) + dot(a, v) * (1 - cos(angle)) * a;
    return result;
}

float random(int seed) {
    return float(wang_hash(seed) % 1000) / 1000.0f;
}

float mod(float x, float y) {
     return x - y * floor(x / y);
}

[numthreads(10,10,10)]
void main(uint index : SV_GroupIndex, uint3 group_id :SV_GroupID){
    uint idx = index + 1000 * (group_id.x + group_id.y * 10 + group_id.z * 100);
    float halfpi = 3.1415 / 2.0f;
    float pi = 3.1415;

    // Fetch current particle state
    float x = particles_x[idx];
    float y = particles_y[idx];
    float z = particles_z[idx];
    float t = particles_theta[idx];
    float ph = particles_phi[idx];

    // Get vector which points in the current particle's direction 
    float3 center_axis = float3(sin(t) * cos(ph), cos(t), sin(t) * sin(ph));
    
    // Get base vector which points away from the current particle's direction and will be used
    // to sample environment in other directions
    float sense_theta = t - sense_spread;
    float3 off_center_base_dir = float3(sin(sense_theta) * cos(ph), cos(sense_theta), sin(sense_theta) * sin(ph));

    // Sample environment straight ahead
    int3 p = int3(x, y, z);
    float3 center_sense_pos = center_axis * sense_distance;

    // Sample environment away from the center axis and store max values.
    float max_value = tex_in[int3(center_sense_pos) + p];
    int max_value_count = 1;
    #define SAMPLE_POINTS 8
    int max_values[SAMPLE_POINTS + 1];// = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    max_values[0] = 0;
    float start_angle = random(idx * 42) * 3.1415 - halfpi;
    for (int i = 1; i < SAMPLE_POINTS + 1; ++i) {
        float angle = start_angle + pi * 2.0 / float(SAMPLE_POINTS) * i;
        float3 sense_position = rotate(off_center_base_dir, center_axis, angle) * sense_distance;
        float stuff = tex_in[int3(sense_position) + p];
        if (stuff > max_value) {
            max_value_count = 1;
            max_value = stuff;
            max_values[0] = i;
        } else if (stuff == max_value) {
            max_values[max_value_count++] = i;
        }
    }

    // Pick direction with max value sampled.
    uint random_max_value_direction = wang_hash(idx * uint(x) * uint(y) * uint(z)) % max_value_count;
    int direction = max_values[random_max_value_direction];
    float theta_turn = t - turn_angle;
    float3 off_center_base_dir_turn = float3(sin(theta_turn) * cos(ph), cos(theta_turn), sin(theta_turn) * sin(ph));
    if (direction > 0) {
        float3 best_direction = rotate(off_center_base_dir_turn, center_axis, direction * pi * 2.0 / float(SAMPLE_POINTS) + start_angle);
        ph = atan2(best_direction.z, best_direction.x);
        t = acos(best_direction.y / length(best_direction));
    }

    // Compute rotation applied by force pointing to the center of environment.
    float3 to_center = float3(world_width  / 2.0 - x, world_height / 2.0 - y, world_depth / 2.0 - z);
    float d_center = length(to_center);
    float d_c_turn = clamp((d_center - 50.0) / 150.0, 0, 1) * center_attraction;
    float3 dir = float3(sin(t) * cos(ph), cos(t), sin(t) * sin(ph));
    float3 center_dir = normalize(to_center);
    float3 center_angle = acos(dot(dir, center_dir));
    float st = 0.1 * d_c_turn;
    dir = sin((1 - st) * center_angle) / sin(center_angle) * dir + sin(st * center_angle) / sin(center_angle) * center_dir;
    if (length(dir) > 0.0 && (dir.z != 0.0 || dir.x != 0.0)){
        t = acos(dir.y / length(dir));
        ph = atan2(dir.z, dir.x);
    }

    // Make a step
    float3 dp = float3(sin(t) * cos(ph), cos(t), sin(t) * sin(ph)) * move_distance;
    x += dp.x;
    y += dp.y;
    z += dp.z;

    // Keep the particle inside environment
    x = mod(x, world_width);
    y = mod(y, world_height);
    z = mod(z, world_depth);

    // Check for collisions
    uint val = 0;
    InterlockedCompareExchange(tex_occ[uint3(x, y, z)], 0, uint(collision), val);
    if (val == 1.0) {
        x = particles_x[idx];
        y = particles_y[idx];
        z = particles_z[idx];
        t = acos(2 * float(wang_hash(idx * uint(x) * uint(y) * uint(z) + 4) % 1000) / 1000.0 - 1);
        ph = float(wang_hash(idx * uint(x) * uint(y) * uint(z) + 12) % 1000) / 1000.0 * 3.1415 * 2.0;
    }

    // Update particle state
    particles_x[idx] = x;
    particles_y[idx] = y;
    particles_z[idx] = z;
    particles_theta[idx] = t;
    particles_phi[idx] = ph;

    tex_in[uint3(x, y, z)] += deposit_value;
}

