RWTexture2D<uint> tex_in: register(u0);
RWTexture2D<float> tex_out: register(u1);

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
    float sample_weight;
};


[numthreads(1, 1, 1)]
void main(uint3 threadIDInGroup : SV_GroupThreadID, uint3 groupID : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID){
    tex_out[dispatchThreadId.xy] = tex_in[dispatchThreadId.xy] / 1000.0 * sample_weight;
}