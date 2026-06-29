// 3D lit pipeline, vertex stage. Transform by model then view_proj.
//
// SDL_GPU HLSL binding model: vertex uniform buffers live in space1, vertex
// inputs are matched by location via TEXCOORDn semantics (n -> location n).
struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
};

cbuffer Frame : register(b0, space1) { float4x4 view_proj; };
cbuffer Draw  : register(b1, space1) { float4x4 model; };

VSOutput main(VSInput input) {
    VSOutput output;
    float4 world = mul(model, float4(input.position, 1.0));
    output.position = mul(view_proj, world);
    output.normal = mul(model, float4(input.normal, 0.0)).xyz;
    return output;
}
