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
void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue);

struct MyVertex {
    float x, y, z;
    float r, g, b, a;
};
MyVertex triangleVertices[] = {
    { 0.0f, 0.5f, 0.0f,  1.0f,0.0f,0.0f,1.0f },
    { 0.45f,-0.5f, 0.0f,  0.0f,1.0f,0.0f,1.0f },
    { -.45f,-0.5f, 0.0f,  0.0f,0.0f,1.0f,1.0f },
};

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
ComPtr<IDXGIFactory3> dxgiFactory;
ComPtr<ID3D12CommandAllocator> cmdAllocator;
ComPtr<ID3D12CommandQueue> cmdQueue;
ComPtr<ID3D12Fence> queueFence;
ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
ComPtr<ID3D12Resource> renderTarget[2];
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;
HANDLE hFenceEvent;

ComPtr<ID3D12GraphicsCommandList> cmdList;
D3D12_CPU_DESCRIPTOR_HANDLE handleRTV[2];
ComPtr<ID3D12Resource> vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexView;

void SetResourceBarrier(
    ID3D12GraphicsCommandList* commandList, 
    ID3D12Resource* resource, 
    D3D12_RESOURCE_STATES before, 
    D3D12_RESOURCE_STATES after);

// DirectX12の初期化関数.
BOOL InitializeDX12() 
{
	HRESULT hr;
    UINT flagsDXGI = 0;
    ID3D12Debug* debug = nullptr;

#ifdef _DEBUG
    D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (debug) {
        debug->EnableDebugLayer();
        debug->Release();
        debug = nullptr;
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
        MessageBoxW(NULL, L"D3D12CreateDevice() Failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS options;
    hr = dxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, (void*)&options, sizeof(options));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CheckFeatureSupport() Failed.", L"ERROR", MB_OK);
        return FALSE;
    }

    // コマンドアロケータを生成.
    hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateCommandAllocator() Failed.", L"ERROR", MB_OK);
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
    descSwapChain.OutputWindow = g_hWnd;
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

	// バックバッファのターゲット用に１つで作成.
    // ディスクリプタヒープ(RenderTarget用)の作成.
    D3D12_DESCRIPTOR_HEAP_DESC descHeap;
    ZeroMemory(&descHeap, sizeof(descHeap));
    descHeap.NumDescriptors = 2;
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = dxDevice->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(descriptorHeapRTV.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
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
	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
    ZeroMemory(&descRootSignature, sizeof(descRootSignature));
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> rootSigBlob, errorBlob;
	hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    descPipelineState.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    descPipelineState.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    descPipelineState.RasterizerState.DepthClipEnable = TRUE;
    descPipelineState.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    descPipelineState.BlendState.RenderTarget[0].BlendEnable = FALSE;
    descPipelineState.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    descPipelineState.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    descPipelineState.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    descPipelineState.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	descPipelineState.DepthStencilState.DepthEnable = FALSE;
	hr = dxDevice->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(pipelineState.GetAddressOf()));
	if (FAILED(hr)) {
		MessageBoxW(NULL, L"CreateGraphicsPipelineState() failed.", L"ERROR", MB_OK);
		return FALSE;
	}

    // 頂点データの準備.
    D3D12_HEAP_PROPERTIES heapProps;
    D3D12_RESOURCE_DESC   descResourceVB;
    ZeroMemory(&heapProps, sizeof(heapProps));
    ZeroMemory(&descResourceVB, sizeof(descResourceVB));
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;
    descResourceVB.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    descResourceVB.Width = sizeof(triangleVertices);
    descResourceVB.Height = 1;
    descResourceVB.DepthOrArraySize = 1;
    descResourceVB.MipLevels = 1;
    descResourceVB.Format = DXGI_FORMAT_UNKNOWN;
    descResourceVB.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    descResourceVB.SampleDesc.Count = 1;

    hr = dxDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &descResourceVB,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(vertexBuffer.GetAddressOf())
        );
    if (FAILED(hr)){
        MessageBoxW(NULL, L"CreateCommittedResource() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    // 頂点データの書き込み
    void* mapped = nullptr;
    hr = vertexBuffer->Map(0, nullptr, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped, triangleVertices, sizeof(triangleVertices));
        vertexBuffer->Unmap(0, nullptr);
    }
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"vertexBuffer->Map() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexView.StrideInBytes = sizeof(MyVertex);
    vertexView.SizeInBytes = sizeof(triangleVertices);

    return TRUE;
}

void RenderDX12()
{
    int targetIndex = swapChain->GetCurrentBackBufferIndex();
	// まずはターゲットのクリア処理
	float clearColor[4] = { 0.2f, 0.45f, 0.8f, 0.0f };
	D3D12_VIEWPORT viewPort = { 0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f };
	D3D12_RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    SetResourceBarrier(cmdList.Get(), renderTarget[targetIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->SetGraphicsRootSignature(rootSignature.Get());
    cmdList->SetPipelineState(pipelineState.Get());
	cmdList->RSSetViewports(1, &viewPort);
	cmdList->RSSetScissorRects(1, &rect);
	cmdList->ClearRenderTargetView(handleRTV[targetIndex], clearColor, 0, nullptr );
	cmdList->OMSetRenderTargets(1, &handleRTV[targetIndex], TRUE, nullptr);

    // 頂点データをセット.
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vertexView );
    cmdList->DrawInstanced(3, 1, 0, 0);

	// ターゲットをPresent用のリソースとして状態変更.
    SetResourceBarrier(cmdList.Get(), renderTarget[targetIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
	cmdList->Close();

	// 上記で積んだコマンドを実行する.
    ID3D12CommandList* pCommandList = cmdList.Get();
	cmdQueue->ExecuteCommandLists(1, &pCommandList );
	swapChain->Present(1, 0);
	WaitForCommandQueue(cmdQueue.Get());
	cmdAllocator->Reset();
	cmdList->Reset(cmdAllocator.Get(), nullptr);
}

void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
    queueFence->Signal(0);
    queueFence->SetEventOnCompletion(1, hFenceEvent);
    pCommandQueue->Signal(queueFence.Get(), 1);
    WaitForSingleObject(hFenceEvent, INFINITE);
}

// リソース状態変更のための関数.
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