#include "00. Global.fx"
#include "00. Light.fx"

struct TextVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 color : NORMAL;
    float3 parameters : TANGENT;
    matrix world : INST;
};

struct TextPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 parameters : TEXCOORD1;
};

TextPixel VS_Text(TextVertex input)
{
    TextPixel output;
    output.position = float4(input.position.xy, 0.0f, 1.0f);
    output.uv = input.uv;
    output.parameters = input.parameters.xy;

    return output;
}

SamplerState TextSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 PS_Text(TextPixel input) : SV_TARGET
{
    float4 coverage = DiffuseMap.Sample(TextSampler, input.uv);
    float3 fill = lerp(Material.diffuse.rgb, Material.specular.rgb, saturate(input.parameters.y));
    float3 rgb = lerp(Material.emissive.rgb, fill, coverage.r);
    float alpha = coverage.g * input.parameters.x;
    clip(alpha - 0.004f);

    return float4(rgb, alpha);
}

BlendState TextAlpha
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
DepthStencilState TextNoDepth
{
    DepthEnable = FALSE;
    DepthWriteMask = ZERO;
};
RasterizerState TextNoCull
{
    FillMode = SOLID;
    CullMode = NONE;
};

technique11 T0{pass Text{SetBlendState(TextAlpha, float4(0, 0, 0, 0), 0xffffffff);
SetDepthStencilState(TextNoDepth, 0);
SetRasterizerState(TextNoCull);
SetVertexShader(CompileShader(vs_5_0, VS_Text()));
SetPixelShader(CompileShader(ps_5_0, PS_Text()));
}
}
;
