cbuffer cbPerObject {
    float4x4 wvp;
};

struct VS_INPUT {
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUTPUT vs(VS_INPUT input) {
    input.position.w = 1.0f;
    
    VS_OUTPUT output;
    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;
    return output;
}