
SamplerState texSampler : register(s0);
Texture2D tex : register(t0);

struct PsInput{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PsInput input) : SV_TARGET{
	float c = input.tex.y;

	float4 color = tex.Sample(texSampler, input.tex);

		return color;
}