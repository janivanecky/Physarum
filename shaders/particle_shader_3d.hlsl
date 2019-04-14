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

    float x = particles_x[idx];
    float y = particles_y[idx];
    float z = particles_z[idx];
    
    float t = particles_theta[idx];
    float ph = particles_phi[idx];

    float x_center = (world_width  / 2.0 - x);
    float y_center = (world_height / 2.0 - y);
    float z_center = (world_depth / 2.0 - z);
    float3 d_center_v = float3(x_center, y_center, z_center);
    float d_center = length(d_center_v);
    float r = float(wang_hash(idx) % 1000.0) / 1000.0 * 0.5 + 0.5f;
    int3 p = int3(x, y, z);

    //float d_c = //sin(d_center / 20.0) * 0.5f + 0.5f;
    float d_c = 1.0;//1.0 - clamp((d_center - 50.0) / 150.0, 0, 1.0);
    float d_c_turn = clamp((d_center - 50.0) / 150.0, 0, 1) * center_attraction;
    float tc = t;
    float pc = ph;
    float dt = t - sense_spread;
    float3 dc = float3(sin(tc) * cos(pc), cos(tc), sin(tc) * sin(pc)) * sense_distance * d_c * r;
    float3 axis = normalize(dc);
    float3 dd = float3(sin(dt) * cos(pc), cos(dt), sin(dt) * sin(pc));// * sense_distance * d_c * r;
    float start_angle = random(idx * 42) * 3.1415 - halfpi;
    float stuffs[5];

    // Value straight ahead
    float max_value = tex_in[int3(dc) + p];
    int max_value_count = 1;
    int max_values[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 1; i < 9; ++i) {
        float angle = start_angle + halfpi / 2.0 * i;
        float3 sense_position = rotate(dd, axis, angle) * sense_distance * d_c * r;
        float stuff = tex_in[int3(sense_position) + p];
        if (stuff > max_value) {
            max_value_count = 1;
            max_value = stuff;
            max_values[0] = i;
        } else if (stuff == max_value) {
            max_values[max_value_count++] = i;
        }
    }

    uint r_direction = wang_hash(idx * uint(x) * uint(y) * uint(z)) % max_value_count;
    int direction = max_values[r_direction];
    float tla = t - turn_angle;
    float3 td = float3(sin(tla) * cos(pc), cos(tla), sin(tla) * sin(pc));
    if (direction > 0) {
        float3 best_direction = rotate(td, axis, direction * halfpi / 2.0 + start_angle);
        ph = atan2(best_direction.z, best_direction.x);
        t = acos(best_direction.y / length(best_direction));
    }

    float3 dir = float3(sin(t) * cos(ph), cos(t), sin(t) * sin(ph));
    float3 center_dir = normalize(d_center_v);
    float3 center_angle = acos(dot(dir, center_dir));
    float st = 0.1 * d_c_turn;
    dir = sin((1 - st) * center_angle) / sin(center_angle) * dir + sin(st * center_angle) / sin(center_angle) * center_dir;
    if (length(dir) > 0.0 && (dir.z != 0.0 || dir.x != 0.0)){
        t = acos(dir.y / length(dir));
        ph = atan2(dir.z, dir.x);
    }

    float3 dp = float3(sin(t) * cos(ph), cos(t), sin(t) * sin(ph)) * move_distance;// * r;
    x += dp.x;
    y += dp.y;
    z += dp.z;

    x = mod(x, world_width);
    y = mod(y, world_height);
    z = mod(z, world_depth);

    uint val = 0;
    InterlockedCompareExchange(tex_occ[uint3(x, y, z)], 0, uint(collision), val);
    if (val == 1.0) {
        x = particles_x[idx];
        y = particles_y[idx];
        z = particles_z[idx];
        t = acos(2 * float(wang_hash(idx * uint(x) * uint(y) * uint(z) + 4) % 1000) / 1000.0 - 1);
        ph = float(wang_hash(idx * uint(x) * uint(y) * uint(z) + 12) % 1000) / 1000.0 * 3.1415 * 2.0;
    }

    particles_x[idx] = x;
    particles_y[idx] = y;
    particles_z[idx] = z;
    particles_theta[idx] = t;
    particles_phi[idx] = ph;

    tex_in[uint3(x, y, z)] += deposit_value;
}

