#pragma once
#include "pch.h"
#include "dxHelper.h"

class D3D12Screen
{
	HWND mTargetWindow;
	uint mScreenW;
	uint mScreenH;

	uint mTracerOutW;
	uint mTracerOutH;

	static const DXGI_FORMAT mTracerOutFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	static const DXGI_FORMAT mScreenOutFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const uint mBackBufferCount = 2;

	ComPtr<ID3D12Device> mDevice_v0;
	ComPtr<ID3D12CommandQueue> mCmdQueue_v0;
	ComPtr<ID3D12GraphicsCommandList> mCmdList_v0;
	ComPtr<ID3D12CommandAllocator> mCmdAllocator_v0;
	BinaryFence mFence_v0;
	void initD3D12();

	ComPtr<IDXGISwapChain3> mSwapChain_v3;
	uint mCurrentBufferIdx;
	vector<ComPtr<ID3D12Resource>> mRenderTargetArr;
	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	uint mRtvDescriptorSize;
	void createSwapChain();

	ComPtr<ID3D12DescriptorHeap> mSrvUavHeap;
	void createSrvUavHeap(uint maxDescriptors);

	ComPtr<ID3D12RootSignature> mRootSig;
	void declareRootSignature();

	ComPtr<ID3D12PipelineState> mPipeline;
	void createPipeline();

	ComPtr<ID3D12Resource> mTracerOutTexture;
	ComPtr<ID3D12Resource> mTextureUploader;
	void initializeResource();

	void fillCommandLists();

public:
	D3D12Screen(HWND hwnd, uint width, uint height);
	~D3D12Screen();

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;
	void onSizeChanged(uint width, uint height);
	void display(const TracedResult& trResult);
};