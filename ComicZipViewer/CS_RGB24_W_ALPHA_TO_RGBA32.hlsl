
Texture2D<float> rgb24Texture;
Texture2D<float> alphaTexture;
RWTexture2D<float4> rgba32Texture;

[numthreads(1, 1, 1)]
void main(uint3 DTID : SV_DispatchThreadID)
{
    uint2 xy;
    xy = uint2(DTID.x * 3, DTID.y);
    float r = rgb24Texture[xy].r;
    xy.x += 1;
    float g = rgb24Texture[xy].r;
    xy.x += 1;
    float b = rgb24Texture[xy].r;
    float3 rgb = float3(r, g, b);
    rgba32Texture[DTID.xy] = float4(rgb, alphaTexture[DTID.xy].r);
}
