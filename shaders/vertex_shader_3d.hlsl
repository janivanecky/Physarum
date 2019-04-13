struct VertexInput
{
	float4 position: POSITION;
	float3 texcoord: TEXCOORD;
};

struct VertexOutput
{
	float4 position_out: SV_POSITION;
	float3 texcoord_out: TEXCOORD;
};


cbuffer ConfigBuffer : register(b4)
{
    float4x4 projection_matrix;
    float4x4 view_matrix;
    float4x4 model_matrix;
	int texcoord_map;
	int show_grid;
};

VertexOutput main(VertexInput input)
{
	VertexOutput result;

	result.position_out = mul(projection_matrix, mul(view_matrix, mul(model_matrix, input.position)));
	if (texcoord_map == 0) {
		result.texcoord_out = input.texcoord;
	} else if (texcoord_map == 1) {
		result.texcoord_out = float3(input.texcoord.x, input.texcoord.z, 1.0-input.texcoord.y);
	} else if (texcoord_map == 2) {
		result.texcoord_out = float3(1.0-input.texcoord.z, input.texcoord.y, input.texcoord.x);
	}

	return result;
}