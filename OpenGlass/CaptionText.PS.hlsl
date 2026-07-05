// caption text coverage: R/G/B = per-subpixel gamma-corrected coverage, A = center
// tap; the blend state (src = blendFactor(textColor), dst = 1 - srcColor) applies
// the Win7 ClearType component-alpha blend
Texture2D g_coverage : register(t0);
SamplerState g_sampler : register(s0);

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
	return g_coverage.Sample(g_sampler, input.texcoord);
}
