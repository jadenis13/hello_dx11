Texture2D shaderTexture;
SamplerState sampleType;

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    float4 textureColor;
    textureColor = shaderTexture.Sample(sampleType, input.texcoord);
    return textureColor;
}