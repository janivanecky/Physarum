RWStructuredBuffer<float> particles_x: register(u2);
RWStructuredBuffer<float> particles_y: register(u3);
RWStructuredBuffer<float> particles_z: register(u4);
RWStructuredBuffer<float> particles_t: register(u6);
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
    float t = particles_t[idx];
    float3 in_pos = float3(x, y, z);

    // Project point in "mold world texture space" to 3D scene world space.
    float3 world_size = float3(world_width, world_height, world_depth);
    float4 in_posf = float4(in_pos / world_size * 2.0 - 1.0, 1.0);
    in_posf.yz *= -1;   // Invert yz axis because the direction is different for textures and standard right handed system.

    // Project into 3D scene world space.        
    float4 world_pos = mul(view_matrix, in_posf);

    // DoF with sampling.
    float d = length(world_pos.xyz);
    float r = m * pow(abs(f - d) / g, e);
    for (int i = 0; i < iterations; ++i) {
        float3 sample_pos = world_pos;
        sample_pos.xyz += random_sphere(idx * 33 + i) * r;

        // Project from 3D to 2D.
        float4 out_posf = mul(projection_matrix, sample_pos);
        out_posf /= out_posf.w;
        out_posf = out_posf * 0.5 + 0.5;
        out_posf.xy *= float2(screen_width, screen_height);
        
        // Store sample to output texture.
        uint2 out_pos = uint2(out_posf.xy);
        if (t < 0.0) {
            InterlockedAdd(tex_out[out_pos], 1000000000);
        } else {
            InterlockedAdd(tex_out[out_pos], 1000);
        }
    }
}