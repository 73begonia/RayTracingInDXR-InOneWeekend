#include "D3D12Screen.h"

namespace DescriptorID
{
	enum
	{
		tracerOutTextureSRV = 0,
		maxDescriptors
	};
}

D3D12Screen::D3D12Screen(HWND hwnd, uint width, uint height) :
	mTargetWindow(hwnd), mScreenW(width), mScreenH(height)
{
	mTracerOutW = mScreenW;
	mTracerOutH = mScreenH;
	mRenderTargetArr.resize(mBackBufferCount);

	initD3D12();

	createSrvUavHeap(DescriptorID::maxDescriptors);

	onSizeChanged(mScreenW, mScreenH);

	declareRootSignature();

	createPipeline();

	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());
}

D3D12Screen::~D3D12Screen()
{
}

void D3D12Screen::initD3D12()
{
	mDevice_v0 = createDX12Device(getRTXAdapter().Get());

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(mDevice_v0->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCmdQueue_v0)));

	ThrowIfFailed(mDevice_v0->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator_v0)));

	ThrowIfFailed(mDevice_v0->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator_v0.Get(), nullptr, IID_PPV_ARGS(&mCmdList_v0)));
	ThrowIfFailed(mCmdList_v0->Close());

	mFence_v0.create(mDevice_v0.Get());

	createSwapChain();
}

void D3D12Screen::createSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = mBackBufferCount;
	swapChainDesc.Width = mScreenW;
	swapChainDesc.Height = mScreenH;
	swapChainDesc.Format = mScreenOutFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;
	swapChainDesc.SampleDesc.Count = 1;

	ThrowIfFailed(getFactory()->CreateSwapChainForHwnd(mCmdQueue_v0.Get(), mTargetWindow, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(mSwapChain_v3.ReleaseAndGetAddressOf())));
	mCurrentBufferIdx = mSwapChain_v3->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	{
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = mBackBufferCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	mDevice_v0->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeap));
	mRtvDescriptorSize = mDevice_v0->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void D3D12Screen::createSrvUavHeap(uint maxDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	{
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = maxDescriptors;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	mDevice_v0->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvUavHeap));
}

void D3D12Screen::initializeResource()
{
	if (mTextureUploader != nullptr)
		mTextureUploader.Reset();

	mTextureUploader = createCommittedBuffer(_textureDataSize(mTracerOutFormat, 1920, 1080));

	if (mTracerOutTexture != nullptr)
		mTracerOutTexture.Reset();

	mTracerOutTexture = createDefaultTexture(mTracerOutFormat, mTracerOutW, mTracerOutH, D3D12_RESOURCE_STATE_COMMON);

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleTexture = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	srvHandleTexture.ptr += (uint)DescriptorID::tracerOutTextureSRV * mRtvDescriptorSize;
	mDevice_v0->CreateShaderResourceView(mTracerOutTexture.Get(), nullptr, srvHandleTexture);
}

void D3D12Screen::onSizeChanged(uint width, uint height)
{
	mTracerOutW = mScreenW = width;
	mTracerOutH = mScreenH = height;

	assert(mDevice_v0);
	assert(mSwapChain_v3);
	assert(mCmdAllocator_v0);

	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());

	ThrowIfFailed(mCmdList_v0->Reset(mCmdAllocator_v0.Get(), nullptr));

	for (uint i = 0; i < mBackBufferCount; ++i)
	{
		if (mRenderTargetArr[i] != nullptr)
		{
			mRenderTargetArr[i].Reset();
		}
	}

	ThrowIfFailed(mSwapChain_v3->ResizeBuffers(
		mBackBufferCount,
		mScreenW,
		mScreenH,
		mScreenOutFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrentBufferIdx = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (uint i = 0; i < mBackBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain_v3->GetBuffer(i, IID_PPV_ARGS(&mRenderTargetArr[i])));
		mDevice_v0->CreateRenderTargetView(mRenderTargetArr[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.ptr += mRtvDescriptorSize;
	}

	ThrowIfFailed(mCmdList_v0->Close());
	ID3D12CommandList* cmdsLists[] = { mCmdList_v0.Get() };
	mCmdQueue_v0->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());

	initializeResource();

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mScreenW);
	mScreenViewport.Height = static_cast<float>(mScreenH);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (long)mScreenW, (long)mScreenH };
}

void D3D12Screen::declareRootSignature()
{
	assert(mSrvUavHeap != nullptr);

	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = 0;
	range.RegisterSpace = 0;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER param = {};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param.DescriptorTable.NumDescriptorRanges = 1;
	param.DescriptorTable.pDescriptorRanges = &range;

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;

	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MinLOD = 0;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;

	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	vector<D3D12_STATIC_SAMPLER_DESC> samplers;
	samplers.resize(1);
	samplers[0] = sampler;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 1;
	rootSigDesc.pParameters = &param;
	rootSigDesc.NumStaticSamplers = (uint)samplers.size();
	rootSigDesc.pStaticSamplers = samplers.size() > 0 ? samplers.data() : nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> blob = nullptr;
	ComPtr<ID3DBlob> error = nullptr;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (error)
	{
		throw Error((char*)error->GetBufferPointer());
	}
	ThrowIfFailed(mDevice_v0->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));
}

void D3D12Screen::createPipeline()
{
	D3D12_INPUT_LAYOUT_DESC inputLayout = { nullptr, 0 };
	dxShader vertexShader(L"D3D12Screen.hlsl", "VSMain", "vs_5_0");
	dxShader pixelShader(L"D3D12Screen.hlsl", "PSMain", "ps_5_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};

	pipelineDesc.InputLayout = inputLayout;
	pipelineDesc.pRootSignature = mRootSig.Get();
	pipelineDesc.VS = vertexShader.getCode();
	pipelineDesc.PS = pixelShader.getCode();
	pipelineDesc.SampleMask = UINT_MAX;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = mScreenOutFormat;

	D3D12_RASTERIZER_DESC rasDesc = {};
	{
		rasDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasDesc.DepthClipEnable = TRUE;
	}
	pipelineDesc.RasterizerState = rasDesc;

	D3D12_BLEND_DESC blendDesc = {};
	{
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (uint i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	}
	pipelineDesc.BlendState = blendDesc;

	ThrowIfFailed(mDevice_v0->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mPipeline)));
}

void D3D12Screen::fillCommandLists()
{
	mCmdList_v0->SetPipelineState(mPipeline.Get());

	mCmdList_v0->SetDescriptorHeaps(1, mSrvUavHeap.GetAddressOf());
	mCmdList_v0->SetGraphicsRootSignature(mRootSig.Get());

	D3D12_GPU_DESCRIPTOR_HANDLE tracerOutTextureHandle = mSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	tracerOutTextureHandle.ptr += (uint)DescriptorID::tracerOutTextureSRV * mRtvDescriptorSize;
	mCmdList_v0->SetGraphicsRootDescriptorTable(DescriptorID::tracerOutTextureSRV, tracerOutTextureHandle);

	mCurrentBufferIdx = mSwapChain_v3->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += mCurrentBufferIdx * mRtvDescriptorSize;
	mCmdList_v0->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCmdList_v0->RSSetViewports(1, &mScreenViewport);
	mCmdList_v0->RSSetScissorRects(1, &mScissorRect);

	mCmdList_v0->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargetArr[mCurrentBufferIdx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList_v0->ResourceBarrier(1, &barrier);

	mCmdList_v0->DrawInstanced(4, 1, 0, 0);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	mCmdList_v0->ResourceBarrier(1, &barrier);
}

void D3D12Screen::display(const TracedResult& trResult)
{
	ThrowIfFailed(mCmdAllocator_v0->Reset());
	ThrowIfFailed(mCmdList_v0->Reset(mCmdAllocator_v0.Get(), nullptr));

	uint64 trResultPitch = trResult.width * trResult.pixelSize;
	uint rowDataSize = _bpp(mTracerOutFormat) * mTracerOutW;
	assert(trResultPitch == rowDataSize);

	uint rowPitch = _align(rowDataSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	uint8* pBufs = nullptr;
	mTextureUploader->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs));
	memcpyPitch(pBufs, rowPitch, trResult.data, trResultPitch, trResultPitch, trResult.height);

	D3D12_TEXTURE_COPY_LOCATION dstDesc;
	dstDesc.pResource = mTracerOutTexture.Get();
	dstDesc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstDesc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcDesc;
	srcDesc.pResource = mTextureUploader.Get();
	srcDesc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcDesc.PlacedFootprint.Footprint.Depth = 1;
	srcDesc.PlacedFootprint.Footprint.Format = mTracerOutFormat;
	srcDesc.PlacedFootprint.Footprint.Width = mTracerOutW;
	srcDesc.PlacedFootprint.Footprint.Height = mTracerOutH;
	srcDesc.PlacedFootprint.Footprint.RowPitch = rowPitch;
	srcDesc.PlacedFootprint.Offset = 0;

	mCmdList_v0->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTracerOutTexture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCmdList_v0->CopyTextureRegion(&dstDesc, 0, 0, 0, &srcDesc, nullptr);
	mCmdList_v0->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTracerOutTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

	fillCommandLists();

	ThrowIfFailed(mCmdList_v0->Close());

	ID3D12CommandList* cmdsLists[] = { mCmdList_v0.Get() };
	mCmdQueue_v0->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain_v3->Present(1, 0));
	mCurrentBufferIdx = mSwapChain_v3->GetCurrentBackBufferIndex();

	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());
}