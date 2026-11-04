#include "00. Global.fx"
#include "00. Light.fx"

struct RangeVertex
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    matrix world : INST;
};

struct RangePixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

RangePixel VS_Range(RangeVertex input)
{
    RangePixel output;
    output.position = mul(mul(input.position, input.world), VP);
    output.uv = input.uv;

    return output;
}

float4 PS_Range(RangePixel input) : SV_TARGET
{
    bool circle = Material.ambient.x > .5f;

    if (Material.ambient.x < -.5f)
    {
        float2 p = float2((input.uv.x - .5f) * 2.f, 1.f - input.uv.y);
        float insideLine = min((1.f - abs(p.x)) * Material.ambient.y, min(p.y, 1.f - p.y));
        float aaLine = max(fwidth(insideLine), .001f);
        float coverageLine = smoothstep(0.f, aaLine, insideLine);
        float outlineLine = 1.f - smoothstep(Material.ambient.w, Material.ambient.w + aaLine, insideLine);
        float alphaLine = coverageLine * (.10f + .90f * outlineLine) * Material.diffuse.a;
        clip(alphaLine - .002f);

        return float4(Material.diffuse.rgb, alphaLine);
    }

    float2 localPoint =
        circle ? (input.uv - .5f) * 2.f : float2((input.uv.x - .5f) * 2.f * Material.ambient.y, 1.f - input.uv.y);
    float radialDistance = length(localPoint);
    float inside = 1.f - radialDistance;

    if (!circle)
        inside = min(inside, localPoint.y * Material.ambient.y - abs(localPoint.x) * Material.ambient.z);
    float aa = max(fwidth(inside), .001f);
    float coverage = smoothstep(0.f, aa, inside);
    float outline = 1.f - smoothstep(Material.ambient.w, Material.ambient.w + aa, inside);
    float fill = circle ? .025f : .10f;
    float alpha = coverage * (fill + (1.f - fill) * outline) * Material.diffuse.a;
    clip(alpha - .002f);

    return float4(Material.diffuse.rgb, alpha);
}

BlendState RangeAlpha
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
DepthStencilState RangeDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ZERO;
    DepthFunc = LESS_EQUAL;
};
RasterizerState RangeRaster
{
    FillMode = SOLID;
    CullMode = NONE;
};
technique11 T0
{
    pass Guide
    {
        SetBlendState(RangeAlpha, float4(0, 0, 0, 0), 0xffffffff);
        SetDepthStencilState(RangeDepth, 0);
        SetRasterizerState(RangeRaster);
        SetVertexShader(CompileShader(vs_5_0, VS_Range()));
        SetPixelShader(CompileShader(ps_5_0, PS_Range()));
    }
}
