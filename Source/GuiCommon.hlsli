#define RsImGui \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
	"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, visibility = SHADER_VISIBILITY_PIXEL)"

struct VsInput
{
	float2 position : AV_Position;
	float2 texcoord : AV_Texcoord0;
	float4 color : AV_Color;
};

struct VsOutput
{
	float4 position : SV_Position;
	float2 texcoord : AV_Texcoord0;
	float4 color : AV_Color;
};
