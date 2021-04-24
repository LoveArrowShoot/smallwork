#include "Basic.hlsli"

// 像素着色器(2D)
float4 PS_2D(VertexPosHTex pIn) : SV_Target
{
    
    float4 texColor = gTexArray.Sample(g_SamLinear, float3(pIn.Tex, g_Pad1));
    return texColor;
}