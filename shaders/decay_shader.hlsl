RWTexture2D<float> tex_in: register(u0);
RWTexture2D<float> tex_out: register(u1);

cbuffer ConfigBuffer : register(b0)
{
    float sense_spread;
    float sense_distance;
    float turn_angle;
    float move_distance;
    float deposit_value;
    float decay_factor;
};

[numthreads(1,1,1)]
void main(uint3 threadIDInGroup : SV_GroupThreadID, uint3 groupID : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID){
    uint2 p = dispatchThreadId.xy;
    float v = 0.0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            v += tex_in[int2(p) + int2(dx, dy)] * decay_factor / 9.0;
        }
    }

    tex_out[p] = v;
}

