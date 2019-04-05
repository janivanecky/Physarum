RWTexture2D<float> tex_in: register(u0);
RWTexture2D<float> tex_out: register(u1);
RWTexture2D<uint> tex_occ: register(u3);

struct Particle {
    float x; float y; float theta; float info;
};

RWStructuredBuffer<Particle> particles: register(u2);

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

[numthreads(10,10,10)]
void main(uint3 threadIDInGroup : SV_GroupThreadID, uint3 groupID : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID){
    uint idx = dispatchThreadId.x * 10000 + dispatchThreadId.y * 100 + dispatchThreadId.z;
    const int WIDTH = 1200;
    const int HEIGHT = 800;

    float x = particles[idx].x;
    float y = particles[idx].y;
    float t = particles[idx].theta;
    float x_center = (WIDTH  / 2.0 - x);
    float y_center = (HEIGHT / 2.0 - y);
    float d_center = x_center * x_center + y_center * y_center;
    d_center = sqrt(d_center);
    float center_angle = atan2(y_center, x_center);
    float r = float(wang_hash(idx) % 1000.0) / 1000.0 * 0.5 + 0.5f;

    //float d_c = //sin(d_center / 20.0) * 0.5f + 0.5f;
    float d_c = 1.0;//1.0 - clamp((d_center - 50.0) / 150.0, 0, 1.0);
    float d_c_turn = clamp((d_center - 50.0) / 150.0, 0, 1) * center_attraction;
    float tc = t;
    float tl = t - sense_spread;
    float tr = t + sense_spread;
    float2 dc = float2(cos(tc), sin(tc)) * sense_distance * d_c * r;
    float2 dl = float2(cos(tl), sin(tl)) * sense_distance * d_c * r;    
    float2 dr = float2(cos(tr), sin(tr)) * sense_distance * d_c * r;

    int2 p = int2(x, y);
    float stuff_c = tex_in[int2(dc) + p];
    float stuff_l = tex_in[int2(dl) + p];
    float stuff_r = tex_in[int2(dr) + p];
    
    if (stuff_l > stuff_r && stuff_l > stuff_c) {
        t -= turn_angle;
    } else if (stuff_r > stuff_l && stuff_r > stuff_c) {
        t += turn_angle;
    } else if (stuff_c < stuff_l && stuff_c < stuff_r) {
        uint r = wang_hash(idx * uint(x) * uint(y));
        if (r % 2 == 0) {
            t += turn_angle;
        } else {
            t -= turn_angle;
        }
    }
    float diff = atan2(sin(center_angle - t), cos(center_angle - t));
    t += diff * 0.1 * d_c_turn;

    float2 dp = float2(cos(t), sin(t)) * move_distance * r;

    x += dp.x;
    y += dp.y;

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

    uint val = 0;
    InterlockedCompareExchange(tex_occ[uint2(x, y)], 0, uint(collision), val);
    if (val == 1.0) {
        x = particles[idx].x;
        y = particles[idx].y;
        t = float(wang_hash(idx * uint(x) * uint(y)) % 1000) / 1000.0 * 3.1415 * 2.0;       
    }

    particles[idx].x = x;
    particles[idx].y = y;
    particles[idx].theta = t;

    tex_in[uint2(x, y)] += deposit_value;
}

