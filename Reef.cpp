#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef INITGUID
#define INITGUID
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <vector>
#include <sstream>
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>

#define SAFE_RELEASE(p) do{if(p) (p)->Release(); (p) = NULL;}while(0);
#define V_HR(x, msg) do{hr = (x); if(FAILED(hr)) throw Exception(hr, msg);}while(0);

#define MS_COUNT 1
#define MS_QUALITY 0
#define AF 16
#define SHADERS_FILENAME L"Reef.hlsl"
#define CUBEMAP_FILENAME L"Reef.dds"
#define MESH_PATCHES_X 50
#define MESH_PATCHES_Z 50

struct Exception
{
    HRESULT hr;
    LPCSTR msg;
    Exception(HRESULT _hr, LPCSTR _msg)
        : hr(_hr), msg(_msg)
    {
    }
};

struct VertexShaderConstantBuffer
{
	XMMATRIX worldViewProjection;
    XMMATRIX world;
	FLOAT time;
    INT waveCount;
    FLOAT crestFactor;
};

struct PixelShaderConstantBuffer
{
    XMFLOAT3 eyePos;    
    FLOAT d;
    XMFLOAT3 lightPos;
    FLOAT s;
    XMFLOAT3 lightColor;
    FLOAT reflectivity;
    XMFLOAT3 waterColor;
    FLOAT transmittance;
};

struct Wave
{
    XMFLOAT2 dir;
    FLOAT length;
    FLOAT crestFactor;
};

void InitWindow();
void InitDevice();
void InitCamera();
void InitShaders();
void InitGeometry();
void InitResources();
void Cleanup();
void Render();
void ResizeBuffers();
INT64 GetCounter();
INT64 GetFrequency();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND window = NULL;
ID3D11Device * device = NULL;
ID3D11DeviceContext * deviceContext = NULL;
IDXGISwapChain * swapChain = NULL;
ID3D11RenderTargetView * backBufferRTV = NULL;
ID3D11DepthStencilView * depthStencilView = NULL;
ID3D11InputLayout * inputLayout = NULL;
ID3D11VertexShader * waterVS = NULL;
ID3D11VertexShader * skyVS = NULL;
ID3D11PixelShader * waterPS = NULL;
ID3D11PixelShader * skyPS = NULL;
ID3D11Buffer * waterVB = NULL;
ID3D11Buffer * skyVB = NULL;
ID3D11Buffer * waterIB = NULL;
ID3D11Buffer * skyIB = NULL;
ID3D11SamplerState * anisotropicSampler = NULL;
ID3D11ShaderResourceView * cubeMapSRV = NULL;
ID3D11ShaderResourceView * waveBufferSRV = NULL;
ID3D11Buffer * vsCB = NULL;
ID3D11Buffer * psCB = NULL;

XMMATRIX waterWorld;
INT64 counter;
FLOAT time = 0.0f;
FLOAT waveInterval = 4;

UINT width;
UINT height;
D3D11_VIEWPORT viewport;
XMFLOAT3 eyePos;
XMMATRIX view;
XMMATRIX projection;

BOOL paused = FALSE;

INT WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, INT cmdShow)
{
    try
    {
        InitWindow();
        InitDevice();
        InitCamera();
        InitShaders();
        InitGeometry();
        InitResources();
        counter = GetCounter();
        ShowWindow(window, SW_SHOWNORMAL);
        MSG msg = {0};
        while(WM_QUIT != msg.message)
        {
            if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                Render();
            }
        }
    } catch(Exception e)
    {
        std::stringstream s;
        s << "HRESULT: 0x" << std::hex << e.hr << std::endl << e.msg;
        MessageBoxA(NULL, s.str().c_str(), "Error", MB_ICONERROR);
        Cleanup();
        return 1;
    }
    Cleanup();
    return 0;
}

void Render()
{
    if(!paused)
    {        
        FLOAT dt = (GetCounter() - counter)
                   / (FLOAT)GetFrequency()
                   / waveInterval;
        counter = GetCounter();

        time += dt;
        if(time > 1.0f)
            time = 0.0f;

        deviceContext->OMSetRenderTargets(1, &backBufferRTV, depthStencilView);

        FLOAT clearColor[4] = {0.0f, 0.5f, 1.0f, 1.0f};
        deviceContext->ClearRenderTargetView(backBufferRTV, clearColor);
        deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

        deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        deviceContext->IASetInputLayout(inputLayout);

        deviceContext->RSSetViewports(1, &viewport);

        deviceContext->VSSetConstantBuffers(0, 1, &vsCB);
        deviceContext->VSSetShaderResources(0, 1, &waveBufferSRV);

        deviceContext->PSSetConstantBuffers(0, 1, &psCB);
        deviceContext->PSSetShaderResources(0, 1, &cubeMapSRV);
        deviceContext->PSSetSamplers(0, 1, &anisotropicSampler);

        UINT stride = sizeof(XMFLOAT3);
        UINT offset = 0;

        VertexShaderConstantBuffer vsBuffer;
        vsBuffer.time = time;
        vsBuffer.crestFactor = 0.3f;
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        waveBufferSRV->GetDesc(&srvDesc);
        vsBuffer.waveCount = srvDesc.Buffer.NumElements;

        PixelShaderConstantBuffer psBuffer;       
        psBuffer.eyePos = eyePos;
        psBuffer.lightColor = XMFLOAT3(1, 1, 0.8f);
        psBuffer.lightPos = XMFLOAT3(0, 1, 0);
        psBuffer.waterColor = XMFLOAT3(0, 0.5f, 1);
        psBuffer.d = 0.7f;
        psBuffer.s = 0.4f;
        psBuffer.reflectivity = 0.3f;
        psBuffer.transmittance = 0.9f;

        // draw skybox
        
        deviceContext->IASetVertexBuffers(0, 1, &skyVB, &stride, &offset);
        deviceContext->IASetIndexBuffer(skyIB, DXGI_FORMAT_R32_UINT, 0);

        vsBuffer.world = XMMatrixIdentity();
        vsBuffer.worldViewProjection = vsBuffer.world * view * projection;
        
        deviceContext->UpdateSubresource(vsCB, 0, NULL, &vsBuffer, 0, 0);
        deviceContext->VSSetShader(skyVS, NULL, 0);

        deviceContext->UpdateSubresource(psCB, 0, NULL, &psBuffer, 0, 0);
        deviceContext->PSSetShader(skyPS, NULL, 0);

        deviceContext->DrawIndexed(36, 0, 0);

        // draw water

        deviceContext->IASetVertexBuffers(0, 1, &waterVB, &stride, &offset);
        deviceContext->IASetIndexBuffer(waterIB, DXGI_FORMAT_R32_UINT, 0);

        vsBuffer.world = waterWorld;
        vsBuffer.worldViewProjection = vsBuffer.world * view * projection;

        deviceContext->UpdateSubresource(vsCB, 0, NULL, &vsBuffer, 0, 0);
        deviceContext->VSSetShader(waterVS, NULL, 0);

        deviceContext->PSSetShader(waterPS, NULL, 0);

        D3D11_BUFFER_DESC bd;
        waterIB->GetDesc(&bd);
        deviceContext->DrawIndexed(bd.ByteWidth / sizeof(DWORD), 0, 0);

    }
    // present scene
    swapChain->Present(0, 0);    
}

void InitDevice()
{
    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC sd;
    sd.Flags = 0;
    sd.Windowed = TRUE;
    sd.OutputWindow = window;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;    
    sd.SampleDesc.Count = MS_COUNT;
    sd.SampleDesc.Quality = MS_QUALITY;
    sd.BufferCount = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel;
    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE
    };
    DWORD createFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(DEBUG) || defined(_DEBUG)
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    for(int i = 0; i < ARRAYSIZE(driverTypes); ++i)
    {
        if(SUCCEEDED(hr = D3D11CreateDeviceAndSwapChain(
                            NULL,
                            driverTypes[i],
                            NULL,
                            createFlags,
                            featureLevels,
                            ARRAYSIZE(featureLevels),
                            D3D11_SDK_VERSION,
                            &sd,
                            &swapChain,
                            &device,
                            &featureLevel,
                            &deviceContext)))
            break;
    }
    V_HR(hr, "Unable to create device and swap chain.");

    ID3D11Texture2D * backBuffer = NULL;
    V_HR(swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer),
         "Unable to get swap chain back buffer.");

    hr = device->CreateRenderTargetView(backBuffer, NULL, &backBufferRTV);
    SAFE_RELEASE(backBuffer);
    V_HR(hr, "Unable to create render target view for back buffer.");

    D3D11_TEXTURE2D_DESC dsDesc;
    dsDesc.ArraySize = 1;
    dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dsDesc.CPUAccessFlags = 0;
    dsDesc.MiscFlags = 0;
    dsDesc.MipLevels = 1;
    dsDesc.Usage = D3D11_USAGE_DEFAULT;
    dsDesc.SampleDesc.Count = MS_COUNT;
    dsDesc.SampleDesc.Quality = MS_QUALITY;
    dsDesc.Width = width;
    dsDesc.Height = height;
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;

    ID3D11Texture2D * depthStencil = NULL;
    V_HR(device->CreateTexture2D(&dsDesc, NULL, &depthStencil),
         "Unable to create depth-stencil surface.");

    hr = device->CreateDepthStencilView(depthStencil, NULL, &depthStencilView);
    SAFE_RELEASE(depthStencil);
    V_HR(hr, "Unable to create depth stencil view.");
}

void ResizeBuffers()
{
    HRESULT hr;

    ID3D11Texture2D * depthStencil = NULL;
    depthStencilView->GetResource((ID3D11Resource**)&depthStencil);

    D3D11_TEXTURE2D_DESC td;
    depthStencil->GetDesc(&td);

    SAFE_RELEASE(depthStencil);        
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(backBufferRTV);

    DXGI_SWAP_CHAIN_DESC sd;
    V_HR(swapChain->GetDesc(&sd),
         "Unable to obtain swap chain description before resizing.");

    V_HR(swapChain->ResizeBuffers(sd.BufferCount, width, height, sd.BufferDesc.Format, sd.Flags),
         "Unable to resize swap chain.");

    ID3D11Texture2D * backBuffer = NULL;
    V_HR(swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer),
         "Unable to obtain swap chain backbuffer after resizing.");

    hr = device->CreateRenderTargetView(backBuffer, NULL, &backBufferRTV);
    SAFE_RELEASE(backBuffer);
    V_HR(hr, "Unable to recreate render target view from swap chain backbuffer after resizing.");

    td.Width = width;
    td.Height = height; 
        
    V_HR(device->CreateTexture2D(&td, NULL, &depthStencil),
         "Unable to recreate depth-stencil surface after swap chain resizing.");

    hr = device->CreateDepthStencilView(depthStencil, NULL, &depthStencilView);
    SAFE_RELEASE(depthStencil);
    V_HR(hr, "Unable to recreate depth-stencil view after swap chain resizing.");
    
    viewport.Width = (FLOAT)width;
    viewport.Height = (FLOAT)height;
    
    projection = XMMatrixPerspectiveFovLH(
                    XM_PIDIV4,
                    width / (FLOAT)height,
                    0.001f,
                    100.0f);                   
}

void InitCamera()
{
    viewport.Width = (FLOAT)width;
    viewport.Height = (FLOAT)height;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;

    XMFLOAT3 eye = eyePos = XMFLOAT3(-0.9f, 0.03f, -0.9f);
    XMFLOAT3 to = XMFLOAT3(0.707f, 0, 0.707f);
    XMFLOAT3 up = XMFLOAT3(0, 1, 0);
    view = XMMatrixLookToLH(XMLoadFloat3(&eye),
                            XMLoadFloat3(&to),
                            XMLoadFloat3(&up));    

    projection = XMMatrixPerspectiveFovLH(
                    XM_PIDIV4,
                    width / (FLOAT)height,
                    0.001f,
                    100.0f);
}

void InitShaders()
{
    HRESULT hr;
    ID3DBlob * blob = NULL;
    ID3DBlob * errors = NULL;

    V_HR(D3DX11CompileFromFileW(
            SHADERS_FILENAME,
            NULL,
            NULL,
            "WaterVS",
            "vs_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            NULL,
            &blob,
            &errors,
            NULL),
         errors ? (LPCSTR)errors->GetBufferPointer()
         : "Unable to compile water vertex shader.");
    V_HR(device->CreateVertexShader(
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    NULL,
                    &waterVS),
         "Unable to create water vertex shader from compiled bytecode.");
    D3D11_INPUT_ELEMENT_DESC layoutDesc[1];
    layoutDesc[0].SemanticName = "POSITION";
    layoutDesc[0].SemanticIndex = 0;
    layoutDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    layoutDesc[0].InputSlot = 0;
    layoutDesc[0].AlignedByteOffset = 0;    
    layoutDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    layoutDesc[0].InstanceDataStepRate = 0;
    V_HR(device->CreateInputLayout(
                    layoutDesc,
                    ARRAYSIZE(layoutDesc),
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    &inputLayout),
         "Unable to create input layout from water shader bytecode.");
    SAFE_RELEASE(blob);
    SAFE_RELEASE(errors);

    V_HR(D3DX11CompileFromFileW(
            SHADERS_FILENAME,
            NULL,
            NULL,
            "SkyVS",
            "vs_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            NULL,
            &blob,
            &errors,
            NULL),
         errors ? (LPCSTR)errors->GetBufferPointer()
         : "Unable to compile skybox vertex shader.");
    V_HR(device->CreateVertexShader(
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    NULL,
                    &skyVS),
         "Unable to create skybox vertex shader from compiled bytecode.");
    SAFE_RELEASE(blob);
    SAFE_RELEASE(errors);

    V_HR(D3DX11CompileFromFileW(
            SHADERS_FILENAME,
            NULL,
            NULL,
            "WaterPS",
            "ps_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            NULL,
            &blob,
            &errors,
            NULL),
         errors ? (LPCSTR)errors->GetBufferPointer()
         : "Unable to compile water pixel shader.");
    V_HR(device->CreatePixelShader(
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    NULL,
                    &waterPS),
         "Unable to create water pixel shader from compiled bytecode.");
    SAFE_RELEASE(blob);
    SAFE_RELEASE(errors);

    V_HR(D3DX11CompileFromFileW(
            SHADERS_FILENAME,
            NULL,
            NULL,
            "SkyPS",
            "ps_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            NULL,
            &blob,
            &errors,
            NULL),
         errors ? (LPCSTR)errors->GetBufferPointer()
         : "Unable to compile skybox pixel shader.");
    V_HR(device->CreatePixelShader(
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    NULL,
                    &skyPS),
         "Unable to create skybox pixel shader from compiled bytecode.");
    SAFE_RELEASE(blob);
    SAFE_RELEASE(errors);
}

void InitGeometry()
{
    HRESULT hr;

    D3D11_BUFFER_DESC bd;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    
    D3D11_SUBRESOURCE_DATA sd;
    sd.SysMemPitch = 0;
    sd.SysMemSlicePitch = 0;

    std::vector<XMFLOAT3> vertices;
    std::vector<DWORD> indices;

    for(int i = 0; i < MESH_PATCHES_X; ++i)
    {
        for(int j = 0; j < MESH_PATCHES_Z; ++j)
        {
            vertices.push_back(XMFLOAT3( (2.0f / MESH_PATCHES_X) * (i + 0) - 1.0f,
								         0.0f,
								         (2.0f / MESH_PATCHES_Z) * (j + 0) - 1.0f ));
            vertices.push_back(XMFLOAT3( (2.0f / MESH_PATCHES_X) * (i + 0) - 1.0f,
								         0.0f,
								         (2.0f / MESH_PATCHES_Z) * (j + 1) - 1.0f ));
            vertices.push_back(XMFLOAT3( (2.0f / MESH_PATCHES_X) * (i + 1) - 1.0f,
								         0.0f,
								         (2.0f / MESH_PATCHES_Z) * (j + 1) - 1.0f ));
            vertices.push_back(XMFLOAT3( (2.0f / MESH_PATCHES_X) * (i + 1) - 1.0f,
								         0.0f,
								         (2.0f / MESH_PATCHES_Z) * (j + 0) - 1.0f ));

            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 0);
            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 1);
            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 2);
            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 2);
            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 3);
            indices.push_back((i*MESH_PATCHES_Z + j)*4 + 0);
        }
    }

    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = vertices.size() * sizeof(XMFLOAT3);
    sd.pSysMem = vertices.data();
    V_HR(device->CreateBuffer(&bd, &sd, &waterVB),
         "Unable to create water vertex buffer.");
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = indices.size() * sizeof(DWORD);
    sd.pSysMem = indices.data();
    V_HR(device->CreateBuffer(&bd, &sd, &waterIB),
         "Unable to create water index buffer.");

    vertices.resize(0);
    indices.resize(0);

    // front
    vertices.push_back( XMFLOAT3(-1, -1, 1) );
    vertices.push_back( XMFLOAT3(-1, 1, 1) );
    vertices.push_back( XMFLOAT3(1, 1, 1) );
    vertices.push_back( XMFLOAT3(1, -1, 1) );
    indices.push_back(0); indices.push_back(1); indices.push_back(2); 
    indices.push_back(2); indices.push_back(3); indices.push_back(0); 
    
    //back
    vertices.push_back( XMFLOAT3(1, -1, -1) );
    vertices.push_back( XMFLOAT3(1, 1, -1) );
    vertices.push_back( XMFLOAT3(-1, 1, -1) );
    vertices.push_back( XMFLOAT3(-1, -1, -1) );
    indices.push_back(4); indices.push_back(5); indices.push_back(6); 
    indices.push_back(6); indices.push_back(7); indices.push_back(4); 
    
    //top
    vertices.push_back( XMFLOAT3(-1, 1, 1) );
    vertices.push_back( XMFLOAT3(-1, 1, -1) );
    vertices.push_back( XMFLOAT3(1, 1, -1) );
    vertices.push_back( XMFLOAT3(1, 1, 1) );
    indices.push_back(8); indices.push_back(9); indices.push_back(10); 
    indices.push_back(10); indices.push_back(11); indices.push_back(8); 
    
    //bottom
    vertices.push_back( XMFLOAT3(-1, -1, -1) );
    vertices.push_back( XMFLOAT3(-1, -1, 1) );
    vertices.push_back( XMFLOAT3(1, -1, 1) );
    vertices.push_back( XMFLOAT3(1, -1, -1) );
    indices.push_back(12); indices.push_back(13); indices.push_back(14); 
    indices.push_back(14); indices.push_back(15); indices.push_back(12); 
    
    //left
    vertices.push_back( XMFLOAT3(-1, -1, -1) );
    vertices.push_back( XMFLOAT3(-1, 1, -1) );
    vertices.push_back( XMFLOAT3(-1, 1, 1) );
    vertices.push_back( XMFLOAT3(-1, -1, 1) );
    indices.push_back(16); indices.push_back(17); indices.push_back(18); 
    indices.push_back(18); indices.push_back(19); indices.push_back(16); 
    
    //right
    vertices.push_back( XMFLOAT3(1, -1, 1) );
    vertices.push_back( XMFLOAT3(1, 1, 1) );
    vertices.push_back( XMFLOAT3(1, 1, -1) );
    vertices.push_back( XMFLOAT3(1, -1, -1) );
    indices.push_back(20); indices.push_back(21); indices.push_back(22); 
    indices.push_back(22); indices.push_back(23); indices.push_back(20); 

    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = vertices.size() * sizeof(XMFLOAT3);
    sd.pSysMem = vertices.data();
    V_HR(device->CreateBuffer(&bd, &sd, &skyVB),
         "Unable to create skybox vertex buffer.");
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = indices.size() * sizeof(DWORD);
    sd.pSysMem = indices.data();
    V_HR(device->CreateBuffer(&bd, &sd, &skyIB),
         "Unable to create skybox index buffer.");

    waterWorld = XMMatrixIdentity();
}

void InitResources()
{
    HRESULT hr;

    D3D11_SAMPLER_DESC samDesc;
    samDesc.AddressU = samDesc.AddressV
                     = samDesc.AddressW
                     = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.BorderColor[0] = samDesc.BorderColor[1]
                           = samDesc.BorderColor[2]
                           = samDesc.BorderColor[3]
                           = 0;
    samDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
    samDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samDesc.MaxAnisotropy = AF;
    samDesc.MinLOD = 0;
    samDesc.MaxLOD = D3D11_FLOAT32_MAX;
    samDesc.MipLODBias = 0;
    
    V_HR(device->CreateSamplerState(&samDesc, &anisotropicSampler),
         "Unable to create sampler state.");

    V_HR(D3DX11CreateShaderResourceViewFromFile(
            device,
            CUBEMAP_FILENAME,
            NULL,
            NULL,
            &cubeMapSRV,
            NULL),
         "Unable to load skybox texture.");

    Wave waves[] = 
    {
        { XMFLOAT2(-0.70710677f,  0.70710677f), 0.2f, 0.002f },
        { XMFLOAT2(0.70710677f,  -0.70710677f), 0.8f, 0.001f },
        { XMFLOAT2(-1, 0), 0.5f, 0.004f },
    };

    D3D11_BUFFER_DESC bd;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.ByteWidth = sizeof(waves);
    bd.StructureByteStride = 0;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA sd;
    sd.SysMemPitch = 0;
    sd.SysMemSlicePitch = 0;
    sd.pSysMem = waves;

    ID3D11Buffer * waveBuffer = NULL;
    V_HR(device->CreateBuffer(&bd, &sd, &waveBuffer),
         "Unable to create buffer holding wave parameters.");
    
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Buffer.ElementOffset = 0;
    srvDesc.Buffer.ElementWidth = sizeof(Wave);
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = ARRAYSIZE(waves);

    hr = device->CreateShaderResourceView(waveBuffer, &srvDesc, &waveBufferSRV);
    SAFE_RELEASE(waveBuffer);
    V_HR(hr, "Unable to create shader resource view for wave buffer.");

    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    bd.ByteWidth = sizeof(VertexShaderConstantBuffer);
    V_HR(device->CreateBuffer(&bd, NULL, &vsCB),
         "Unable to create constant buffer for vertex shaders.");

    bd.ByteWidth = sizeof(PixelShaderConstantBuffer);
    V_HR(device->CreateBuffer(&bd, NULL, &psCB),
         "Unable to create constant buffer for pixel shaders.");
}

void Cleanup()
{
    if(deviceContext)
        deviceContext->ClearState();
    
    SAFE_RELEASE(vsCB);
    SAFE_RELEASE(psCB);
    SAFE_RELEASE(anisotropicSampler);
    SAFE_RELEASE(waveBufferSRV);
    SAFE_RELEASE(cubeMapSRV);
    SAFE_RELEASE(waterVB);
    SAFE_RELEASE(skyVB);
    SAFE_RELEASE(waterIB);
    SAFE_RELEASE(skyIB);
    SAFE_RELEASE(waterVS);
    SAFE_RELEASE(skyVS);
    SAFE_RELEASE(waterPS);
    SAFE_RELEASE(skyPS);
    SAFE_RELEASE(inputLayout);
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(backBufferRTV);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(deviceContext);
    SAFE_RELEASE(device);
}

void InitWindow()
{
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.hCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
    wc.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"Window";
    wc.hInstance = GetModuleHandle(NULL);
    wc.style = CS_DBLCLKS;
    
    if(!RegisterClassEx(&wc))
        throw Exception(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to register window class.");

    RECT r;
    GetWindowRect(GetDesktopWindow(), &r);
    width = (r.right - r.left)/3*2;
    height = (r.bottom - r.top)/3*2;

    if(!(window = CreateWindow(
                    L"Window",
                    L"Reef",
                    WS_OVERLAPPEDWINDOW,
                    (r.right - r.left)/6,
                    (r.bottom - r.top)/6,
                    width,
                    height,
                    NULL,
                    NULL,
                    wc.hInstance,
                    NULL)))
        throw Exception(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to create window.");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_DESTROY:
            {
                PostQuitMessage(0);
            }
            return 0;

        case WM_ENTERSIZEMOVE:
            {
                paused = TRUE;
            }
            return 0;

        case WM_SIZE:
            {
                width = LOWORD(lParam);
                height = HIWORD(lParam);
                if(!paused)
                    ResizeBuffers();
            }
            return 0;

        case WM_EXITSIZEMOVE:
            {
                paused = FALSE;
                ResizeBuffers();
            }
            return 0;

        case WM_PAINT:
            {
                Render();
            }
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

INT64 GetCounter()
{
    LARGE_INTEGER x;
    QueryPerformanceCounter(&x);
    return x.QuadPart;
}

INT64 GetFrequency()
{
    LARGE_INTEGER x;
    QueryPerformanceFrequency(&x);
    return x.QuadPart;
}
