#include "GuiCommon.hlsli"

struct Transform
{
	float4x4 mat;
};
ConstantBuffer<Transform> cbv_Transform : register(b0);

[RootSignature(RsImGui)]
VsOutput main(VsInput input)
{
	VsOutput output;
	output.position = mul(cbv_Transform.mat, float4(input.position, 0.0f, 1.0f));
	output.texcoord = input.texcoord;
	output.color = input.color;
	return output;
}
