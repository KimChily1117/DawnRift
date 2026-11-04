#include "00. Global.fx"
#include "00. Light.fx"

struct BarVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
    matrix world : INST;
};

struct BarPixel
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

BarPixel VS_Bar(BarVertex input)
{
    BarPixel output;
    output.position = float4(input.position.xy, 0.0f, 1.0f);
    output.color = float4(input.color, input.parameters.x);

    return output;
}

float4 PS_Bar(BarPixel input) : SV_TARGET
{
    return input.color;
}

BlendState BarAlpha
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
DepthStencilState BarNoDepth
{
    DepthEnable = FALSE;
    DepthWriteMask = ZERO;
};
RasterizerState BarNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};

technique11 T0{pass Bar{SetBlendState(BarAlpha, float4(0, 0, 0, 0), 0xffffffff);
SetDepthStencilState(BarNoDepth, 0);
SetRasterizerState(BarNoCull);
SetVertexShader(CompileShader(vs_5_0, VS_Bar()));
SetPixelShader(CompileShader(ps_5_0, PS_Bar()));
}
}
;
