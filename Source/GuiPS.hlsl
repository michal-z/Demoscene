#include "GuiCommon.hlsli"

Texture2D srv_GuiImage : register(t0);
SamplerState sam_GuiSampler : register(s0);

[RootSignature(RsImGui)]
float4 main(VsOutput input) : SV_Target0
{
	return input.color * srv_GuiImage.Sample(sam_GuiSampler, input.texcoord);
}
