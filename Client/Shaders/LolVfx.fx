#include "00. Global.fx"
#include "00. Light.fx"

struct VfxVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
    matrix world : INST;
};

struct VfxPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 erosion : TEXCOORD1;
};

VfxPixel VS_Vfx(VfxVertex input)
{
    VfxPixel output;
    output.position = mul(mul(input.position, input.world), VP);
    output.uv = input.uv;
    output.color = float4(input.color, input.parameters.x);
    output.erosion = input.parameters.yz;

    return output;
}

SamplerState VfxClamp
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};
SamplerState VfxWrap
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

float4 PS_Vfx(VfxPixel input) : SV_TARGET
{
    float4 color =
        (Material.ambient.z > 0.5f ? DiffuseMap.Sample(VfxWrap, input.uv) : DiffuseMap.Sample(VfxClamp, input.uv)) *
        input.color;

    if (Material.ambient.y > 0.5f)
        color *= SpecularMap.Sample(VfxClamp, input.uv);

    if (Material.ambient.x > 0.5f)
    {
        float value = dot(NormalMap.Sample(VfxClamp, input.uv), Material.diffuse);
        color.a *= smoothstep(input.erosion.x, input.erosion.x + input.erosion.y, value);
    }

    clip(color.a - 0.002f);

    return color;
}

BlendState VfxAlpha
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
BlendState VfxAdditive
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
DepthStencilState VfxDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ZERO;
    DepthFunc = LESS_EQUAL;
};
RasterizerState VfxNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};

technique11 T0{pass Alpha{SetBlendState(VfxAlpha, float4(0, 0, 0, 0), 0xffffffff);
SetDepthStencilState(VfxDepth, 0);
SetRasterizerState(VfxNoCull);
SetVertexShader(CompileShader(vs_5_0, VS_Vfx()));
SetPixelShader(CompileShader(ps_5_0, PS_Vfx()));
}

pass Additive
{
    SetBlendState(VfxAdditive, float4(0, 0, 0, 0), 0xffffffff);
    SetDepthStencilState(VfxDepth, 0);
    SetRasterizerState(VfxNoCull);
    SetVertexShader(CompileShader(vs_5_0, VS_Vfx()));
    SetPixelShader(CompileShader(ps_5_0, PS_Vfx()));
}
}
;
