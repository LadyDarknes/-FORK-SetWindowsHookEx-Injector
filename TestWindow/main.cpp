#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace DirectX;

// Shaders (HLSL)
const char* g_ShaderSource = R"(
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProjection;
    float4 HighlightColor;
};

struct VS_INPUT_28
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct VS_INPUT_40
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct VS_INPUT_24
{
    float3 Pos : POSITION;
    float3 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT VS_Main28(VS_INPUT_28 input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProjection);
    output.Color = input.Color * HighlightColor;
    return output;
}

PS_INPUT VS_Main40(VS_INPUT_40 input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProjection);
    output.Color = input.Color * HighlightColor;
    return output;
}

PS_INPUT VS_Main24(VS_INPUT_24 input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProjection);
    output.Color = float4(input.Color, 1.0f) * HighlightColor;
    return output;
}

float4 PS_Main(PS_INPUT input) : SV_Target
{
    return input.Color;
}
)";

// Vertex structures for different Strides
// Stride 28: 3 floats (12 bytes) + 4 floats (16 bytes) = 28 bytes
struct VertexStride28
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

// Stride 40: 3 floats (12) + 3 floats (12) + 4 floats (16) = 40 bytes
struct VertexStride40
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
};

// Stride 24: 3 floats (12) + 3 floats (12) = 24 bytes (Unhooked)
struct VertexStride24
{
    XMFLOAT3 Pos;
    XMFLOAT3 Color;
};

// Constant buffer structure
struct ConstantBufferData
{
    XMMATRIX WorldViewProjection;
    XMFLOAT4 HighlightColor;
};

enum StrideMode
{
    STRIDE_28 = 0, // Hooked (Stride == 28)
    STRIDE_40 = 1, // Hooked (Stride == 40)
    STRIDE_24 = 2  // Not hooked (Stride == 24)
};

// Global variables
HWND                    g_hWnd = NULL;
ID3D11Device*           g_pd3dDevice = NULL;
ID3D11DeviceContext*    g_pImmediateContext = NULL;
IDXGISwapChain*         g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D*        g_pDepthStencil = NULL;
ID3D11DepthStencilView* g_pDepthStencilView = NULL;
ID3D11RasterizerState*  g_pRasterState = NULL;

ID3D11VertexShader*     g_pVS28 = NULL;
ID3D11VertexShader*     g_pVS40 = NULL;
ID3D11VertexShader*     g_pVS24 = NULL;
ID3D11PixelShader*      g_pPixelShader = NULL;

ID3D11InputLayout*      g_pLayout28 = NULL;
ID3D11InputLayout*      g_pLayout40 = NULL;
ID3D11InputLayout*      g_pLayout24 = NULL;

ID3D11Buffer*           g_pVB28 = NULL;
ID3D11Buffer*           g_pVB40 = NULL;
ID3D11Buffer*           g_pVB24 = NULL;
ID3D11Buffer*           g_pIndexBuffer = NULL;
ID3D11Buffer*           g_pConstantBuffer = NULL;

StrideMode              g_CurrentStride = STRIDE_28;
bool                    g_Rotate = true;
float                   g_RotationAngle = 0.0f;
UINT                    g_WindowWidth = 900;
UINT                    g_WindowHeight = 600;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
void Render();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    SetConsoleTitleW(L"DEBUG");

    std::cout << " PID: " << GetCurrentProcessId() << " | main id: " << GetCurrentThreadId() << "\n";
    std::cout << " Controls:\n";
    std::cout << "   [1] Stride = 28 bytes (hidden)\n";
    std::cout << "   [2] Stride = 40 bytes (hidden)\n";
    std::cout << "   [3] Stride = 24 bytes (visible)\n";
    std::cout << "   [SPACE] Pause/Play rotation\n";

    InitWindow(hInstance, nCmdShow);
    InitDevice();

    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if (wParam == '1')
        {
            g_CurrentStride = STRIDE_28;
            std::cout << "[STRIDE] Stride = 28 bytes (hidden)\n";
        }
        else if (wParam == '2')
        {
            g_CurrentStride = STRIDE_40;
            std::cout << "[STRIDE] Stride = 40 bytes (hidden)\n";
        }
        else if (wParam == '3')
        {
            g_CurrentStride = STRIDE_24;
            std::cout << "[STRIDE] Stride = 24 bytes (visible)\n";
        }
        else if (wParam == VK_SPACE)
        {
            g_Rotate = !g_Rotate;
            std::cout << "[Control] Rotation " << (g_Rotate ? "Resumed" : "Paused") << "\n";
        }
        break;

    case WM_SIZE:
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TestWindowClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wcex))
        return E_FAIL;

    RECT rc = { 0, 0, (LONG)g_WindowWidth, (LONG)g_WindowHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowW(
        L"TestWindowClass",
        L"TestWindow",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    SetWindowTextW(g_hWnd, L"TestWindow"); // for hook its static

    return S_OK;
}

HRESULT CompileShaderFromMemory(const char* szSource, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = NULL;
    HRESULT hr = D3DCompile(
        szSource,
        strlen(szSource),
        NULL,
        NULL,
        NULL,
        szEntryPoint,
        szShaderModel,
        dwShaderFlags,
        0,
        ppBlobOut,
        &pErrorBlob
    );

    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            std::cerr << "Shader compile error: " << (char*)pErrorBlob->GetBufferPointer() << "\n";
            pErrorBlob->Release();
        }
        return hr;
    }

    if (pErrorBlob) pErrorBlob->Release();
    return S_OK;
}

HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        hr = D3D11CreateDeviceAndSwapChain(
            NULL,
            driverTypes[driverTypeIndex],
            NULL,
            createDeviceFlags,
            featureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pImmediateContext
        );

        if (SUCCEEDED(hr))
            break;
    }

    if (FAILED(hr))
        return hr;

    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return hr;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    // z depth for nex time.
    D3D11_TEXTURE2D_DESC descDepth = { 0 };
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
    if (FAILED(hr)) return hr;

    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr)) return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // Viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Setup Rasterizer State
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    hr = g_pd3dDevice->CreateRasterizerState(&rasterDesc, &g_pRasterState);
    if (FAILED(hr)) return hr;
    g_pImmediateContext->RSSetState(g_pRasterState);

    // Compile Shaders
    ID3DBlob* pVSBlob28 = NULL;
    hr = CompileShaderFromMemory(g_ShaderSource, "VS_Main28", "vs_4_0", &pVSBlob28);
    if (FAILED(hr)) return hr;

    hr = g_pd3dDevice->CreateVertexShader(pVSBlob28->GetBufferPointer(), pVSBlob28->GetBufferSize(), NULL, &g_pVS28);
    if (FAILED(hr)) { pVSBlob28->Release(); return hr; }

    D3D11_INPUT_ELEMENT_DESC layout28[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(layout28, ARRAYSIZE(layout28), pVSBlob28->GetBufferPointer(), pVSBlob28->GetBufferSize(), &g_pLayout28);
    pVSBlob28->Release();
    if (FAILED(hr)) return hr;

    // Compile VS40 (Stride 40)
    ID3DBlob* pVSBlob40 = NULL;
    hr = CompileShaderFromMemory(g_ShaderSource, "VS_Main40", "vs_4_0", &pVSBlob40);
    if (FAILED(hr)) return hr;

    hr = g_pd3dDevice->CreateVertexShader(pVSBlob40->GetBufferPointer(), pVSBlob40->GetBufferSize(), NULL, &g_pVS40);
    if (FAILED(hr)) { pVSBlob40->Release(); return hr; }

    D3D11_INPUT_ELEMENT_DESC layout40[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(layout40, ARRAYSIZE(layout40), pVSBlob40->GetBufferPointer(), pVSBlob40->GetBufferSize(), &g_pLayout40);
    pVSBlob40->Release();
    if (FAILED(hr)) return hr;

    ID3DBlob* pVSBlob24 = NULL;
    hr = CompileShaderFromMemory(g_ShaderSource, "VS_Main24", "vs_4_0", &pVSBlob24);
    if (FAILED(hr)) return hr;

    hr = g_pd3dDevice->CreateVertexShader(pVSBlob24->GetBufferPointer(), pVSBlob24->GetBufferSize(), NULL, &g_pVS24);
    if (FAILED(hr)) { pVSBlob24->Release(); return hr; }

    D3D11_INPUT_ELEMENT_DESC layout24[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(layout24, ARRAYSIZE(layout24), pVSBlob24->GetBufferPointer(), pVSBlob24->GetBufferSize(), &g_pLayout24);
    pVSBlob24->Release();
    if (FAILED(hr)) return hr;

    // Compile Pixel Shader
    ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromMemory(g_ShaderSource, "PS_Main", "ps_4_0", &pPSBlob);
    if (FAILED(hr)) return hr;

    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return hr;

    // 3D cube 
    // Stride 28 (sizeof = 28 bytes)
    VertexStride28 vertices28[] =
    {
        { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 0.8f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.2f, 0.2f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices28);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA InitData = { 0 };
    InitData.pSysMem = vertices28;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVB28);
    if (FAILED(hr)) return hr;

    // Stride 40 Vertices (sizeof = 40 bytes)
    VertexStride40 vertices40[] =
    {
        { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0, 1, 0), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT3(0, 1, 0), XMFLOAT4(0.0f, 0.8f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT3(0, 1, 0), XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0, 1, 0), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0,-1, 0), XMFLOAT4(0.2f, 0.2f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT3(0,-1, 0), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT3(0,-1, 0), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0,-1, 0), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    };
    bd.ByteWidth = sizeof(vertices40);
    InitData.pSysMem = vertices40;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVB40);
    if (FAILED(hr)) return hr;

    // Stride 24 Vertices (sizeof = 24 bytes)
    VertexStride24 vertices24[] =
    {
        { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.8f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 0.2f, 0.2f) },
        { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.2f, 0.2f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
    };
    bd.ByteWidth = sizeof(vertices24);
    InitData.pSysMem = vertices24;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVB24);
    if (FAILED(hr)) return hr;

    WORD indices[] =
    {
        3,1,0, 2,1,3,
        0,5,4, 1,5,0,
        3,4,7, 0,4,3,
        1,6,5, 2,6,1,
        2,7,6, 3,7,2,
        6,4,5, 7,4,6,
    };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr)) return hr;

    // Constant Buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBufferData);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pConstantBuffer);
    if (FAILED(hr)) return hr;

    return S_OK;
}

void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pVB28) g_pVB28->Release();
    if (g_pVB40) g_pVB40->Release();
    if (g_pVB24) g_pVB24->Release();

    if (g_pLayout28) g_pLayout28->Release();
    if (g_pLayout40) g_pLayout40->Release();
    if (g_pLayout24) g_pLayout24->Release();

    if (g_pVS28) g_pVS28->Release();
    if (g_pVS40) g_pVS40->Release();
    if (g_pVS24) g_pVS24->Release();
    if (g_pPixelShader) g_pPixelShader->Release();

    if (g_pRasterState) g_pRasterState->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}

void Render()
{
    if (!g_pImmediateContext || !g_pRenderTargetView)
        return;

    float ClearColor[4] = { 0.08f, 0.10f, 0.16f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (g_Rotate)
    {
        g_RotationAngle += 0.015f;
        if (g_RotationAngle > XM_2PI)
            g_RotationAngle -= XM_2PI;
    }

    // Setup Matrices
    XMMATRIX mWorld = XMMatrixRotationY(g_RotationAngle) * XMMatrixRotationX(g_RotationAngle * 0.7f);
    XMVECTOR Eye = XMVectorSet(0.0f, 2.5f, -4.5f, 0.0f);
    XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX mView = XMMatrixLookAtLH(Eye, At, Up);
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)g_WindowWidth / (float)g_WindowHeight, 0.01f, 100.0f);

    ConstantBufferData cb;
    cb.WorldViewProjection = XMMatrixTranspose(mWorld * mView * mProjection);
    cb.HighlightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb, 0, 0);

    UINT stride = 0;
    UINT offset = 0;
    ID3D11Buffer* currentVB = NULL;
    ID3D11InputLayout* currentLayout = NULL;
    ID3D11VertexShader* currentVS = NULL;

    switch (g_CurrentStride)
    {
    case STRIDE_28:
        stride = sizeof(VertexStride28); // 28 bytes
        currentVB = g_pVB28;
        currentLayout = g_pLayout28;
        currentVS = g_pVS28;
        break;
    case STRIDE_40:
        stride = sizeof(VertexStride40); // 40 bytes
        currentVB = g_pVB40;
        currentLayout = g_pLayout40;
        currentVS = g_pVS40;
        break;
    case STRIDE_24:
        stride = sizeof(VertexStride24); // 24 bytes
        currentVB = g_pVB24;
        currentLayout = g_pLayout24;
        currentVS = g_pVS24;
        break;
    }

    g_pImmediateContext->IASetVertexBuffers(0, 1, &currentVB, &stride, &offset);
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->IASetInputLayout(currentLayout);

    g_pImmediateContext->VSSetShader(currentVS, NULL, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

    // this call will hooked
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    g_pSwapChain->Present(1, 0);
}
