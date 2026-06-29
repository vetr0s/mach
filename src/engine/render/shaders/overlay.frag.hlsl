// 2D overlay pipeline, fragment stage. Samples the R8 font atlas; the atlas's
// white texel lets the same shader draw solid rects (sample == 1).
//
// Fragment sampled textures + samplers live in space2.
struct PSInput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

Texture2D<float4> atlas : register(t0, space2);
SamplerState      smp   : register(s0, space2);

float4 main(PSInput input) : SV_Target {
    float a = atlas.Sample(smp, input.uv).r;
    return float4(input.color.rgb, input.color.a * a);
}
