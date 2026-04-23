#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <cmath> 
#include <stdio.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// [시험 대비 수학 상수]
const float PI = 3.14159265f;
const float d2r = PI / 180.0f; // Degree to Radian

// [비디오 설정]
const float SCREEN_W = 800.0f;
const float SCREEN_H = 600.0f;

struct ConstantData {
    XMFLOAT2 offset;
    float padding[2];
};

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// 전역 객체
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

XMFLOAT2 g_CurOffset = { 0.0f, 0.0f };

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 생성 (기본 코드 생략)
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc; wcex.hInstance = hInstance; wcex.lpszClassName = L"DX11PentagramClass";
    RegisterClassExW(&wcex);
    HWND hWnd = CreateWindowW(L"DX11PentagramClass", L"Pentagram Area Lab", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, (int)SCREEN_W, (int)SCREEN_H, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. DX11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = (UINT)SCREEN_W; sd.BufferDesc.Height = (UINT)SCREEN_H;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // ---------------------------------------------------------
    // [시험 핵심: 넓이 기반 반지름 역산 및 NDC 변환]
    // ---------------------------------------------------------
    float targetArea = 30000.0f; // 교수님이 제시한 오망성의 넓이 (픽셀 단위)

    // 공식: A = 0.948 * R^2  => R = sqrt(A / 0.948)
    float pixelR = sqrtf(targetArea / 0.948f);

    // NDC 좌표계 변환 (픽셀 / (해상도/2))
    float rx_out = pixelR / (SCREEN_W / 2.0f);
    float ry_out = pixelR / (SCREEN_H / 2.0f);

    // 오망성의 오목한 부분 (안쪽 반지름) 비율: 황금비 약 0.382 적용
    float rx_in = rx_out * 0.382f;
    float ry_in = ry_out * 0.382f;

    // 3. 셰이더 컴파일 (생략 - 기존 무브 셰이더와 동일)
    const char* shaderSource = R"(
        cbuffer MoveBuffer : register(b0) { float2 g_Offset; };
        struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
        PS_INPUT VS(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos.x + g_Offset.x, input.pos.y + g_Offset.y, input.pos.z, 1.0f);
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

    // ---------------------------------------------------------
    // 4. 정점 버퍼 생성 (삼각형 10개 조립)
    // ---------------------------------------------------------
    Vertex vertices[30];
    for (int i = 0; i < 10; i++) {
        float a1 = 90.0f - (i * 36.0f);     // 현재 각도 (360/10 = 36도 간격)
        float a2 = 90.0f - ((i + 1) * 36.0f); // 다음 각도

        // 현재 각도와 다음 각도의 반지름 결정 (짝수=바깥, 홀수=안쪽)
        float cur_rx = (i % 2 == 0) ? rx_out : rx_in;
        float cur_ry = (i % 2 == 0) ? ry_out : ry_in;
        float next_rx = ((i + 1) % 2 == 0) ? rx_out : rx_in;
        float next_ry = ((i + 1) % 2 == 0) ? ry_out : ry_in;

        vertices[i * 3 + 0] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f }; // 중심
        vertices[i * 3 + 1] = { cur_rx * cosf(a1 * d2r), cur_ry * sinf(a1 * d2r), 0.0f, 1.0f, 0.8f, 0.0f, 1.0f };
        vertices[i * 3 + 2] = { next_rx * cosf(a2 * d2r), next_ry * sinf(a2 * d2r), 0.0f, 1.0f, 0.5f, 0.0f, 1.0f };
    }

    D3D11_BUFFER_DESC vbd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA vData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&vbd, &vData, &g_pVertexBuffer);

    D3D11_BUFFER_DESC cbd = { sizeof(ConstantData), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // 5. 루프 및 렌더링
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            if (GetAsyncKeyState(VK_LEFT))  g_CurOffset.x -= 0.005f;
            if (GetAsyncKeyState(VK_RIGHT)) g_CurOffset.x += 0.005f;
            if (GetAsyncKeyState(VK_UP))    g_CurOffset.y += 0.005f;
            if (GetAsyncKeyState(VK_DOWN))  g_CurOffset.y -= 0.005f;

            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            ConstantData cb = { g_CurOffset };
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            D3D11_VIEWPORT vp = { 0, 0, SCREEN_W, SCREEN_H, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
            g_pImmediateContext->Draw(30, 0); // 10개 조각 * 3개 정점
            g_pSwapChain->Present(1, 0);
        }
    }
    // 정리 (생략)
    return 0;
}

/*
================================================================================
 [오망성 시험 대비 최종 요약]
================================================================================
 1. 면적 공식: A = 0.948 * R^2 (약 A = R^2 로 계산해도 무방)
 2. 반지름 역산: R = sqrt(A / 0.95)
 3. 정점 로직: 중심점을 포함한 삼각형 10개를 36도 간격으로 배치
 4. 화면비 대응: x좌표는 (R/400), y좌표는 (R/300) 비중으로 각각 따로 곱해줌
================================================================================
*/