#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <cmath> // 삼각함수 사용

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 수학 관련 상수
const float PI = 3.14159265f;
const float R = 0.5f;           // 별의 반지름
const float d2r = PI / 180.0f;  // 도 -> 라디안 변환 계수

// 1. 상수 버퍼 구조체 (16바이트 정렬 필수!)
struct ConstantData {
    XMFLOAT2 offset;   // 8바이트
    float padding[2];  // 8바이트 (합계 16바이트)
};

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// 전역 객체
// 1. 핵심 인프라
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// 2. 쉐이더 및 레이아웃
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

// 3. 자원 버퍼
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

// 위치 변수
XMFLOAT2 g_CurOffset = { 0.0f, 0.0f };

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // [1] 윈도우 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11StarMoveClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DX11StarMoveClass", L"과제: 상수 버퍼로 별 움직이기",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // [2] DX11 및 스왑체인 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // [3] 쉐이더 컴파일
    const char* shaderSource = R"(
        cbuffer MoveBuffer : register(b0) {
            float2 g_Offset;
        };
        struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
        PS_INPUT VS(VS_INPUT input) {
            PS_INPUT output;
            float3 finalPos = input.pos;
            finalPos.x += g_Offset.x;
            finalPos.y += g_Offset.y;
            output.pos = float4(finalPos, 1.0f);
            output.col = input.col;
            return output;
        }
        float4 PS(PS_INPUT input) : SV_Target { return input.col; }
    )";

    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release(); psBlob->Release();

    // [시험장 대응: 픽셀/넓이 기반 좌표 계산]
    float targetArea = 20000.0f; // 만약 넓이(Area)가 주어지면 (예: 20000px^2)
    // 육망성 면적 공식 A = (3 * sqrt(3) / 2) * R^2  => R = sqrt(2A / 3sqrt(3))
    float pixelR = sqrtf((2.0f * targetArea) / (3.0f * sqrtf(3.0f)));

    // 만약 반지름(Radius)이 직접 주어지면 그냥 pixelR = 100.0f; 식으로 쓰면 됨

    // [중요] NDC 좌표(-1 ~ 1)로 변환 (화면 해상도 800x600 기준)
    float rx = pixelR / (800.0f / 2.0f); // 가로 반지름 비율
    float ry = pixelR / (600.0f / 2.0f); // 세로 반지름 비율

    // [4] 정점 버퍼 생성 (삼각함수 적용 + 시계 방향 정렬)

    Vertex vertices[] = {
        // 첫 번째 삼각형 (x에는 rx, y에는 ry를 곱한다!)
        { rx * cosf(90 * d2r),  ry * sinf(90 * d2r),  0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        { rx * cosf(330 * d2r), ry * sinf(330 * d2r), 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { rx * cosf(210 * d2r), ry * sinf(210 * d2r), 0.0f, 0.0f, 0.0f, 1.0f, 1.0f },

        // 두 번째 삼각형
        { rx * cosf(270 * d2r), ry * sinf(270 * d2r), 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
        { rx * cosf(150 * d2r), ry * sinf(150 * d2r), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
        { rx * cosf(30 * d2r),  ry * sinf(30 * d2r),  0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    };
    /*
    Vertex vertices[] = {
        // 첫 번째 삼각형 (90도 -> 330도 -> 210도 : 시계 방향)
        { R * cosf(90 * d2r),  R * sinf(90 * d2r),  0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        { R * cosf(330 * d2r), R * sinf(330 * d2r), 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { R * cosf(210 * d2r), R * sinf(210 * d2r), 0.0f, 0.0f, 0.0f, 1.0f, 1.0f },

        // 두 번째 삼각형 (270도 -> 150도 -> 30도 : 시계 방향)
        { R * cosf(270 * d2r), R * sinf(270 * d2r), 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
        { R * cosf(150 * d2r), R * sinf(150 * d2r), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
        { R * cosf(30 * d2r),  R * sinf(30 * d2r),  0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    };

        Vertex vertices[] = {
        // 1. 똑바로 선 삼각형 (Red-Green-Blue)
        {  0.0f,    0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // 상단
        {  0.433f, -0.25f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // 우하단
        { -0.433f, -0.25f, 0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // 좌하단

        // 2. 뒤집힌 삼각형 (Yellow-Cyan-Magenta 느낌)
        {  0.0f,   -0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // 하단
        { -0.433f,  0.25f, 0.5f,  0.0f, 1.0f, 1.0f, 1.0f }, // 좌상단
        {  0.433f,  0.25f, 0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // 우상단
    };
    */

    D3D11_BUFFER_DESC vbd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA vData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&vbd, &vData, &g_pVertexBuffer);

    // [5] 상수 버퍼 생성
    D3D11_BUFFER_DESC cbd = { sizeof(ConstantData), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // [6] 게임 루프
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 업데이트
            if (GetAsyncKeyState(VK_LEFT))  g_CurOffset.x -= 0.005f;
            if (GetAsyncKeyState(VK_RIGHT)) g_CurOffset.x += 0.005f;
            if (GetAsyncKeyState(VK_UP))    g_CurOffset.y += 0.005f;
            if (GetAsyncKeyState(VK_DOWN))  g_CurOffset.y -= 0.005f;

            // 렌더링
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            ConstantData cb = { g_CurOffset };
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

            g_pImmediateContext->Draw(6, 0);
            g_pSwapChain->Present(1, 0);
        }
    }

    // [7] 해제
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVertexBuffer)   g_pVertexBuffer->Release();
    if (g_pInputLayout)    g_pInputLayout->Release();
    if (g_pVertexShader)   g_pVertexShader->Release();
    if (g_pPixelShader)    g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain)      g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice)      g_pd3dDevice->Release();

    return (int)msg.wParam;
}