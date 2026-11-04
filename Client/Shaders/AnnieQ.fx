#include "00. Global.fx"
#include "00. Light.fx"

struct QVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
    matrix world : INST;
};

struct QPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 parameters : TEXCOORD1;
};

QPixel VS_Q(QVertex input)
{
    QPixel output;
    output.position = mul(mul(input.position, input.world), VP);
    output.uv = input.uv;
    output.color = float4(input.color, input.parameters.x);
    output.parameters = input.parameters.yz;

    return output;
}

SamplerState QClamp
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 PS_Q(QPixel input) : SV_TARGET
{
    float4 texel = DiffuseMap.Sample(QClamp, input.uv);

    float noise = NormalMap.Sample(QClamp, input.uv).r;
    float erosion = 1.0f - smoothstep(noise - 0.08f, noise + 0.08f, input.parameters.x);
    float3 multiply = SpecularMap.Sample(QClamp, input.uv).rgb;
    float4 color = texel * input.color;
    color.rgb *= lerp(float3(1, 1, 1), multiply * 2.0f, Material.ambient.y);
    color.a *= lerp(1.0f, erosion, Material.ambient.x);

    clip(color.a - 0.001f);

    return color;
}

float4 DenseQ(float4 color)
{
    color.a *= saturate(max(color.r, max(color.g, color.b)) * 4.f);
    color.a = saturate(pow(saturate(color.a), .8f) * 1.6f);
    color.rgb *= color.a * 1.3f;

    return color;
}

float4 PS_QDense(QPixel input) : SV_TARGET
{
    return DenseQ(PS_Q(input));
}

float4 PS_Core(QPixel input) : SV_TARGET
{
    float2 scrollingUV = input.uv + Material.specular.xy;
    float4 first = DiffuseMap.Sample(LinearSampler, scrollingUV);
    float4 second = DiffuseMap.Sample(LinearSampler, scrollingUV + float2(0.f, 0.5f));

    float wrapV = frac(scrollingUV.y);
    first.a *= smoothstep(0.0f, 0.2f, wrapV) * (1.0f - smoothstep(0.8f, 1.0f, wrapV));
    wrapV = frac(scrollingUV.y + 0.5f);
    second.a *= smoothstep(0.0f, 0.2f, wrapV) * (1.0f - smoothstep(0.8f, 1.0f, wrapV)) * 0.7f;

    float alpha = first.a + second.a;
    float4 color = float4((first.rgb * first.a + second.rgb * second.a) / max(alpha, 0.001f), saturate(alpha));
    color *= input.color * Material.diffuse;
    clip(color.a - 0.001f);

    return DenseQ(color);
}

BlendState QAdditive
{
    BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = ONE;
    BlendOp = ADD;
    SrcBlendAlpha = ONE;
    DestBlendAlpha = ONE;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};
BlendState QAlpha
{
    BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ONE;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};
BlendState QDense
{
    BlendEnable[0] = TRUE;
    SrcBlend = ONE;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ONE;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};
DepthStencilState QDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ZERO;
    DepthFunc = LESS_EQUAL;
};
RasterizerState QNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};
technique11 T0
{
    pass Additive
    {
        SetBlendState(QAdditive, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(QDepth, 0);
        SetRasterizerState(QNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_Q()));
        SetPixelShader(CompileShader(ps_5_0, PS_Q()));
    }

    pass Alpha
    {
        SetBlendState(QAlpha, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(QDepth, 0);
        SetRasterizerState(QNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_Q()));
        SetPixelShader(CompileShader(ps_5_0, PS_Q()));
    }

    pass Core
    {
        SetBlendState(QDense, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(QDepth, 0);
        SetRasterizerState(QNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_Q()));
        SetPixelShader(CompileShader(ps_5_0, PS_Core()));
    }

    pass DenseFlame
    {
        SetBlendState(QDense, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(QDepth, 0);
        SetRasterizerState(QNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_Q()));
        SetPixelShader(CompileShader(ps_5_0, PS_QDense()));
    }
}
