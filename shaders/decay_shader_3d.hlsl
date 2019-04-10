RWTexture3D<float> tex_in: register(u0);
RWTexture3D<float> tex_out: register(u1);

cbuffer ConfigBuffer : register(b0)
{
    float sense_spread;
    float sense_distance;
    float turn_angle;
    float move_distance;
    float deposit_value;
    float decay_factor;
};

[numthreads(10,10,10)]
void main(uint3 threadIDInGroup : SV_GroupThreadID, uint3 groupID : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID){
    uint3 p = dispatchThreadId.xyz;
    float v = 0.0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                v += tex_in[int3(p) + int3(dx, dy, dz)] * decay_factor / 27.0;
            }
        }
    }

    tex_out[p] = v;
}

