// caption text quad: render-target-space positions to clip space
cbuffer ConstantBufferVS : register(b0)
{
	float2 g_viewportSize;
	float2 g_padding;
};

struct VSInput
{
	float2 position : POSITION;
	float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	output.position = float4(
		input.position.x / g_viewportSize.x * 2.0f - 1.0f,
		1.0f - input.position.y / g_viewportSize.y * 2.0f,
		0.0f,
		1.0f
	);
	output.texcoord = input.texcoord;
	return output;
}
