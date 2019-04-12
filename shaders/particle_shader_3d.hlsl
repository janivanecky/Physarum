RWTexture3D<float> tex_in: register(u0);
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
};

float3 rotate(float3 v, float3 a, float angle) {
    float3 result = cos(angle) * v + sin(angle) * (cross(a, v)) + dot(a, v) * (1 - cos(angle)) * a;
    return result;
}

float random(int seed) {
    return float(wang_hash(seed) % 1000) / 1000.0f;
}

[numthreads(10,10,10)]
void main(uint index : SV_GroupIndex, uint3 group_id :SV_GroupID){
    uint idx = index + 1000 * (group_id.x + group_id.y * 10 + group_id.z * 100);
    const int WIDTH = 400;
    const int HEIGHT = 400;
    const int DEPTH = 400;
    float halfpi = 3.1415 / 2.0f;

    float x = particles_x[idx];
    float y = particles_y[idx];
    float z = particles_z[idx];
    
    float t = particles_theta[idx];
    float ph = particles_phi[idx];

    float x_center = (WIDTH  / 2.0 - x);
    float y_center = (HEIGHT / 2.0 - y);
    float z_center = (DEPTH / 2.0 - z);
    float3 d_center_v = float3(x_center, y_center, z_center);
    float d_center = length(d_center_v);
    float r = float(wang_hash(idx) % 1000.0) / 1000.0 * 0.5 + 0.5f;

    //float d_c = //sin(d_center / 20.0) * 0.5f + 0.5f;
    float d_c = 1.0;//1.0 - clamp((d_center - 50.0) / 150.0, 0, 1.0);
    float d_c_turn = clamp((d_center - 50.0) / 150.0, 0, 1) * center_attraction;
    float tc = t;
    float pc = ph;
    float dt = t - sense_spread;
    float3 dc = float3(sin(tc) * cos(pc), cos(tc), sin(tc) * sin(pc)) * sense_distance * d_c * r;
    float3 axis = normalize(dc);
    float3 dd = float3(sin(dt) * cos(pc), cos(dt), sin(dt) * sin(pc));// * sense_distance * d_c * r;
    float lca = random(idx * 42) * 3.1415 - halfpi;
    float rca = lca + 3.1415;
    float cla = lca + halfpi;
    float cra = lca -halfpi;
    float3 dlc = rotate(dd, axis, lca) * sense_distance * d_c * r;
    float3 drc = rotate(dd, axis, rca) * sense_distance * d_c * r;
    float3 dcl = rotate(dd, axis, cla) * sense_distance * d_c * r;
    float3 dcr = rotate(dd, axis, cra) * sense_distance * d_c * r;

    int3 p = int3(x, y, z);
    float stuff_c =  tex_in[int3(dc) + p];
    float stuff_lc = tex_in[int3(dlc) + p];
    float stuff_rc = tex_in[int3(drc) + p];
    float stuff_cl = tex_in[int3(dcl) + p];
    float stuff_cr = tex_in[int3(dcr) + p];
    
    float tla = t - turn_angle;
    float3 td = float3(sin(tla) * cos(pc), cos(tla), sin(tla) * sin(pc));
    float tra = t + turn_angle;
    float3 rlc = rotate(td, axis, lca);
    float3 rrc = rotate(td, axis, rca);
    float3 rcl = rotate(td, axis, cla);
    float3 rcr = rotate(td, axis, cra);
    float rlc_length = length(rlc);
    float rrc_length = length(rrc);
    float rcl_length = length(rcl);
    float rcr_length = length(rcr);

    int max_value_count = 0;
    int max_values[5];
    #define C  0
    #define LC 1
    #define RC 2
    #define CL 3
    #define CR 4
    float max_value = max(stuff_c, stuff_lc);
    max_value = max(max_value, stuff_rc);
    max_value = max(max_value, stuff_cl);
    max_value = max(max_value, stuff_cr);

    if (max_value == stuff_c) {
        max_values[max_value_count++] = C;
    }
    if (max_value == stuff_lc) {
        max_values[max_value_count++] = LC;
    }
    if (max_value == stuff_rc) {
        max_values[max_value_count++] = RC;
    }
    if (max_value == stuff_cl) {
        max_values[max_value_count++] = CL;
    }
    if (max_value == stuff_cr) {
        max_values[max_value_count++] = CR;
    }
//
    uint r_direction = wang_hash(idx * uint(x) * uint(y) * uint(z)) % max_value_count;
    int direction = max_values[r_direction];
    if (direction == LC) {
        ph = atan2(rlc.z, rlc.x);
        t = acos(rlc.y / rlc_length);
    } else if (direction == RC) {
        ph = atan2(rrc.z, rrc.x);
        t = acos(rrc.y / rrc_length);
    } else if (direction == CL) {
        ph = atan2(rcl.z, rcl.x);
        t = acos(rcl.y / rcl_length);
    } else if (direction == CR) {
        ph = atan2(rcr.z, rcr.x);
        t = acos(rcr.y / rcr_length);
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

    // TODO: Make robust
    if (y < 0) {
        y = HEIGHT - 1;
    } else if (y > HEIGHT - 1) {
        y = 0;
    } 
    if (x < 0) {
        x = WIDTH - 1;
    } else if (x > WIDTH - 1) { 
        x = 0;
    }
    if (z < 0) {
        z = DEPTH - 1;
    } else if (z > DEPTH - 1) {
        z = 0;
    }

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

