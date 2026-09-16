#include "common.hlsl"

cbuffer BoneBuffer : register(b6)
{
    matrix BoneMatrix[256];
}

struct VS_IN_SKIN
{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    uint4 BoneIndex : BLENDINDICES0;
    float4 BoneWeight : BLENDWEIGHT0;
};

void main(in VS_IN_SKIN In, out PS_IN Out)
{
    matrix skin = BoneMatrix[In.BoneIndex[0]] * In.BoneWeight[0]
	            + BoneMatrix[In.BoneIndex[1]] * In.BoneWeight[1]
	            + BoneMatrix[In.BoneIndex[2]] * In.BoneWeight[2]
	            + BoneMatrix[In.BoneIndex[3]] * In.BoneWeight[3];

    float4 pos = mul(In.Position, skin);

    float4 normal = float4(In.Normal.xyz, 0.0f); // wを0にして移動を無視
    normal = mul(normal, skin);

    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);

    Out.Position = mul(pos, wvp);
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse * Material.Diffuse;
	// ライティングするなら normal を litVS と同じように使う
}