/*================================================================================
 [핵심 개념: 변환 행렬 (SRT: Scale -> Rotation -> Translation)]
================================================================================
 1. 왜 행렬을 쓰는가?
    - 수천 개의 정점 좌표에 일일이 더하기, 곱하기, 삼각함수를 적용하면 너무 느림.
    - 변환 공식들을 '4x4 행렬' 하나로 압축해서 쉐이더(GPU)에 던져주면 한 방에 계산됨.
 2. SRT 곱셈 순서 (절대 규칙!)
    - 무조건 크기(S) -> 회전(R) -> 이동(T) 순서로 곱해야 함.
    - 순서가 바뀌면? (예: 이동 후 회전) 물체가 원점을 기준으로 크게 궤도를 돌며 공전해버림.
================================================================================*/

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h> // [문법 포인트] DirectX의 행렬/벡터 수학 라이브러리 추가!

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX; // [문법 포인트] XMMATRIX 등을 편하게 쓰기 위한 네임스페이스

struct VideoConfig {
    const int Width = 800;
    const int Height = 600;
} g_Config;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// [문법 포인트: 상수 버퍼 구조체]
// CPU에서 계산한 '변환 행렬'을 GPU로 택배 보내기 위한 상자(구조체)야.
// GPU는 16바이트 단위로 데이터를 읽기 때문에, 64바이트(16x4)인 행렬은 규격에 딱 맞음!
struct ConstantBuffer {
    XMMATRIX WorldMatrix;
};

// HLSL 쉐이더 소스
const char* shaderSource = R"(
// [쉐이더 포인트: cbuffer] CPU에서 보낸 상수 버퍼(Constant Buffer)를 받는 곳
cbuffer TransformBuffer : register(b0) {
    matrix WorldMatrix;
};

struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    
    // [핵심 연산: mul()] 
    // 원래의 좌표(input.pos)에 CPU에서 보내준 변환 행렬(WorldMatrix)을 곱함!
    // 이렇게 하면 이동, 회전, 크기 조절이 한 방에 적용된 최종 좌표가 나옴.
    output.pos = mul(float4(input.pos, 1.0f), WorldMatrix);
    
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col;
})";

// 전역 객체
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr; // 행렬을 담을 상수 버퍼

// [물체의 상태(Transform)를 저장할 변수들]
float g_PosX = 0.0f, g_PosY = 0.0f; // 이동 (기본 위치: 정중앙)
float g_RotZ = 0.0f;                // 회전 (기본 각도: 0도)
float g_Scale = 1.0f;               // 크기 (기본 크기: 1배)

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // (윈도우 생성 및 초기화 로직은 동일)
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11EngineClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"DX11EngineClass", L"SRT 변환(이동/회전/확대) 예제",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // DX11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
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

    // 쉐이더 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release();

    // 정점 버퍼 생성
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    // [중요: 상수 버퍼 생성]
    // CPU의 행렬을 GPU로 넘겨주기 위한 메모리 공간을 생성함
    D3D11_BUFFER_DESC cbd = { sizeof(ConstantBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // --- [게임 루프 단계] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [A. 입력 처리: 상태값 변경]
            // 이동 (방향키)
            if (GetAsyncKeyState(VK_LEFT))  g_PosX -= 0.005f;
            if (GetAsyncKeyState(VK_RIGHT)) g_PosX += 0.005f;
            if (GetAsyncKeyState(VK_UP))    g_PosY += 0.005f;
            if (GetAsyncKeyState(VK_DOWN))  g_PosY -= 0.005f;

            // 회전 (Q, E키) - 라디안 값이 누적됨
            if (GetAsyncKeyState('Q')) g_RotZ += 0.01f; // 반시계 회전
            if (GetAsyncKeyState('E')) g_RotZ -= 0.01f; // 시계 회전

            // 확대/축소 (W, S키)
            if (GetAsyncKeyState('W')) g_Scale += 0.005f; // 커짐
            if (GetAsyncKeyState('S')) g_Scale -= 0.005f; // 작아짐

            // [B. 로직 계산: SRT 행렬 만들기]
            // 1. 크기(Scale), 회전(Rotation), 이동(Translation) 행렬을 각각 생성
            XMMATRIX matScale = XMMatrixScaling(g_Scale, g_Scale, 1.0f);
            XMMATRIX matRot = XMMatrixRotationZ(g_RotZ); // Z축을 꼬챙이 삼아 회전시킴
            XMMATRIX matTrans = XMMatrixTranslation(g_PosX, g_PosY, 0.0f);

            // 2. 행렬 곱하기 (순서는 반드시 S * R * T)
            XMMATRIX matWorld = matScale * matRot * matTrans; // 자전
            // XMMATRIX matWorld = matScale * matTrans * matRot; // 공전

            // [문법 포인트: Transpose (전치 행렬)]
            // C++(행 기준)과 HLSL(열 기준)은 행렬을 읽는 방식이 반대임.
            // 그래서 GPU로 넘기기 전에 행과 열을 뒤집어줘야(Transpose) 쉐이더에서 제대로 계산됨!
            matWorld = XMMatrixTranspose(matWorld);

            // 3. 계산된 행렬을 상수 버퍼에 담아서 GPU로 복사 (택배 배달)
            ConstantBuffer cb;
            cb.WorldMatrix = matWorld;
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

            // [C. 렌더링 단계]
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            // [중요] 그리기 전에, 업데이트한 상수 버퍼를 버텍스 쉐이더의 0번 슬롯(b0)에 꽂아줌
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

            g_pImmediateContext->Draw(3, 0);
            g_pSwapChain->Present(1, 0); // V-Sync 켬
        }
    }

    // 정리
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}