#define PI (3.14159265)
#define G (9.8)
#define PHASE (PI*2)

cbuffer VertexShaderConstantBuffer : register(b0)
{
	float4x4 worldViewProjection;
    float4x4 world;
	float time;
    int waveCount;
    FLOAT crestFactor;
};

cbuffer PixelShaderConstantBuffer : register(b0)
{
    float3 eyePos;
    float reflectivity;
    float3 lightDir;
    float transmittance;
    float3 lightColor;
    float freshnelPower;
    float3 waterColor;
    float freshnelScale;
    float3 etaRatio;
    float freshnelBias;
    float specularFactor;
    float shininess;
};

TextureCube cubeMap : register(t0);

SamplerState anisotropic : register(s0);

struct PS_INPUT
{
	float4 pos : SV_Position;    
    float3 vPos : Texcoord0;
	float3 norm : Texcoord1;
};

struct WAVE
{
    float2 dir;
    float length;
    float amp;
};

Buffer<WAVE> waveBuffer : register(t0);

struct WAVE_SUM
{
    float3 pos;
    float3 norm;
};

WAVE_SUM GerstnerWaveSum(float2 pos, Buffer<WAVE> waves, int n)
{
    WAVE_SUM sum;
    for(int i = 0; i < n; ++i)
    {
        WAVE wave = waves[i];
        float freq = sqrt(G * 2 * PI / wave.length);
        float q = 1/(wave.amp * freq) * crestFactor;        
        float tmp = q * wave.amp * cos(dot(freq * wave.dir, pos) + PHASE * time);
        sum.pos += float3( tmp * wave.dir.x,
                           wave.amp * sin(dot(freq * wave.dir, pos) + PHASE * time),
                           tmp * wave.dir.y );
        tmp = freq * dot(wave.dir, pos) + PHASE * time;
        float s = sin(tmp);
        float c = cos(tmp);
        sum.norm += float3(wave.dir.x * freq * wave.amp * c,
                           q * freq * wave.amp * s,
                           wave.dir.y * freq * wave.amp * c);
    }
    sum.pos.x += pos.x;
    sum.pos.z += pos.y;
    sum.norm.x = -sum.norm.x;
    sum.norm.z = -sum.norm.z;
    sum.norm.y = 1 - sum.norm.y;
    sum.norm = normalize(sum.norm);
    return sum;
}

void WaterVS(float3 pos : POSITION, out PS_INPUT result)
{
    WAVE_SUM waveSum = GerstnerWaveSum(pos.xz, waveBuffer, waveCount);
	result.pos = mul(worldViewProjection, float4(waveSum.pos, 1));
    result.norm = mul(world, float4(waveSum.norm, 1)).xyz;
    result.vPos = mul(world, float4(waveSum.pos, 1)).xyz;
}

void SkyVS(float3 pos : POSITION, out float4 oPos : SV_Position, out float3 vPos : TEXCOORD)
{
    oPos = mul(worldViewProjection, float4(pos, 1));
    vPos = mul(world, float4(pos, 1));
}

void WaterPS(PS_INPUT input, out float4 color : SV_Target)
{
    float3 p = input.vPos;
    float3 n = normalize(input.norm);
    float3 i = normalize(p - eyePos);
    float3 r = reflect(i, n);
    float3 l = normalize(lightDir);

    float3 tRed = refract(i, n, etaRatio.r);
    float3 tGreen = refract(i, n, etaRatio.g);
    float3 tBlue = refract(i, n, etaRatio.b);
    
    float reflectionFactor = freshnelBias +
                             freshnelScale * pow(1 + dot(i, n),
                                                 freshnelPower);

    float3 reflectedColor = lerp(waterColor, cubeMap.Sample(anisotropic, r), reflectivity);
    
    float3 refractedColor = lerp(waterColor,
                                 float3(cubeMap.Sample(anisotropic, tRed).r,
                                        cubeMap.Sample(anisotropic, tGreen).g,
                                        cubeMap.Sample(anisotropic, tBlue).b),
                                 transmittance);

    float3 specularColor =  lightColor * specularFactor * pow(dot(reflect(l, n), i), shininess); 

    color.a = 1;
    color.rgb = lerp(refractedColor,
                     reflectedColor,
                     reflectionFactor) +
                specularColor;
}

void SkyPS(float4 pos : SV_Position, float3 vPos : TEXCOORD, out float4 color : SV_Target)
{
    color = cubeMap.Sample(anisotropic, vPos - eyePos);
}
