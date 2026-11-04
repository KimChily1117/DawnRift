#ifndef _GLOBAL_FX_
#define _GLOBAL_FX_

cbuffer GlobalBuffer
{
    matrix V;
    matrix P;
    matrix VP;
    matrix VInv;
};

cbuffer TransformBuffer
{
    matrix W;
};

cbuffer UIFillMount_HP
{
    float FillAmount_Hp;
};

cbuffer UIFillMount_MP
{
    float FillAmount_Mp;
};

cbuffer UIFillMount_Q
{
    float FillAmount_Q;
};

cbuffer UIFillMount_W
{
    float FillAmount_W;
};

cbuffer UIFillMount_E
{
    float FillAmount_E;
};

cbuffer UIFillMount_R
{
    float FillAmount_R;
};

struct Vertex
{
    float4 position : POSITION;
};

struct VertexTexture
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VertexColor
{
    float4 Position : POSITION;
    float4 Color : COLOR;
};

struct VertexTextureNormal
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct VertexTextureNormalTangent
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VertexTextureNormalTangentBlend
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct MeshOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION1;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState PointSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Wrap;
    AddressV = Wrap;
};

RasterizerState FillModeWireFrame
{
    FillMode = WireFrame;
};

RasterizerState FrontCounterClockwiseTrue
{
    FrontCounterClockwise = true;
};

#define PASS_VP(name, vs, ps)                                                                                          \
    pass name                                                                                                          \
    {                                                                                                                  \
        SetVertexShader(CompileShader(vs_5_0, vs()));                                                                  \
        SetPixelShader(CompileShader(ps_5_0, ps()));                                                                   \
    }

#define PASS_RS_VP(name, rs, vs, ps)                                                                                   \
    pass name                                                                                                          \
    {                                                                                                                  \
        SetRasterizerState(rs);                                                                                        \
        SetVertexShader(CompileShader(vs_5_0, vs()));                                                                  \
        SetPixelShader(CompileShader(ps_5_0, ps()));                                                                   \
    }

float3 CameraPosition()
{
    return VInv._41_42_43;
}

#endif
