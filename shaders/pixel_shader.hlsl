struct PixelInput
{
	float4 position_out: SV_POSITION;
    float2 texcoord_out: TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState tex_sampler : register(s0);

float4 main(PixelInput input) : SV_TARGET
{
	float v = tex.Sample(tex_sampler, input.texcoord_out) / 5.0;
	return float4(v, v, v, 1);
}