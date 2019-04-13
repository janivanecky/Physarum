struct PixelInput
{
	float4 position_out: SV_POSITION;
    float3 texcoord_out: TEXCOORD;
};

Texture3D tex : register(t0);
SamplerState tex_sampler : register(s0);

cbuffer ConfigBuffer : register(b4)
{
    float4x4 projection_matrix;
    float4x4 view_matrix;
    float4x4 model_matrix;
	int texcoord_map;
	int show_grid;
};

static const float N = 30.0f;
static const float GRID_SIZE = 5.0f;
static const float GRID_OPACITY = 0.3f;

// https://www.iquilezles.org/www/articles/filterableprocedurals/filterableprocedurals.htm
float grid(float3 p, float3 dpdx, float3 dpdy)
{
    float3 w = max(abs(dpdx), abs(dpdy)) + 0.01f;
    float3 a = p + 0.5f * w;                        
    float3 b = p - 0.5f * w;           
    float3 i = (floor(a)+min(frac(a)*N,1.0)-floor(b)-min(frac(b)*N,1.0))/(N*w);
	return i.x * i.y * i.z;
}

float4 main(PixelInput input) : SV_TARGET
{
	float v = tex.Sample(tex_sampler, input.texcoord_out.xyz) / 5.0;
	float3 p = input.texcoord_out.xyz * (GRID_SIZE + 1.0f / N);
	float3 dx = ddx(p);
	float3 dy = ddy(p);
	float grid_color = grid(p, dx, dy) * GRID_OPACITY * show_grid;
	v = max(grid_color, v);
	return float4(1.0f, 1.0f, 1.0f, v);
}