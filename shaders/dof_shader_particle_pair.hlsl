RWStructuredBuffer<float> particles_x: register(u2);
RWStructuredBuffer<float> particles_y: register(u3);
RWStructuredBuffer<float> particles_z: register(u4);
RWStructuredBuffer<uint> particles_buddy: register(u6);
RWTexture2D<uint> tex_out: register(u1);

cbuffer ConfigBuffer : register(b4)
{
    float4x4 projection_matrix;
    float4x4 view_matrix;
    float4x4 model_matrix;
	int texcoord_map;
	int show_grid;
    float m;
    float e;
    float f;
    float g;
    int iterations;
    float break_distance;
    float world_width;
    float world_height;
    float world_depth;
    float screen_width;
    float screen_height;
};

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}


float random(int seed) {
    return float(wang_hash(seed) % 1000) / 1000.0f;
}

float3 random_sphere(uint hash)
{
	float a = random(hash);
    float b = random(hash + 3);
    float azimuth = a * 2 * 3.14159265;
    float polar = acos(2 * b - 1);

    float3 result;
	result.x = sin(polar) * cos(azimuth);
	result.y = cos(polar);
	result.z = sin(polar) * sin(azimuth);

	return result;
}

[numthreads(10, 10, 10)]
void main(uint3 threadIDInGroup : SV_GroupThreadID, uint3 group_id : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID, uint index : SV_GroupIndex){
    uint idx = index + 1000 * (group_id.x + group_id.y * 10 + group_id.z * 100);

    float x = particles_x[idx];
    float y = particles_y[idx];
    float z = particles_z[idx];
    float3 in_pos = float3(x, y, z);

    // To render particle pairs, we have to have a particle within reasonable distance - buddy.
    uint buddy_idx = particles_buddy[idx];
    bool need_new_buddy = false;
    // If we don't find any buddy, we just use current particle.
    float x2 = x;
    float y2 = y;
    float z2 = z;
    // If buddy_idx is over 9000000, this means that the buddy index array has just been initialized,
    // so we have to search for a closest buddy.
    if (buddy_idx > 9000000) {
        need_new_buddy = true;
    } else {
        float xb = particles_x[buddy_idx];
        float yb = particles_y[buddy_idx];
        float zb = particles_z[buddy_idx];
        // If distance between particle and its previous buddy is too high, we'll have to find a new buddy.
        if (length(float3(xb, yb, zb) - in_pos) > break_distance) {
            need_new_buddy = true;
        } else {
            x2 = xb;
            y2 = yb;
            z2 = zb;
        }
    }
    if(need_new_buddy) {
        // Find a new buddy.
        float buddy_distance = break_distance;
        for (int i = 1; i < 1000 && idx; ++i) {
            float xb = particles_x[idx + i];
            float yb = particles_y[idx + i];
            float zb = particles_z[idx + i];
            float distance = length(float3(xb, yb, zb) - in_pos);
            if (distance < buddy_distance) {
                buddy_distance = distance;
                x2 = xb;
                y2 = yb;
                z2 = zb;
                buddy_idx = idx + i;
            }
        }
        if (buddy_idx == particles_buddy[idx]) {
            return;
        }
        particles_buddy[idx] = buddy_idx;
    }
    float3 in_pos2 = float3(x2, y2, z2);
    
    // Project points in "mold world texture space" to 3D scene world space.
    float3 world_size = float3(world_width, world_height, world_depth);
    float4 in_posf = float4(in_pos / world_size * 2.0 - 1.0, 1.0);
    in_posf.yz *= -1;
    float4 in_posf2 = float4(in_pos2 / world_size * 2.0 - 1.0, 1.0);
    in_posf2.yz *= -1;

    for (int i = 0; i < iterations; ++i) {
        // Pick a random point on a line between two selected points.
        float rand = random(idx + i * 32 * 13);
        float4 line_pos = rand * in_posf + (1 - rand) * in_posf2;

        // Project into 3D scene world space.        
        float4 world_pos = mul(view_matrix, line_pos);

        // DoF
        float d = length(world_pos.xyz);
        float r = m * pow(abs(f - d) / g, e);
        world_pos.xyz += random_sphere(idx * 33 + i) * r;

        // Project from 3D to 2D.
        float4 out_posf = mul(projection_matrix, world_pos);
        out_posf /= out_posf.w;
        out_posf = 0.5 * out_posf + 0.5;
        out_posf.xy *= float2(screen_width, screen_height);
        
        // Store sample to output texture.
        uint2 out_pos = uint2(out_posf.xy);
        InterlockedAdd(tex_out[out_pos], 1000);
    }
}