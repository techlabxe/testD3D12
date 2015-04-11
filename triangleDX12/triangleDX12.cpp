// triangleDX12.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "triangleDX12.h"

#define MAX_LOADSTRING 100
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

#pragma comment( lib, "d3d12.lib")
#pragma comment( lib, "dxgi.lib")

// Global Variables:
HINSTANCE hInst;								// current instance
HWND g_hWnd;
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

// DirectX12 Function
BOOL InitializeDX12();
BOOL ResourceSetupDX12();
void RenderDX12();
void TerminateDX12();
void WaitForCommandQueue();

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_TRIANGLEDX12, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}
	if (!InitializeDX12()) {
		return FALSE;
	}
	if (!ResourceSetupDX12()) {
		return FALSE;
	}

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			RenderDX12();
		}
	}

	TerminateDX12();
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
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRIANGLEDX12));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_TRIANGLEDX12);
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
   RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
   DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
   AdjustWindowRect(&rect, dwStyle, FALSE);
   hWnd = CreateWindow(szWindowClass, szTitle, 
	   dwStyle,
	   CW_USEDEFAULT, CW_USEDEFAULT,
	   rect.right-rect.left, rect.bottom-rect.top,
       NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }
   g_hWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

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
struct ShaderObject {
	void* binaryPtr;
	int   size;
};
ShaderObject gVertexShader;
ShaderObject gPixelShader;

using namespace Microsoft::WRL;
ComPtr<ID3D12Device> dxDevice;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12Fence> commandFence;
ComPtr<ID3D12DescriptorHeap> descriptorHeap;
ComPtr<ID3D12Resource> renderTargetPrimary;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;
HANDLE hFenceEvent;

ComPtr<ID3D12GraphicsCommandList> commandList;
D3D12_CPU_DESCRIPTOR_HANDLE descriptorRTV; // 従来のRenderTargetViewみたいなもの.

void AddResourceBarrier( 
    ID3D12GraphicsCommandList* command, 
    ID3D12Resource* pResource,
    D3D12_RESOURCE_USAGE before,
    D3D12_RESOURCE_USAGE after
    );

// DirectX12の初期化関数.
BOOL InitializeDX12() 
{
	HRESULT hr;
	D3D12_CREATE_DEVICE_FLAG dxflags = D3D12_CREATE_DEVICE_DEBUG;
#ifndef _DEBUG
	dxflags = 0;
#endif
	hr = D3D12CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		dxflags,
		D3D_FEATURE_LEVEL_11_1,
		D3D12_SDK_VERSION,
		IID_PPV_ARGS(dxDevice.GetAddressOf())
		);
	
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"D3D12CreateDevice() Failed.", L"ERROR", MB_OK);
		return FALSE;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS options;
	hr = dxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, (void*)&options, sizeof(options));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CheckFeatureSupport() Failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	// SwapChain作成時に必要になるのでコマンドキューを作成.
	hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf()) );
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateCommandAllocator() Failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	D3D12_COMMAND_QUEUE_DESC descCommandQueue;
	ZeroMemory(&descCommandQueue, sizeof(descCommandQueue));
	descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = dxDevice->CreateCommandQueue( &descCommandQueue, IID_PPV_ARGS(commandQueue.GetAddressOf()));

	// DXGIの初期化を行う.
	DXGI_SWAP_CHAIN_DESC descSwapChain;
	ZeroMemory(&descSwapChain, sizeof(descSwapChain));
	descSwapChain.BufferCount = 2;
	descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	descSwapChain.OutputWindow = g_hWnd;
	descSwapChain.SampleDesc.Count = 1;
	descSwapChain.Windowed = TRUE;
	descSwapChain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	ComPtr<IDXGIFactory3> dxgiFactory;
	hr = CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateDXGIFactory2() failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	hr = dxgiFactory->CreateSwapChain( commandQueue.Get(), &descSwapChain, (IDXGISwapChain**)(swapChain.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateSwapChain() failed.", L"ERROR", MB_OK);
		return FALSE;
	}

	// バックバッファのターゲット用に１つで作成.
	D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeap;
	ZeroMemory(&descDescriptorHeap, sizeof(descDescriptorHeap));
	descDescriptorHeap.NumDescriptors = 1;
	descDescriptorHeap.Type = D3D12_RTV_DESCRIPTOR_HEAP;
	descDescriptorHeap.Flags = D3D12_DESCRIPTOR_HEAP_NONE;
	hr = dxDevice->CreateDescriptorHeap(&descDescriptorHeap, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	// コマンドキューの同期用オブジェクトを作成.
	hFenceEvent = CreateEventA( nullptr, FALSE, FALSE, nullptr);
	hr = dxDevice->CreateFence(0, D3D12_FENCE_MISC_NONE, IID_PPV_ARGS(commandFence.GetAddressOf()));

	// バックバッファをプライマリのターゲットとして.
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(renderTargetPrimary.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"swapChain->GetBuffer() failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	descriptorRTV = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	dxDevice->CreateRenderTargetView(renderTargetPrimary.Get(), nullptr, descriptorRTV);

	return TRUE;
}

// DirectX12の終了関数.
void TerminateDX12() 
{
	if (gVertexShader.binaryPtr) { free(gVertexShader.binaryPtr); }
	if (gPixelShader.binaryPtr) { free(gPixelShader.binaryPtr); }
	CloseHandle(hFenceEvent);
}

// リソースの準備.
BOOL ResourceSetupDX12()
{
	HRESULT hr;
	// PipelineStateのための RootSignature の作成.
	D3D12_ROOT_SIGNATURE descRootSignature;
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> rootSigBlob, errorBlob;
	hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_V1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
	hr = dxDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateRootSignature() failed.", L"ERROR", MB_OK);
		return FALSE;
	}

	// コンパイル済みシェーダーの読み込み.
	// コンパイルそのものはVisualStudioのビルド時にやる.
	FILE* fpVS = nullptr;
	fopen_s( &fpVS, "VertexShader.cso", "rb");
	if (!fpVS) { return FALSE; }
	fseek(fpVS, 0, SEEK_END);
	gVertexShader.size = ftell(fpVS); rewind(fpVS);
	gVertexShader.binaryPtr = malloc(gVertexShader.size);
	fread(gVertexShader.binaryPtr, 1, gVertexShader.size, fpVS);
	fclose(fpVS); fpVS = nullptr;
	FILE* fpPS = nullptr;
	fopen_s( &fpPS, "PixelShader.cso", "rb");
	if (!fpPS) { return FALSE; }
	fseek(fpPS, 0, SEEK_END);
	gPixelShader.size = ftell(fpPS); rewind(fpPS);
	gPixelShader.binaryPtr = malloc(gPixelShader.size);
	fread(gPixelShader.binaryPtr, 1, gPixelShader.size, fpPS);
	fclose(fpPS); fpPS = nullptr;

	// 今回のための頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC descInputElements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,12, D3D12_INPUT_PER_VERTEX_DATA, 0 },
	};

	// PipelineStateオブジェクトの作成.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	descPipelineState.VS.pShaderBytecode = gVertexShader.binaryPtr;
	descPipelineState.VS.BytecodeLength = gVertexShader.size;
	descPipelineState.PS.pShaderBytecode = gPixelShader.binaryPtr;
	descPipelineState.PS.BytecodeLength = gPixelShader.size;
	descPipelineState.SampleDesc.Count = 1;
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.InputLayout.pInputElementDescs = descInputElements;
	descPipelineState.InputLayout.NumElements = _countof(descInputElements);
	descPipelineState.pRootSignature = rootSignature.Get();
	descPipelineState.NumRenderTargets = 1;
	descPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.RasterizerState = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.DepthStencilState = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState = CD3D12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.DepthStencilState.DepthEnable = FALSE;
	hr = dxDevice->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(pipelineState.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateGraphicsPipelineState() failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	// 今回用のコマンドリストをここで作成.
	hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(commandList.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateCommandList() failed.", L"ERROR", MB_OK);
		return FALSE;
	}
	commandList->Close(); // 後で投入する.
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);
	return TRUE;
}

void RenderDX12()
{
	// まずはターゲットのクリア処理
	float clearColor[4] = { 0.2f, 0.45f, 0.8f, 0.0f };
	D3D12_VIEWPORT viewPort = { 0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f };
	D3D12_RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AddResourceBarrier(commandList.Get(), renderTargetPrimary.Get(), D3D12_RESOURCE_USAGE_PRESENT, D3D12_RESOURCE_USAGE_RENDER_TARGET);
	commandList->RSSetViewports(1, &viewPort);
	commandList->RSSetScissorRects(1, &rect);
	commandList->ClearRenderTargetView(descriptorRTV, clearColor, nullptr, 0);
	commandList->SetRenderTargets(&descriptorRTV, FALSE, 1, nullptr);

	// ターゲットをPresent用のリソースとして状態変更.
    AddResourceBarrier(commandList.Get(), renderTargetPrimary.Get(), D3D12_RESOURCE_USAGE_RENDER_TARGET, D3D12_RESOURCE_USAGE_PRESENT );
	commandList->Close();

	// 上記で積んだコマンドを実行する.
	commandQueue->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));
	swapChain->Present(1, 0);
	WaitForCommandQueue();
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);
}

void WaitForCommandQueue()
{
	commandFence->Signal(0);
	commandFence->SetEventOnCompletion(1, hFenceEvent);
	commandQueue->Signal(commandFence.Get(), 1);
	WaitForSingleObject(hFenceEvent, INFINITE);
}

void AddResourceBarrier(
    ID3D12GraphicsCommandList* command,
    ID3D12Resource* pResource,
    D3D12_RESOURCE_USAGE before,
    D3D12_RESOURCE_USAGE after
    )
{
    D3D12_RESOURCE_BARRIER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.pResource = pResource;
    desc.Transition.StateBefore = before;
    desc.Transition.StateAfter = after;
    command->ResourceBarrier(1, &desc);
}
