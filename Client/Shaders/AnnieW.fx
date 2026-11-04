#include "00. Global.fx"
#include "00. Light.fx"

struct WVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
    matrix world : INST;
};

struct WPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float3 world : TEXCOORD1;
};

WPixel VS_W(WVertex input)
{
    WPixel output;
    float4 world = mul(input.position, input.world);
    output.position = mul(world, VP);
    output.world = world.xyz;
    output.uv = input.uv;
    output.color = float4(input.color, input.parameters.x);

    return output;
}

SamplerState WClamp
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 PS_W(WPixel input) : SV_TARGET
{
    float age = Material.ambient.x;
    float mode = Material.ambient.y;
    float phase = Material.ambient.z;
    float erosion = Material.ambient.w;

    if (mode > 2.5f)
        return DiffuseMap.Sample(WClamp, input.uv) * input.color;

    float2 offset = input.world.xz - Material.specular.xz;
    float radius = length(offset);
    float forward = dot(offset, Material.emissive.xy);
    float side = dot(offset, float2(Material.emissive.y, -Material.emissive.x));
    float edge = forward - radius * Material.emissive.z;

    float sector = smoothstep(0.f, Material.specular.w * .01375f, edge) *
                   (1.f - smoothstep(Material.specular.w * .96f, Material.specular.w, radius));
    clip(sector - .001f);
    float2 noiseUV = input.uv * float2(1.8f, 1.2f) + float2(phase, -age * .7f);
    float3 noise = NormalMap.Sample(LinearSampler, noiseUV).rgb;
    float4 texel;

    if (mode < .5f)
    {
        float2 groundUV =
            float2(.5f + side / (Material.specular.w * .9375f), 1.f - forward / (Material.specular.w * 1.05f));
        texel = DiffuseMap.Sample(WClamp, groundUV);
        texel.rgb *= .5f + noise.r * 1.4f;
        texel.a *= saturate(texel.r * 2.f);
    }
    else if (mode < 1.5f)
    {
        float2 uv = input.uv + float2(0.f, -age * 1.3f + phase);
        texel = DiffuseMap.Sample(LinearSampler, uv);
        float mask = SpecularMap.Sample(WClamp, input.uv).r;
        texel.a *= mask * smoothstep(erosion * .6f - .20f, erosion * .6f + .08f, noise.g);
    }
    else
    {
        texel = DiffuseMap.Sample(WClamp, input.uv);
        texel.a *= smoothstep(erosion * .65f - .22f, erosion * .65f + .06f, noise.b);
    }

    float4 color = texel * input.color * Material.diffuse;
    color.a *= sector;
    clip(color.a - .001f);

    return color;
}

float4 PS_WDense(WPixel input) : SV_TARGET
{
    float4 color = PS_W(input);
    color.a = saturate(pow(saturate(color.a), .8f) * 1.9f);
    color.rgb *= color.a * 1.35f;

    return color;
}

BlendState WAdditive
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
BlendState WDense
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
DepthStencilState WDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ZERO;
    DepthFunc = LESS_EQUAL;
};
RasterizerState WNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};
technique11 T0
{
    pass Flame
    {
        SetBlendState(WAdditive, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(WDepth, 0);
        SetRasterizerState(WNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_W()));
        SetPixelShader(CompileShader(ps_5_0, PS_W()));
    }

    pass DenseFlame
    {
        SetBlendState(WDense, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(WDepth, 0);
        SetRasterizerState(WNoCull);
        SetVertexShader(CompileShader(vs_5_0, VS_W()));
        SetPixelShader(CompileShader(ps_5_0, PS_WDense()));
    }
}
