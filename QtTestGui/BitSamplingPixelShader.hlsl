
SamplerState texSampler : register(s0);
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
Texture2D tex2 : register(t2);
Texture2D tex3 : register(t3);

struct PsInput{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PsInput input) : SV_TARGET{
	float4 color;

	color.x = tex0.Sample(texSampler, input.tex).r;
	color.y = tex1.Sample(texSampler, input.tex).r;
	color.z = tex2.Sample(texSampler, input.tex).r;
	color.w = tex3.Sample(texSampler, input.tex).r;

	float xPos = input.tex.x * 1024.0f;
	float bitPos = 7.0f - floor(xPos % 8.0f);
	float shiftVal = pow(2.0f, bitPos);

	color = floor(((color * 255) / shiftVal) % 2) * 1.0f;

	return color;
}