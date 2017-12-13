
struct PsInput
{
	float4 position : SV_Position;
	float3 positionWs : AV_Position;
	float3 normalWs : AV_Normal;
	float3 color : AV_Color;
};

float4 main(PsInput input) : SV_Target0
{
	float3 normal = normalize(input.normalWs);
	float3 light = normalize(float3(10.0f, 10.0f, -10.0f));
	float nl = saturate(dot(normal, light));

	return float4(nl * input.color, 1.0f);
}
