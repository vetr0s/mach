// 3D lit pipeline, fragment stage. One directional light plus constant ambient.
//
// Fragment uniform buffers live in space3. float4 used throughout to dodge the
// std140/packing differences around float3.
struct PSInput {
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
};

cbuffer Light : register(b0, space3) { float4 light_dir; float4 ambient; };
cbuffer Color : register(b1, space3) { float4 rgba; };

float4 main(PSInput input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 L = normalize(-light_dir.xyz);
    float ndl = max(dot(N, L), 0.0);
    float lit = ambient.x + (1.0 - ambient.x) * ndl;
    return float4(rgba.rgb * lit, rgba.a);
}
