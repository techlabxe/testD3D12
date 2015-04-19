struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

float4 main(VSOutput vsout) : SV_TARGET
{
	return vsout.Color;
}