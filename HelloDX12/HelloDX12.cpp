// Win32Project1.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Win32Project1.h"
#include <stdio.h>
#include <stdlib.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <math.h>
#pragma comment( lib, "d3d12.lib" )
#pragma comment(lib, "dxgi.lib")

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL InitializeD3D12( HWND hWnd );
void WaitForCommandQueue(ID3D12CommandQueue*);
void SetResourceBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
void OnRender();


HWND gHwnd;
HANDLE hFenceEvent;
using namespace Microsoft::WRL;
ComPtr<ID3D12Device> dxDevice;
ComPtr<ID3D12CommandQueue> cmdQueue;
ComPtr<ID3D12CommandAllocator> cmdAllocator;
ComPtr<ID3D12GraphicsCommandList> cmdList;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12Fence>  queueFence;
ComPtr<IDXGIFactory3> dxgiFactory;
ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
ComPtr<ID3D12Resource> renderTarget[2];
D3D12_CPU_DESCRIPTOR_HANDLE handleRTV[2];


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_WIN32PROJECT1, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

    InitializeD3D12(gHwnd);

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT1));

	// Main message loop:
	ZeroMemory(&msg, sizeof(msg));
	while( msg.message != WM_QUIT )
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
            OnRender();
		}
	}
	CloseHandle(hFenceEvent);
	return (int) msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIN32PROJECT1));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   RECT rc = { 0, 0, 640, 480 };
   AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   gHwnd = hWnd;
   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL InitializeD3D12(HWND hWnd)
{
    UINT flagsDXGI = 0;
    ID3D12Debug* debug = nullptr;
    HRESULT hr;
#if _DEBUG
    D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (debug) {
        debug->EnableDebugLayer();
        debug->Release();
    }
    flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateDXGIFactory2\n");
        return FALSE;
    }
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiFactory->EnumAdapters(0, adapter.GetAddressOf());
    if (FAILED(hr)) {
        return FALSE;
    }
    // デバイス生成.
    // D3D12は 最低でも D3D_FEATURE_LEVEL_11_0 を要求するようだ.
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(dxDevice.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed D3D12CreateDevice\n");
        return FALSE;
    }
    // コマンドアロケータを生成.
    hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateCommandAllocator\n");
        return FALSE;
    }
    // コマンドキューを生成.
    D3D12_COMMAND_QUEUE_DESC descCommandQueue;
    ZeroMemory(&descCommandQueue, sizeof(descCommandQueue));
    descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    descCommandQueue.Priority = 0;
    descCommandQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = dxDevice->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(cmdQueue.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateCommandQueue\n");
        return FALSE;
    }
    // コマンドキュー用のフェンスを準備.
    hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hr = dxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(queueFence.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateFence\n");
        return FALSE;
    }

    // スワップチェインを生成.
    DXGI_SWAP_CHAIN_DESC descSwapChain;
    ZeroMemory(&descSwapChain, sizeof(descSwapChain));
    descSwapChain.BufferCount = 2;
    descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descSwapChain.OutputWindow = gHwnd;
    descSwapChain.SampleDesc.Count = 1;
    descSwapChain.Windowed = TRUE;
    descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    descSwapChain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    hr = dxgiFactory->CreateSwapChain(cmdQueue.Get(), &descSwapChain, (IDXGISwapChain**)swapChain.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateSwapChain\n");
        return FALSE;
    }
    // コマンドリストの作成.
    hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateCommandList\n");
        return FALSE;
    }

    // ディスクリプタヒープ(RenderTarget用)の作成.
    D3D12_DESCRIPTOR_HEAP_DESC descHeap;
    ZeroMemory(&descHeap, sizeof(descHeap));
    descHeap.NumDescriptors = 2;
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = dxDevice->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(descriptorHeapRTV.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugString(L"Failed CreateDescriptorHeap\n");
        return FALSE;
    }

    // レンダーターゲット(プライマリ用)の作成.
    UINT strideHandleBytes = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0;i < descSwapChain.BufferCount;++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(renderTarget[i].GetAddressOf()));
        if (FAILED(hr)) {
            OutputDebugString(L"Failed swapChain->GetBuffer\n");
            return FALSE;
        }
        handleRTV[i] = descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart();
        handleRTV[i].ptr += i*strideHandleBytes;
        dxDevice->CreateRenderTargetView(renderTarget[i].Get(), nullptr, handleRTV[i]);
    }
    return TRUE;
}

void SetResourceBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) 
{
    D3D12_RESOURCE_BARRIER descBarrier;
    ZeroMemory(&descBarrier, sizeof(descBarrier));
    descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    descBarrier.Transition.pResource = resource;
    descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    descBarrier.Transition.StateBefore = before;
    descBarrier.Transition.StateAfter = after;
    commandList->ResourceBarrier(1, &descBarrier);
}

void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
	queueFence->Signal(0);
	queueFence->SetEventOnCompletion(1, hFenceEvent);
	pCommandQueue->Signal(queueFence.Get(), 1);
	WaitForSingleObject(hFenceEvent, INFINITE);
}

void OnRender() {
    static int count = 0;
    float clearColor[4] = { 0,1.0f,1.0f,0 };
    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = 0; viewport.TopLeftY = 0;
    viewport.Width = 640;
    viewport.Height = 480;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;

    int targetIndex = swapChain->GetCurrentBackBufferIndex();

    SetResourceBarrier(
        cmdList.Get(),
        renderTarget[targetIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    // レンダーターゲットのクリア処理.
    clearColor[0] = (float)(0.5f * sin(count*0.05f) + 0.5f);
    clearColor[1] = (float)(0.5f * sin(count*0.10f) + 0.5f);
    cmdList->RSSetViewports(1, &viewport);
    cmdList->ClearRenderTargetView(handleRTV[targetIndex], clearColor, 0, nullptr );

    // Presentする前の準備.
    SetResourceBarrier(
        cmdList.Get(),
        renderTarget[targetIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    cmdList->Close();

    // 積んだコマンドの実行.
    ID3D12CommandList* pCommandList = cmdList.Get();
    cmdQueue->ExecuteCommandLists(1, &pCommandList);
    swapChain->Present(1, 0);

    WaitForCommandQueue(cmdQueue.Get());
    cmdAllocator->Reset();
    cmdList->Reset(cmdAllocator.Get(), nullptr);
    count++;
}