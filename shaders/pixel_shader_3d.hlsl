struct PixelInput
{
	float4 position_out: SV_POSITION;
    float3 texcoord_out: TEXCOORD;
};

Texture3D tex : register(t0);
SamplerState tex_sampler : register(s0);

float4 main(PixelInput input) : SV_TARGET
{
	float v = tex.Sample(tex_sampler, input.texcoord_out.xyz) / 5.0;
	return float4(1.0f, 1.0f, 1.0f, v);
}