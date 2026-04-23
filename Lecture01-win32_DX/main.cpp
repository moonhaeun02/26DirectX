/*
 * [강의 노트: DirectX 11 & Win32 GameLoop]
 * 1. WinMain: 프로그램의 입구
 * 2. WndProc: OS가 보낸 우편물(메시지)을 확인하는 곳
 * 3. GameLoop: 쉬지 않고 Update와 Render를 반복하는 엔진의 심장
 * 4. Release: 빌려온 GPU 메모리를 반드시 반납하는 습관 (메모리 누수 방지)
 */

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

 // --- [전역 객체 관리] ---
 // DirectX 객체들은 GPU 메모리를 직접 사용함. 
 // 사용 후 'Release()'를 호출하지 않으면 프로그램 종료 후에도 메모리가 점유될 수 있음(메모리 누수).
 // ID3D11으로 시작하는 모든 '인터페이스' 객체, g_pd3dDevice->Create... 함수를 통해 만들어진 모든 객체는 반드시 Release 해줘야 한다.
ID3D11Device* g_pd3dDevice = nullptr; // 리소스 생성자 (공장) - GPU 메모리에 자리를 만드는 일을 함. 버퍼 생성, 텍스처 생성, 쉐이더 생성 등. Create로 시작하는 모든 함수는 이 Device가 담당함.
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (현장 작업 반장, 일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 교체기 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지, 캔버스 (View)

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; }; // SV: System Value (OS가 화면 어디에 찍을지 결정할 때 쓰는 진짜 최종 좌표)

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11GameLoopClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: 움직이는 육망성 만들기",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hWnd, NULL, 100, 100, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);

    // 2. DX11 디바이스 및 스왑 체인 초기화 (스왑체인 설정)
    DXGI_SWAP_CHAIN_DESC sd = {}; // 스왑체인 설명서
	sd.BufferCount = 1; // 백 버퍼 개수가 1개 (더블 버퍼링)
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32비트 트루 컬러 (RBGA)
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용하겠다
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1; // 안티앨리어싱(계단 현상 제거) 기능 끔
    sd.Windowed = TRUE; // 전체 화면이 아닌 창 모드로 실행

    // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);
    
    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob; // HLSL 코드를 GPU 기계어로 번역, 컴파일 결과물을 임시로 저장하는 메모리 블록 (Binary Large Object)
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

   /*
    typedef struct D3D11_INPUT_ELEMENT_DESC {
        LPCSTR                     SemanticName;         // 1. 의미 (이름)
        UINT                       SemanticIndex;        // 2. 번호 (인덱스)
        DXGI_FORMAT                Format;               // 3. 데이터 형식 (크기/타입)
        UINT                       InputSlot;            // 4. 입력 슬롯 (통로)
        UINT                       AlignedByteOffset;    // 5. 오프셋 (시작 지점)
        D3D11_INPUT_CLASSIFICATION InputSlotClass;       // 6. 클래스 (데이터 성격)
        UINT                       InstanceDataStepRate; // 7. 인스턴싱 스텝
    } D3D11_INPUT_ELEMENT_DESC;
    */
    //정점의 데이터 형식을 정의 (IA 단계에 알려줌)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성 (삼각형 데이터)
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer); // &bd 설명서대로, &initData 데이터를 넣어서, &pVBuffer 완성된 GPU 버퍼의 주소를 여기에 담아줘

    // --- [5. 정석 게임 루프] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        // (1) 입력 단계: PeekMessage는 메시지가 없어도 바로 리턴함 (Non-blocking)
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // (2) 업데이트 단계: 여기서 캐릭터의 위치나 로직을 계산함
            // (과제: GetAsyncKeyState 등을 써서 posX, posY를 변경하셈)

            // (3) 출력 단계: 변한 데이터를 바탕으로 화면에 그림
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // 렌더링 파이프라인 상태 설정
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr); // OM(Output Merger): 최종 픽셀을 어디다 꽂을지 정함. 사용할 렌더 타겟 개수 1 (1개 이상인 경우는 색상, 방향 정보, 광원 거리 등을 각각 다른 도화지에서 그리는 경우)
            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            g_pImmediateContext->IASetInputLayout(pInputLayout);
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

            // Primitive Topology 설정: 삼각형 리스트로 연결하라!
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            // 최종 그리기
            // Draw(이번 호출에서 그릴 정점(Vertex)의 개수 , 정점 버퍼의 어느 인덱스부터 읽기 시작할 것인가)
            g_pImmediateContext->Draw(3, 0);

            // 화면 교체 (프론트 버퍼와 백 버퍼 스왑)
            /* Present(a,b)
            * 첫 번째 인자 (a): SyncInterval (V-Sync 설정)
            *      - 0:     즉시 출력. 모니터가 화면을 갱신하든 말든 상관없이 다 그렸으면 바로 화면을 교체함. (FPS가 무제한으로 올라가며 화면 찢어짐 발생 가능)
            *      - 1~4:   수직 동기화(V-Sync) 활성화.
            *               1은 모니터 주사율(60Hz 등)에 맞춰서 기다렸다가 화면 스왑.
            *               2/3/4 는 모니터가 화면을 2/3/4번 그릴 때마다 스왑
            * 두 번째 인자 (b): Flags (출력 옵션)
            *      - 0:                         일반적인 출력임.
            *        DXGI_PRESENT_TEST:         실제로 화면을 바꾸지는 않고, 장치가 준비되었는지 테스트만 할 때 씀.
            *        DXGI_PRESENT_DO_NOT_WAIT:  만약 GPU가 이전 프레임을 처리하느라 바쁘면 기다리지 않고 바로 에러를 뱉게 함.
            */
            g_pSwapChain->Present(0, 0);
        }
    }

    // --- [6. 자원 해제 (Release)] ---
    // 생성(Create)한 모든 객체는 프로그램 종료 전 반드시 Release 해야 함.
    // 생성의 역순으로 해제하는 것이 관례임.
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