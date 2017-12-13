#define RootSig \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
	"SRV(t0, visibility = SHADER_VISIBILITY_VERTEX), " \
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)"

//"RootConstants(b0, num32BitConstants = 2)"

struct InstanceConst
{
	float4x4 objectToWorld;
	uint color;
};
StructuredBuffer<InstanceConst> srv_InstanceConst : register(t0);

struct FrameConst
{
	float4x4 worldToProj;
};
ConstantBuffer<FrameConst> cbv_FrameConst : register(b0);

struct VsInput
{
	float3 position : AV_Position;
	float3 normal : AV_Normal;
	uint instanceId : SV_InstanceID;
};

struct VsOutput
{
	float4 position : SV_Position;
	float3 positionWs : AV_Position;
	float3 normalWs : AV_Normal;
	float3 color : AV_Color;
};

[RootSignature(RootSig)]
VsOutput main(VsInput input)
{
	VsOutput output;

	uint color = srv_InstanceConst[input.instanceId].color;
	float r = ((color >> 16) & 0xff) / 255.0f;
	float g = ((color >> 8) & 0xff) / 255.0f;
	float b = (color & 0xff) / 255.0f;

	float4x4 objectToWorld = srv_InstanceConst[input.instanceId].objectToWorld;
	float4 positionWs = mul(float4(input.position, 1.0f), objectToWorld);
	float3 normalWs = mul(input.normal, (float3x3)objectToWorld);

	output.position = mul(positionWs, cbv_FrameConst.worldToProj);
	output.positionWs = positionWs.xyz;
	output.normalWs = normalWs;
	output.color = float3(r, g, b);

	return output;
}
