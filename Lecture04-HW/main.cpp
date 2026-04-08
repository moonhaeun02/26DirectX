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
std::vector<GameObject*> g_GameWorld;

struct VideoConfig {
    int Width = 800;
    int Height = 600;
} g_Config;

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

    // 메모리 정리
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}