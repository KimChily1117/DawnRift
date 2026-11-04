#include "00. Global.fx"
#include "00. Light.fx"

struct HudVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
};

struct HudPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

HudPixel VS_Hud(HudVertex input)
{
    HudPixel output;
    output.position = float4(input.position.xy, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = float4(input.color, input.parameters.x);

    return output;
}

SamplerState HudSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 PS_Hud(HudPixel input) : SV_TARGET
{
    float4 texel = input.uv.x < -0.5f ? float4(1.0f, 1.0f, 1.0f, 1.0f) : DiffuseMap.Sample(HudSampler, input.uv);
    float4 color = texel * input.color;
    clip(color.a - 0.004f);

    return color;
}

BlendState HudAlpha
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
DepthStencilState HudNoDepth
{
    DepthEnable = FALSE;
    DepthWriteMask = ZERO;
};
RasterizerState HudNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};

technique11 T0{pass Hud{SetBlendState(HudAlpha, float4(0, 0, 0, 0), 0xffffffff);
SetDepthStencilState(HudNoDepth, 0);
SetRasterizerState(HudNoCull);
SetVertexShader(CompileShader(vs_5_0, VS_Hud()));
SetPixelShader(CompileShader(ps_5_0, PS_Hud()));
}
}
;
