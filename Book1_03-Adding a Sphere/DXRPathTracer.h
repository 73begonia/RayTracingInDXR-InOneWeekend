#pragma once
#include "dxHelper.h"
#include "Camera.h"
#include "Scene.h"

using pFloat4 = float(*)[4];
struct dxTransform
{
	float mat[3][4];
	dxTransform() {}
	dxTransform(float scale) : mat{}
	{
		mat[0][0] = mat[1][1] = mat[2][2] = scale;
	}
	void operator=(const Transform& tm)
	{
		*this = *(dxTransform*)&tm;
	}
	pFloat4 data() { return mat; }
};


#define NextAlignedLine __declspec(align(16))

struct GlobalConstants
{
	NextAlignedLine
	float3 backgroundLight;
	NextAlignedLine
	XMFLOAT3 cameraPos;
	NextAlignedLine
	XMFLOAT4X4 invViewProj;
};

class DXRPathTracer
{
	uint mTracerOutW;
	uint mTracerOutH;
	HWND mTargetWindow;

	static const DXGI_FORMAT mTracerOutFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ComPtr<ID3D12Device5> mDevice_v5;
	ComPtr<ID3D12CommandQueue> mCmdQueue_v0;
	ComPtr<ID3D12GraphicsCommandList4> mCmdList_v4;
	ComPtr<ID3D12CommandAllocator> mCmdAllocator_v0;
	BinaryFence mFence_v0;
	void initD3D12();

	ComPtr<ID3D12DescriptorHeap> mSrvUavHeap;
	uint mSrvDescriptorSize;
	void createSrvUavHeap();

	ComPtr<ID3D12RootSignature> mGlobalRS;
	ComPtr<ID3D12RootSignature> buildRootSignatures(const D3D12_ROOT_SIGNATURE_DESC& desc);
	void declareRootSignatures();

	dxShader mDxrLib;
	ComPtr<ID3D12StateObject> mRTPipeline;
	void buildRaytracingPipeline();

	GlobalConstants mGlobalConstants;
	ComPtr<ID3D12Resource> mGlobalConstantsBuffer;
	ComPtr<ID3D12Resource> mTracerOutBuffer;
	uint64 mMaxBufferSize;
	ComPtr<ID3D12Resource> mReadBackBuffer;
	void initializeApplication();

	ComPtr<ID3D12Resource> mVertexBuffer;
	ComPtr<ID3D12Resource> mIndexBuffer;

	ComPtr<ID3D12Heap1> mShaderTableHeap_v1;
	ComPtr<ID3D12Resource> mRayGenShaderTable;
	ComPtr<ID3D12Resource> mMissShaderTable;
	ComPtr<ID3D12Resource> mHitGroupShaderTable;
	const wchar* cRayGenShaderName = L"rayGen";
	const wchar* cMissShaderName = L"missRay";
	const wchar* cHitGroupName = L"hitGp";
	const wchar* cClosestHitShaderName = L"closestHit";
	void setupShaderTable();

	ComPtr<ID3D12Resource> bottomScratch;
	ComPtr<ID3D12Resource> bottomResult;
	ComPtr<ID3D12Resource> topScratch;
	ComPtr<ID3D12Resource> topResult;
	ComPtr<ID3D12Resource> topInstanceDesc;
	vector<ComPtr<ID3D12Resource>> mBottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> mTopLevelAccelerationStructure;
	void buildAccelerationStructure();

	int2 mLastMousePos;
	Camera mCamera;

public:
	Camera getCamera() { return mCamera; }
	void onMouseDown(WPARAM btnState, int x, int y);
	void onMouseUp(WPARAM btnState, int x, int y);
	void onMouseMove(WPARAM btnState, int x, int y);

	Mesh sphere;
	void setupScene();
	TracedResult shootRays();

public:
	~DXRPathTracer();
	DXRPathTracer(HWND hwnd, uint width, uint height);
	void onSizeChanged(uint width, uint height);
	void update();
};