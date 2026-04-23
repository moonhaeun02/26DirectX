/* * [포인트: 셰이더 컴파일의 4가지 길]
 * 1. CompileFromString: 코드 내부에 텍스트로 존재 (빠른 테스트용)
 * 2. CompileFromFile: 외부 파일(.hlsl)에서 읽기 (실무 표준)
 * 3. Separate Files: VS와 PS를 완전히 다른 파일로 관리 (대규모 프로젝트)
 * 4. Unified FX: 하나의 파일 안에 VS/PS 통합 관리
 */

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h> // [문법] D3DCompile 함수를 쓰기 위한 필수 헤더
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib") // [문법] 컴파일러 라이브러리 링크

 // --- [전역 객체] ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr; // GPU에서 실행될 정점 셰이더 객체
ID3D11PixelShader* g_pPixelShader = nullptr;   // GPU에서 실행될 픽셀 셰이더 객체
ID3D11InputLayout* g_pVertexLayout = nullptr;  // CPU 데이터와 VS 입력 시맨틱을 연결하는 도구
ID3D11Buffer* g_pVertexBuffer = nullptr;       // 정점 데이터를 담는 GPU 메모리

// --- [방식 1: 메모리 스트링 방식] ---
// 소스코드 내부에 셰이더 텍스트를 포함함. (배포 시 셰이더 코드가 노출됨)
const char* g_szShaderCode = R"(
struct VS_OUTPUT { float4 Pos : SV_POSITION; float4 Col : COLOR; };

VS_OUTPUT VS(float3 pos : POSITION, float4 col : COLOR) {
    VS_OUTPUT output;
    output.Pos = float4(pos, 1.0f);
    output.Col = col;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target { 
    return input.Col; 
}
)";

// --- [셰이더 컴파일 헬퍼 함수] ---
// pSrc: 코드(문자열) 혹은 파일명, isFile: 파일 모드 여부, szEntry: 시작 함수명, szTarget: 셰이더 버전
HRESULT CompileShader(const void* pSrc, bool isFile, LPCSTR szEntry, LPCSTR szTarget, ID3DBlob** ppBlob) {
    ID3DBlob* pErrorBlob = nullptr; // 에러 메시지를 담을 임시 바구니
    HRESULT hr;

    if (isFile) {
        // [기능] 하드디스크의 .hlsl 파일을 열어 컴파일함
        hr = D3DCompileFromFile((LPCWSTR)pSrc, nullptr, nullptr, szEntry, szTarget, 0, 0, ppBlob, &pErrorBlob);
    }
    else {
        // [기능] 메모리에 로드된 문자열(String)을 컴파일함
        hr = D3DCompile(pSrc, strlen((char*)pSrc), nullptr, nullptr, nullptr, szEntry, szTarget, 0, 0, ppBlob, &pErrorBlob);
    }

    // [문법] 컴파일 실패 시 에러 메시지 출력 로직
    if (FAILED(hr) && pErrorBlob) {
        printf("Shader Error: %s\n", (char*)pErrorBlob->GetBufferPointer());
        pErrorBlob->Release(); // 사용한 에러 바구니 해제
    }
    return hr;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. 창 생성 및 DX11 초기화 (생략/기본 구조)
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"DX11", nullptr };
    RegisterClassExW(&wc);
    HWND hWnd = CreateWindowW(L"DX11", L"Shader Compilation Lab", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, hInst, nullptr);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // ---------------------------------------------------------
    // 3. 셰이더 로드 핵심 (ID3DBlob: 컴파일된 결과물을 담는 범용 바구니)
    // ---------------------------------------------------------
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    // 방식 1: 스트링 컴파일 선택
    CompileShader(g_szShaderCode, false, "VS", "vs_4_0", &vsBlob);
    CompileShader(g_szShaderCode, false, "PS", "ps_4_0", &psBlob);

    // [기능] Blob에 담긴 기계어를 바탕으로 실제 GPU에서 돌아가는 셰이더 객체 생성
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    // 4. 레이아웃 (정점 셰이더의 입력 시맨틱과 일치해야 함)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    // [중요] InputLayout 생성 시 VS의 컴파일된 정보(vsBlob)가 필요함! (연결 확인용)
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pVertexLayout);

    // [문법/중요] 셰이더 객체를 만들고 난 뒤 임시 바구니(Blob)는 무조건 Release!
    vsBlob->Release();
    psBlob->Release();

    // 5. 정점 버퍼 및 렌더링 루프 (기존과 동일)
    float vertices[] = { 0.0f, 0.5f, 0.0f, 1,0,0,1, 0.5f, -0.5f, 0.0f, 0,1,0,1, -0.5f, -0.5f, 0.0f, 0,0,1,1 };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA init = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &init, &g_pVertexBuffer);

    ShowWindow(hWnd, nCmdShow);
    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            float color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, color);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
            UINT stride = 28, offset = 0; // [문법] stride: 점 하나당 크기 (3f+4f = 28byte)
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
            g_pImmediateContext->Draw(3, 0);
            g_pSwapChain->Present(0, 0);
        }
    }

    // 6. 자원 해제
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader)  g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}

/*
================================================================================
 [시험 적중 포인트: 셰이더 컴파일 & 관리]
================================================================================
 1. ID3DBlob의 정체:
    - 컴파일된 '기계어' 데이터를 담는 메모리 블록입니다.
    - CreateVertexShader/CreatePixelShader 함수의 재료로 사용됩니다.

 2. 왜 생성 후 바로 Release() 하는가?:
    - Create 함수를 통해 이미 GPU 전용 객체(ID3D11VertexShader 등)가 생성되었으므로,
      재료로 쓰인 임시 Blob 메모리는 더 이상 필요 없습니다. (메모리 낭비 방지)

 3. CompileShader의 인자 'szEntry'의 의미:
    - 셰이더 코드 내에서 어느 함수를 시작점(Entry Point)으로 삼을지 결정합니다.
    - 보통 VS는 "VS", PS는 "PS" 혹은 "main" 등으로 설정합니다.

 4. 시맨틱(Semantic) 불일치 시 발생하는 현상:
    - InputLayout의 SemanticName(예: "POSITION")과 HLSL의 시맨틱이 다르면
      CreateInputLayout 단계에서 실패하거나 화면에 아무것도 그려지지 않습니다.

 5. 스트링 방식 vs 파일 방식 장단점:
    - 스트링: 별도의 파일 관리가 필요 없어 간결하지만, 셰이더 코드를 수정할 때마다
      C++ 전체 프로젝트를 다시 빌드(Compile)해야 합니다.
    - 파일: 게임 실행 중에도 .hlsl 파일만 수정하면 즉시 적용할 수 있는
      '런타임 수정'이 가능하여 생산성이 높습니다.
================================================================================
*/