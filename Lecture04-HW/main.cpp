#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 전역 변수
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

struct VideoConfig {
    int Width = 800;
    int Height = 600;
} g_Config;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// 셰이더
const char* g_ShaderSource = R"(
    struct VS_IN { 
        float3 pos : POSITION; 
        float4 col : COLOR; 
    };
    
    struct PS_IN { 
        float4 pos : SV_POSITION; 
        float4 col : COLOR; 
    };
    
    PS_IN VS(VS_IN input) { 
        PS_IN output; 
        output.pos = float4(input.pos, 1.0f); 
        output.col = input.col; 
        return output; 
    }
    
    float4 PS(PS_IN input) : SV_Target { 
        return input.col; 
    }
)";

class Component {
public:
    class GameObject* pOwner = nullptr;
    bool isStarted = false;

    virtual void Start() = 0;
    virtual void Input() {}
    virtual void Update(float dt) = 0;
    virtual void Render() {}
    virtual ~Component() {}
};

class GameObject {
public:
    std::string name;
    std::vector<Component*> components;
    float x = 0.0f;
    float y = 0.0f;

    GameObject(std::string n, float startX = 0.0f, float startY = 0.0f)
        : name(n), x(startX), y(startY) {
    }

    ~GameObject() {
        for (auto comp : components) {
            delete comp;
        }
    }

    void AddComponent(Component* pComp) {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

std::vector<GameObject*> g_GameWorld;

// 메세지 처리 함수
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 윈도우 클래스
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11Engine";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    
    // 윈도우 창 생성
    HWND hWnd = CreateWindowW(L"DX11Engine", L"Lecture04 HW - 2 Players", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);
    
    // DirectX 11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // 장치, 스왑체인 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더타겟뷰 설정
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 셰이더 컴파일
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(g_ShaderSource, strlen(g_ShaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(g_ShaderSource, strlen(g_ShaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    // 셰이더 생성
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    // InputLayout 생성
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release();
    psBlob->Release();

    // 게임 루프
    MSG msg = { 0 };
    auto prevTime = std::chrono::high_resolution_clock::now();
    float deltaTime = 0.0f;

    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // DeltaTime 계산
            auto currentTime = std::chrono::high_resolution_clock::now();
            deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();
            prevTime = currentTime;

            // 1. input
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PostQuitMessage(0);

            for (auto obj : g_GameWorld) {
                for (auto comp : obj->components) {
                    comp->Input();
                }
            }

            // 2. update
            for (auto obj : g_GameWorld) {
                for (auto comp : obj->components) {
                    if (!comp->isStarted) {
                        comp->Start();
                        comp->isStarted = true;
                    }
                    comp->Update(deltaTime);
                }
            }

            // 3. render
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            for (auto obj : g_GameWorld) {
                for (auto comp : obj->components) {
                    comp->Render();
                }
            }

            g_pSwapChain->Present(1, 0);
        }
    }

    for (auto obj : g_GameWorld) {
        delete obj;
    }

    // 메모리 정리
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}