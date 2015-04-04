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

HWND gHwnd;
ID3D12Device* gpDevice = nullptr;
using namespace Microsoft::WRL;
ComPtr<ID3D12Device> dxDevice;
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

	ID3D12CommandQueue* pCommandQueue = nullptr;
	ComPtr<ID3D12CommandAllocator> commandAllocators[2];

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT1));
	HRESULT hr;
	IDXGIFactory2* dxgiFactory = nullptr;
	hr = CreateDXGIFactory2( 0, __uuidof(IDXGIFactory2), (void**)&dxgiFactory);

	hr = D3D12CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, D3D12_CREATE_DEVICE_DEBUG, D3D_FEATURE_LEVEL_11_1, D3D12_SDK_VERSION, __uuidof(ID3D12Device), (void**)&gpDevice);
	dxDevice.Attach(gpDevice);
	hr = gpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (LPVOID*)&commandAllocators[0]);
	hr = gpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (LPVOID*)&commandAllocators[1]);
	D3D12_COMMAND_QUEUE_DESC descCommandQueue;
	ZeroMemory(&descCommandQueue, sizeof(descCommandQueue) );
	descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	descCommandQueue.Priority = 0;
	descCommandQueue.Flags = D3D12_COMMAND_QUEUE_NONE;
	hr = gpDevice->CreateCommandQueue( &descCommandQueue, __uuidof(ID3D12CommandQueue), (void**)&pCommandQueue);

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = gHwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	IDXGIAdapter* adapter = nullptr;

	IDXGISwapChain* pSwapChain = nullptr;
	hr = dxgiFactory->CreateSwapChain( pCommandQueue, &swapChainDesc, &pSwapChain);

	D3D12_DESCRIPTOR_HEAP_DESC descHeap;
	ZeroMemory(&descHeap, sizeof(descHeap));
	descHeap.NumDescriptors = 1;
	descHeap.Type = D3D12_RTV_DESCRIPTOR_HEAP;
	ID3D12DescriptorHeap* pDescriptorHeap[2] = { nullptr };
	hr = gpDevice->CreateDescriptorHeap(&descHeap, __uuidof(ID3D12DescriptorHeap), (void**)&pDescriptorHeap[0]);
	descHeap.Type = D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP;
	descHeap.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
	hr = gpDevice->CreateDescriptorHeap(&descHeap, __uuidof(ID3D12DescriptorHeap), (void**)&pDescriptorHeap[1]);


	D3D12_ROOT_PARAMETER rootParameter[4];
	D3D12_DESCRIPTOR_RANGE DescriptorRange[2];
	ID3D12RootSignature* pRootSignature = nullptr;
	DescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_SAMPLER, 2, 0);
	DescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_SRV, 5, 2);
	rootParameter[0].InitAsDescriptorTable(1, &DescriptorRange[0]);
	rootParameter[1].InitAsDescriptorTable(1, &DescriptorRange[1]);
	rootParameter[2].InitAsConstantBufferView(7);
	rootParameter[3].InitAsConstants(1, 0);
	D3D12_ROOT_SIGNATURE rootSignature;
	rootSignature.Init(4, rootParameter);
	rootSignature.Flags = D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* pRootSigBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rootSignature, D3D_ROOT_SIGNATURE_V1, &pRootSigBlob, &pErrorBlob);
	hr = gpDevice->CreateRootSignature(1, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pRootSignature);

	D3D12_INPUT_ELEMENT_DESC  descInputElement =
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 }
	;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso;
	ZeroMemory(&descPso, sizeof(descPso));
	descPso.NumRenderTargets = 1;
	descPso.RasterizerState.CullMode = D3D12_CULL_NONE;
	descPso.RasterizerState.FillMode = D3D12_FILL_SOLID;
	descPso.pRootSignature = pRootSignature;
	descPso.SampleDesc.Count = 1;
	descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPso.VS.BytecodeLength = vssize;
	descPso.VS.pShaderBytecode = pVSBlob;
	descPso.InputLayout.pInputElementDescs = &descInputElement;
	descPso.InputLayout.NumElements = 1;
	descPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	ID3D12PipelineState* pPipelineState = nullptr;
	hr = gpDevice->CreateGraphicsPipelineState(&descPso, __uuidof(ID3D12PipelineState), (void**)&pPipelineState);

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	hr = gpDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get()/*pCommandListAllocator*/, pPipelineState, __uuidof(ID3D12GraphicsCommandList), (void**)&pCommandList);
	pCommandList->SetDescriptorHeaps(&pDescriptorHeap[1], 1);

	ID3D12Resource* pRenderTarget = nullptr;
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&pRenderTarget);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = pDescriptorHeap[0]->GetCPUDescriptorHandleForHeapStart();
	gpDevice->CreateRenderTargetView(pRenderTarget, NULL, cpuDescriptor);

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
			static int frame = 0, count = 0;
			ID3D12CommandAllocator* pAlloc = commandAllocators[frame].Get();
			pAlloc->Reset();
			pCommandList->Reset(pAlloc, nullptr);

			ZeroMemory(&descBarrier, sizeof(descBarrier));
			descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			descBarrier.Transition.pResource = pRenderTarget;
			descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			descBarrier.Transition.StateBefore = D3D12_RESOURCE_USAGE_PRESENT;
			descBarrier.Transition.StateAfter = D3D12_RESOURCE_USAGE_RENDER_TARGET;
			pCommandList->ResourceBarrier(1, &descBarrier);

			clearColor[0] = 0.5f * sin(count*0.05f) + 0.5f;
			clearColor[1] = 0.5f * sin(count*0.10f) + 0.5f;
			pCommandList->RSSetViewports(1, &viewport);
			pCommandList->ClearRenderTargetView(cpuDescriptor, clearColor, NULL, 0);

			descBarrier.Transition.StateBefore = D3D12_RESOURCE_USAGE_RENDER_TARGET;
			descBarrier.Transition.StateAfter = D3D12_RESOURCE_USAGE_PRESENT;
			pCommandList->ResourceBarrier(1, &descBarrier);

			pCommandList->Close();
			pCommandQueue->ExecuteCommandLists(1, CommandListCast(&pCommandList));
			pSwapChain->Present(1, 0);

			frame = 1 - frame;
			count++;

		}
	}
	pRootSignature->Release();
	pRootSigBlob->Release();
	pRenderTarget->Release();
	pPipelineState->Release();
	pCommandList->Release();
	pDescriptorHeap[0]->Release();
	pDescriptorHeap[1]->Release();
	pSwapChain->Release();
	pCommandQueue->Release();
	dxgiFactory->Release();
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

