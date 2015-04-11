struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

VSOutput main( 
	float4 pos : POSITION,
	float4 color : COLOR
	)
{
	VSOutput vsout = (VSOutput)0;
	vsout.Position = pos;
	vsout.Color = color;
	return vsout;
}