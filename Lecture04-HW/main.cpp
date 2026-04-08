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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return 0;
}