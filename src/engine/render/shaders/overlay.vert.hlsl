// 2D overlay pipeline, vertex stage. Screen-space quads through an ortho proj.
struct VSInput {
    float2 pos   : TEXCOORD0;
    float2 uv    : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

cbuffer Frame : register(b0, space1) { float4x4 proj; };

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(proj, float4(input.pos, 0.0, 1.0));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
