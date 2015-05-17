struct VSOutput {
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float2 UV0   : TEXCOORD;
};
Texture2D<float4> tex0 : register(t0);
SamplerState samp0 : register(s0);

float4 main(VSOutput vsout) : SV_TARGET
{
  return tex0.Sample(samp0, vsout.UV0);
}