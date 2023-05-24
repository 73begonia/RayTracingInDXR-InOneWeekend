#include "DXRPathTracer.h"

namespace DescriptorID
{
	enum
	{
		outUAV = 0,
		maxDescriptors = 32
	};
}

namespace RootParamID
{
	enum
	{
		tableForOutBuffer = 0,
		numParams
	};
}

DXRPathTracer::~DXRPathTracer()
{
}

DXRPathTracer::DXRPathTracer(HWND hwnd, uint width, uint height) :
	mTargetWindow(hwnd), mTracerOutW(width), mTracerOutH(height)
{
	initD3D12();

	createSrvUavHeap();

	onSizeChanged(mTracerOutW, mTracerOutH);

	declareRootSignatures();

	buildRaytracingPipeline();

	initializeApplication();
}

void DXRPathTracer::initD3D12()
{
	ThrowIfFailed(createDX12Device(getRTXAdapter().Get())->QueryInterface(IID_PPV_ARGS(&mDevice_v5)));

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(mDevice_v5->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCmdQueue_v0)));
	ThrowIfFailed(mDevice_v5->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator_v0)));
	ThrowIfFailed(mDevice_v5->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator_v0.Get(), nullptr, IID_PPV_ARGS(&mCmdList_v4)));

	ThrowIfFailed(mCmdList_v4->Close());
	ThrowIfFailed(mCmdAllocator_v0->Reset());
	ThrowIfFailed(mCmdList_v4->Reset(mCmdAllocator_v0.Get(), nullptr));
	mFence_v0.create(mDevice_v5.Get());
}

void DXRPathTracer::createSrvUavHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	{
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = DescriptorID::maxDescriptors;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	mDevice_v5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvUavHeap));
	mSrvDescriptorSize = mDevice_v5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DXRPathTracer::onSizeChanged(uint width, uint height)
{
	mTracerOutW = width;
	mTracerOutH = height;

	if (mTracerOutBuffer != nullptr)
		mTracerOutBuffer.Reset();

	uint64 bufferSize = _bpp(mTracerOutFormat) * mTracerOutW * mTracerOutH;
	mTracerOutBuffer = createCommittedBuffer(bufferSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format = mTracerOutFormat;
		uavDesc.Buffer.NumElements = mTracerOutW * mTracerOutH;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	uavDescriptorHandle.ptr += ((uint)DescriptorID::outUAV) * mSrvDescriptorSize;
	mDevice_v5->CreateUnorderedAccessView(mTracerOutBuffer.Get(), nullptr, &uavDesc, uavDescriptorHandle);
}

ComPtr<ID3D12RootSignature> DXRPathTracer::buildRootSignatures(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	ComPtr<ID3DBlob> pSigBlob;
	ComPtr<ID3DBlob> pErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);

	if (pErrorBlob)
		throw Error((char*)pErrorBlob->GetBufferPointer());

	ComPtr<ID3D12RootSignature> pRootSig;
	ThrowIfFailed(mDevice_v5->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig)));

	return pRootSig;
}

void DXRPathTracer::declareRootSignatures()
{
	//Global
	vector<D3D12_DESCRIPTOR_RANGE> globalRange;
	globalRange.resize(1);

	globalRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	globalRange[0].NumDescriptors = 1;
	globalRange[0].BaseShaderRegister = 0;
	globalRange[0].RegisterSpace = 0;
	globalRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	vector<D3D12_ROOT_PARAMETER> globalRootParams;
	globalRootParams.resize(RootParamID::numParams);

	globalRootParams[RootParamID::tableForOutBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	globalRootParams[RootParamID::tableForOutBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	globalRootParams[RootParamID::tableForOutBuffer].DescriptorTable.NumDescriptorRanges = 1;
	globalRootParams[RootParamID::tableForOutBuffer].DescriptorTable.pDescriptorRanges = &globalRange[0];

	D3D12_ROOT_SIGNATURE_DESC globalRootSigDesc = {};
	globalRootSigDesc.NumParameters = RootParamID::numParams;
	globalRootSigDesc.pParameters = globalRootParams.data();
	globalRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	mGlobalRS = buildRootSignatures(globalRootSigDesc);

	//Local
}

void DXRPathTracer::buildRaytracingPipeline()
{
	vector<D3D12_STATE_SUBOBJECT> subObjects;
	subObjects.resize(7);
	uint index = 0;

	//Global Root Signature
	D3D12_STATE_SUBOBJECT subObjGlobalRS = {};

	D3D12_GLOBAL_ROOT_SIGNATURE grsDesc;
	grsDesc.pGlobalRootSignature = mGlobalRS.Get();
	subObjGlobalRS.pDesc = (void*)&grsDesc;
	subObjGlobalRS.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;

	subObjects[index++] = subObjGlobalRS;

	//Raytracing Pipeline Config
	D3D12_STATE_SUBOBJECT subObjPipelineCfg = {};

	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg = {};
	pipelineCfg.MaxTraceRecursionDepth = 2;
	subObjPipelineCfg.pDesc = &pipelineCfg;
	subObjPipelineCfg.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;

	subObjects[index++] = subObjPipelineCfg;

	//DXIL Library
	D3D12_STATE_SUBOBJECT subObjDXILLib = {};

	mDxrLib.load(L"DXRShader.cso");
	D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
	dxilLibDesc.DXILLibrary = mDxrLib.getCode();
	subObjDXILLib.pDesc = (void*)&dxilLibDesc;
	subObjDXILLib.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;

	subObjects[index++] = subObjDXILLib;

	//HitGroup Shader Table Group
	D3D12_STATE_SUBOBJECT subObjHitGroup = {};

	D3D12_HIT_GROUP_DESC hitGroupDesc;
	hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	hitGroupDesc.ClosestHitShaderImport = cClosestHitShaderName;
	hitGroupDesc.AnyHitShaderImport = nullptr;
	hitGroupDesc.HitGroupExport = cHitGroupName;
	hitGroupDesc.IntersectionShaderImport = nullptr;
	subObjHitGroup.pDesc = (void*)&hitGroupDesc;
	subObjHitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;

	subObjects[index++] = subObjHitGroup;

	//Raytracing Shader Config
	D3D12_STATE_SUBOBJECT subObjShaderCfg = {};

	D3D12_RAYTRACING_SHADER_CONFIG shaderCfg = {};
	shaderCfg.MaxPayloadSizeInBytes = 16;
	shaderCfg.MaxAttributeSizeInBytes = 8;
	subObjShaderCfg.pDesc = (void*)&shaderCfg;
	subObjShaderCfg.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;

	subObjects[index++] = subObjShaderCfg;

	//Local Root Signature
	D3D12_STATE_SUBOBJECT subObjLocalRS = {};

	D3D12_LOCAL_ROOT_SIGNATURE lrsDesc;
	lrsDesc.pLocalRootSignature = nullptr;
	subObjLocalRS.pDesc = (void*)&lrsDesc;
	subObjLocalRS.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;

	uint localObjIdx = index;
	subObjects[index++] = subObjLocalRS;

	//Association
	D3D12_STATE_SUBOBJECT subObjAssoc = {};

	vector<LPCWSTR> exportName;
	exportName.resize(1);
	exportName[0] = cHitGroupName;

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION obj2ExportsAssoc = {};
	obj2ExportsAssoc.pSubobjectToAssociate = &subObjects[localObjIdx];
	obj2ExportsAssoc.NumExports = (uint)exportName.size();
	obj2ExportsAssoc.pExports = exportName.data();

	subObjAssoc.pDesc = &obj2ExportsAssoc;
	subObjAssoc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;

	subObjects[index++] = subObjAssoc;

	assert(index == subObjects.size());

	//Create Raytracing PSO
	D3D12_STATE_OBJECT_DESC raytracingPSODesc = {};
	raytracingPSODesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	raytracingPSODesc.NumSubobjects = (uint)subObjects.size();
	raytracingPSODesc.pSubobjects = subObjects.data();

	ThrowIfFailed(mDevice_v5->CreateStateObject(&raytracingPSODesc, IID_PPV_ARGS(&mRTPipeline)));
}


void DXRPathTracer::initializeApplication()
{
	mMaxBufferSize = _bpp(mTracerOutFormat) * 1920 * 1080;
	mMaxBufferSize = _align(mMaxBufferSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	mReadBackBuffer = createCommittedBuffer(mMaxBufferSize, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
}

void DXRPathTracer::update()
{
}

void DXRPathTracer::setupScene()
{
	setupShaderTable();
}

TracedResult DXRPathTracer::shootRays()
{
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//rayGen
	desc.RayGenerationShaderRecord.StartAddress = mRayGenShaderTable->GetGPUVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = mRayGenShaderTable->GetDesc().Width;

	//miss
	desc.MissShaderTable.StartAddress = mMissShaderTable->GetGPUVirtualAddress();
	desc.MissShaderTable.StrideInBytes = _align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	desc.MissShaderTable.SizeInBytes = mMissShaderTable->GetDesc().Width;

	//hit
	desc.HitGroupTable.StartAddress = mHitGroupShaderTable->GetGPUVirtualAddress();
	desc.HitGroupTable.StrideInBytes = _align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	desc.HitGroupTable.SizeInBytes = mHitGroupShaderTable->GetDesc().Width;

	desc.Width = mTracerOutW;
	desc.Height = mTracerOutH;
	desc.Depth = 1;

	mCmdList_v4->SetPipelineState1(mRTPipeline.Get());
	ID3D12DescriptorHeap* ppDescriptorHeaps[] = { mSrvUavHeap.Get() };
	mCmdList_v4->SetDescriptorHeaps(1, ppDescriptorHeaps);
	mCmdList_v4->SetComputeRootSignature(mGlobalRS.Get());

	//tableForOutBuffer
	D3D12_GPU_DESCRIPTOR_HANDLE objUAVHandle = mSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	objUAVHandle.ptr += ((uint)DescriptorID::outUAV * mSrvDescriptorSize);
	mCmdList_v4->SetComputeRootDescriptorTable((uint)RootParamID::tableForOutBuffer, objUAVHandle);

	mCmdList_v4->DispatchRays(&desc);

	D3D12_RESOURCE_DESC tracerBufferDesc = mTracerOutBuffer->GetDesc();
	if (tracerBufferDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		if (tracerBufferDesc.Width > mMaxBufferSize)
		{
			mMaxBufferSize = _align(tracerBufferDesc.Width * 2, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			mReadBackBuffer = createCommittedBuffer(mMaxBufferSize, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
		}

		mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTracerOutBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
		mCmdList_v4->CopyBufferRegion(mReadBackBuffer.Get(), 0, mTracerOutBuffer.Get(), 0, tracerBufferDesc.Width);
		mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTracerOutBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	ThrowIfFailed(mCmdList_v4->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList_v4.Get() };
	mCmdQueue_v0->ExecuteCommandLists(1, cmdLists);
	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());
	ThrowIfFailed(mCmdAllocator_v0->Reset());
	ThrowIfFailed(mCmdList_v4->Reset(mCmdAllocator_v0.Get(), nullptr));

	uint8* tracedResultData;
	ThrowIfFailed(mReadBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&tracedResultData)));
	TracedResult result;
	result.data = tracedResultData;
	result.width = mTracerOutW;
	result.height = mTracerOutH;
	result.pixelSize = _bpp(mTracerOutFormat);

	mReadBackBuffer->Unmap(0, nullptr);

	return result;
}

void DXRPathTracer::setupShaderTable()
{
	void* pRaygenShaderIdentifier;
	void* pMissShaderIdentifier;
	void* pHitGroupShaderIdentifier;

	ComPtr<ID3D12StateObjectProperties> pStateObjectProperties;
	ThrowIfFailed(mRTPipeline->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties)));

	pRaygenShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(cRayGenShaderName);
	pMissShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(cMissShaderName);
	pHitGroupShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(cHitGroupName);

	D3D12_HEAP_DESC uploadHeapDesc = {};
	uint64 n64HeapSize = 1024 * 1024;
	uint64 n64HeapOffset = 0;
	uint64 n64AllocSize = 0;
	uint8* pBufs = nullptr;

	uploadHeapDesc.SizeInBytes = _align(n64HeapSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	uploadHeapDesc.Alignment = 0;
	uploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

	ThrowIfFailed(mDevice_v5->CreateHeap(&uploadHeapDesc, IID_PPV_ARGS(&mShaderTableHeap_v1)));

	//rayGen shader table
	{
		uint nNumShaderRecords = 1;
		uint nShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		n64AllocSize = nNumShaderRecords * nShaderRecordSize;
		n64AllocSize = _align(n64AllocSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		mRayGenShaderTable = createPlacedBuffer(mShaderTableHeap_v1, n64HeapOffset, n64AllocSize, D3D12_RESOURCE_STATE_GENERIC_READ);
		ThrowIfFailed(mRayGenShaderTable->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs)));

		memcpy(pBufs, pRaygenShaderIdentifier, nShaderRecordSize);

		mRayGenShaderTable->Unmap(0, nullptr);
	}

	n64HeapOffset += _align(n64AllocSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	ThrowIfFalse(n64HeapOffset < n64HeapSize);

	//miss shader table
	{
		uint nNumShaderRecords = 1;
		uint nShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		n64AllocSize = nNumShaderRecords * nShaderRecordSize;
		n64AllocSize = _align(n64AllocSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		mMissShaderTable = createPlacedBuffer(mShaderTableHeap_v1, n64HeapOffset, n64AllocSize, D3D12_RESOURCE_STATE_GENERIC_READ);
		pBufs = nullptr;
		ThrowIfFailed(mMissShaderTable->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs)));

		memcpy(pBufs, pMissShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

		mMissShaderTable->Unmap(0, nullptr);
	}

	n64HeapOffset += _align(n64AllocSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	ThrowIfFailed(n64HeapOffset < n64HeapSize);

	//hit group shader table
	{
		uint nNumShaderRecords = 1;
		uint nShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		nShaderRecordSize = _align(nShaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		n64AllocSize = nNumShaderRecords * nShaderRecordSize;

		mHitGroupShaderTable = createPlacedBuffer(mShaderTableHeap_v1, n64HeapOffset, n64AllocSize, D3D12_RESOURCE_STATE_GENERIC_READ);
		pBufs = nullptr;
		ThrowIfFailed(mHitGroupShaderTable->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs)));

		memcpy(pBufs, pHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

		mHitGroupShaderTable->Unmap(0, nullptr);
	}
}