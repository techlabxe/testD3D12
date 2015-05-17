struct VSOutput {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV0   : TEXCOORD;
};
float4x4 mtx;

VSOutput main(
    float4 pos : POSITION,
    float4 color : COLOR,
    float2 UV0 : TEXCOORD
    )
{
    VSOutput vsout = (VSOutput)0;
    vsout.Position = mul(mtx, pos);
    vsout.Color = color;
    vsout.UV0 = UV0;
    return vsout;
}