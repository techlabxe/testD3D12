// Win32Project1.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Win32Project1.h"
#include <stdio.h>
#include <stdlib.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_3.h>
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
void                WaitForCommandQueue(ID3D12CommandQueue*);

HWND gHwnd;
HANDLE hFenceEvent;
using namespace Microsoft::WRL;
ComPtr<ID3D12Device> dxDevice;
ComPtr<ID3D12Fence>  queueFence;

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
	void* pVSBlob = 0;
	FILE* fp = nullptr;
	fopen_s(&fp, "VertexShader.cso", "rb");
	fseek(fp, 0, SEEK_END);
	int vssize = ftell(fp); rewind(fp);
	pVSBlob = malloc(vssize);
	fread(pVSBlob, 1, vssize, fp);
	fclose(fp);

	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT1));
	HRESULT hr;
	ComPtr<IDXGIFactory2> dxgiFactory;
	hr = CreateDXGIFactory2( 0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()) );

	hr = D3D12CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, D3D12_CREATE_DEVICE_DEBUG, D3D_FEATURE_LEVEL_11_1, D3D12_SDK_VERSION, IID_PPV_ARGS( dxDevice.GetAddressOf()));
	hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( commandAllocator.GetAddressOf()) );

	D3D12_COMMAND_QUEUE_DESC descCommandQueue;
	ZeroMemory(&descCommandQueue, sizeof(descCommandQueue) );
	descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	descCommandQueue.Priority = 0;
	descCommandQueue.Flags = D3D12_COMMAND_QUEUE_NONE;
	hr = dxDevice->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(commandQueue.GetAddressOf()));
	hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hr = dxDevice->CreateFence(0, D3D12_FENCE_MISC_NONE, IID_PPV_ARGS(queueFence.GetAddressOf()));

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = gHwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	IDXGISwapChain* pSwapChain = nullptr;
	hr = dxgiFactory->CreateSwapChain( commandQueue.Get(), &swapChainDesc, &pSwapChain);

	ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descHeap;
	ZeroMemory(&descHeap, sizeof(descHeap));
	descHeap.NumDescriptors = 1;
	descHeap.Type = D3D12_RTV_DESCRIPTOR_HEAP;
	descHeap.Flags = D3D12_DESCRIPTOR_HEAP_NONE;
	hr = dxDevice->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(pDescriptorHeap.GetAddressOf()));


	ComPtr<ID3D12RootSignature> pRootSignature;
	D3D12_ROOT_SIGNATURE rootSignature = D3D12_ROOT_SIGNATURE();
	ComPtr<ID3DBlob> pRootSigBlob;
	ComPtr<ID3DBlob> pErrorBlob;
	rootSignature.Flags = D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	hr = D3D12SerializeRootSignature(&rootSignature, D3D_ROOT_SIGNATURE_V1, pRootSigBlob.GetAddressOf(), pErrorBlob.GetAddressOf() );
	hr = dxDevice->CreateRootSignature(1, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS( pRootSignature.GetAddressOf()) );

	D3D12_INPUT_ELEMENT_DESC  descInputElement =
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 }
	;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso;
	ZeroMemory(&descPso, sizeof(descPso));
	descPso.NumRenderTargets = 1;
	descPso.RasterizerState.CullMode = D3D12_CULL_NONE;
	descPso.RasterizerState.FillMode = D3D12_FILL_SOLID;
	descPso.pRootSignature = pRootSignature.Get();
	descPso.SampleDesc.Count = 1;
	descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPso.VS.BytecodeLength = vssize;
	descPso.VS.pShaderBytecode = pVSBlob;
	descPso.InputLayout.pInputElementDescs = &descInputElement;
	descPso.InputLayout.NumElements = 1;
	descPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPso.RasterizerState = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
	ComPtr<ID3D12PipelineState> pPipelineState;
	hr = dxDevice->CreateGraphicsPipelineState(&descPso, IID_PPV_ARGS(pPipelineState.GetAddressOf()) );

	ComPtr<ID3D12GraphicsCommandList> pCommandList;
	hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pPipelineState.Get(), IID_PPV_ARGS(pCommandList.GetAddressOf()) );

	ID3D12Resource* pRenderTarget = nullptr;
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&pRenderTarget);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	dxDevice->CreateRenderTargetView(pRenderTarget, NULL, cpuDescriptor);

	float clearColor[4] = { 0,1.0f,1.0f,0 };
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0; viewport.TopLeftY = 0;
	viewport.Width = 640; 
	viewport.Height = 480;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	D3D12_RESOURCE_BARRIER_DESC descBarrier;
	ZeroMemory(&descBarrier, sizeof(descBarrier));
	pCommandList->Close();

	hr = commandAllocator->Reset();
	hr = pCommandList->Reset(commandAllocator.Get(), nullptr);

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
			static int count = 0;
			ZeroMemory(&descBarrier, sizeof(descBarrier));
			descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			descBarrier.Transition.pResource = pRenderTarget;
			descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			descBarrier.Transition.StateBefore = D3D12_RESOURCE_USAGE_PRESENT;
			descBarrier.Transition.StateAfter = D3D12_RESOURCE_USAGE_RENDER_TARGET;
			pCommandList->ResourceBarrier(1, &descBarrier);

			clearColor[0] = (float)(0.5f * sin(count*0.05f) + 0.5f);
			clearColor[1] = (float)(0.5f * sin(count*0.10f) + 0.5f);
			pCommandList->RSSetViewports(1, &viewport);
			pCommandList->ClearRenderTargetView(cpuDescriptor, clearColor, NULL, 0);

			descBarrier.Transition.StateBefore = D3D12_RESOURCE_USAGE_RENDER_TARGET;
			descBarrier.Transition.StateAfter = D3D12_RESOURCE_USAGE_PRESENT;
			pCommandList->ResourceBarrier(1, &descBarrier);

			pCommandList->Close();
			commandQueue->ExecuteCommandLists(1, CommandListCast(pCommandList.GetAddressOf()));
			pSwapChain->Present(1, 0);

			WaitForCommandQueue(commandQueue.Get());
			commandAllocator->Reset();
			pCommandList->Reset(commandAllocator.Get(), nullptr);
			count++;

		}
	}
	pRenderTarget->Release();
	pSwapChain->Release();
	CloseHandle(hFenceEvent);
	if (pVSBlob) { free(pVSBlob); }

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

void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
	queueFence->Signal(0);
	queueFence->SetEventOnCompletion(1, hFenceEvent);
	pCommandQueue->Signal(queueFence.Get(), 1);
	WaitForSingleObject(hFenceEvent, INFINITE);
}
