#define PI (3.14159265)
#define G (9.8)
#define PHASE (PI*2)
#define ETA_RATIO (1/1.3333)
#define SHININESS (50)

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
    float d;
    float3 lightPos;
    float s;
    float3 lightColor;
    float reflectivity;
    float3 waterColor;
    float transmittance;
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

    float3 l = normalize(lightPos - p);
    float diffuseLight = max(dot(n,l), 0);
    float3 diffuse = d * waterColor * diffuseLight;

    float3 v = normalize(eyePos - p);
    float3 h = normalize(l + v);
    float specularLight = pow(max(dot(n, h), 0), SHININESS);
    if(diffuseLight <= 0) specularLight = 0;
    float3 specular = s * lightColor * specularLight;

    float3 decalColor = diffuse + specular;    
    float3 reflectionColor = cubeMap.Sample(anisotropic, reflect(normalize(p - eyePos), n));
    float3 refractionColor = cubeMap.Sample(anisotropic, refract(normalize(p - eyePos), n, ETA_RATIO));

    color.a = 1;
    color.rgb = lerp(decalColor, refractionColor, transmittance);
    color.rgb = lerp(color.xyz, reflectionColor, reflectivity);
}

void SkyPS(float4 pos : SV_Position, float3 vPos : TEXCOORD, out float4 color : SV_Target)
{
    color = cubeMap.Sample(anisotropic, vPos - eyePos);
}
