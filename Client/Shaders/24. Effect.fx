// 필요한 리소스 등록 (누락된 부분)
Texture2D DiffuseMap : register(t0);
SamplerState LinearSampler : register(s0);

// ParticleBuffer
cbuffer ParticleBuffer : register(b0)
{
    float4 startColor;
    float4 endColor;
    float duration;
    float time;
    float3 padding;
};

// GlobalBuffer
cbuffer GlobalBuffer : register(b1)
{
    matrix View;
    matrix Projection;
    matrix VP;
};

// InstancingBuffer (Transform 행렬 배열)
cbuffer InstancingBuffer : register(b2)
{
    matrix transforms[1000];
};

// 입력 구조
struct VS_IN
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    uint instanceID : SV_InstanceID;
};

// 출력 구조
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

// 정점 쉐이더
VS_OUT VS(VS_IN input)
{
    VS_OUT output;

    matrix world = transforms[input.instanceID];
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPos, VP);

    output.uv = input.uv;

    float t = saturate(time / duration);
    output.color = lerp(startColor, endColor, t);

    return output;
}

// 픽셀 쉐이더
float4 PS(VS_OUT input) : SV_Target
{
    float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);

    return texColor * input.color;
}

// 테크닉
technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));
    }
}
