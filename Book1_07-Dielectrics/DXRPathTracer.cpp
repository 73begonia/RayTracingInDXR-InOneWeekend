#include "DXRPathTracer.h"

namespace DescriptorID
{
	enum
	{
		outUAV = 0,

		sceneObjectBuff = 1,
		vertexBuff = 2,
		tridexBuff = 3,
		materialBuff = 4,

		maxDescriptors = 32
	};
}

namespace RootParamID
{
	enum
	{
		tableForOutBuffer = 0,
		pointerForAccelerationStructure = 1,
		tableForGeometryInputs = 2,
		pointerForGlobalConstants = 3,
		numParams
	};
}

namespace HitGroupParamID
{
	enum
	{
		constantForObject = 0,
		numParams
	};
}

void DXRPathTracer::onMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mTargetWindow);
}

void DXRPathTracer::onMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void DXRPathTracer::onMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.pitch(dy);
		mCamera.rotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
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

	mCamera.setLens(0.5f * XM_PI, float(mTracerOutW) / mTracerOutH, 1.0f, 1000.0f);

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
	globalRange.resize(2);

	globalRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	globalRange[0].NumDescriptors = 1;
	globalRange[0].BaseShaderRegister = 0;
	globalRange[0].RegisterSpace = 0;
	globalRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	globalRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	globalRange[1].NumDescriptors = 4;
	globalRange[1].BaseShaderRegister = 0;
	globalRange[1].RegisterSpace = 0;
	globalRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	vector<D3D12_ROOT_PARAMETER> globalRootParams;
	globalRootParams.resize(RootParamID::numParams);

	globalRootParams[RootParamID::tableForOutBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	globalRootParams[RootParamID::tableForOutBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	globalRootParams[RootParamID::tableForOutBuffer].DescriptorTable.NumDescriptorRanges = 1;
	globalRootParams[RootParamID::tableForOutBuffer].DescriptorTable.pDescriptorRanges = &globalRange[0];

	globalRootParams[RootParamID::pointerForAccelerationStructure].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	globalRootParams[RootParamID::pointerForAccelerationStructure].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	globalRootParams[RootParamID::pointerForAccelerationStructure].Descriptor.ShaderRegister = 0;
	globalRootParams[RootParamID::pointerForAccelerationStructure].Descriptor.RegisterSpace = 100;

	globalRootParams[RootParamID::tableForGeometryInputs].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	globalRootParams[RootParamID::tableForGeometryInputs].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	globalRootParams[RootParamID::tableForGeometryInputs].DescriptorTable.NumDescriptorRanges = 1;
	globalRootParams[RootParamID::tableForGeometryInputs].DescriptorTable.pDescriptorRanges = &globalRange[1];

	globalRootParams[RootParamID::pointerForGlobalConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	globalRootParams[RootParamID::pointerForGlobalConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	globalRootParams[RootParamID::pointerForGlobalConstants].Descriptor.ShaderRegister = 0;
	globalRootParams[RootParamID::pointerForGlobalConstants].Descriptor.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_DESC globalRootSigDesc = {};
	globalRootSigDesc.NumParameters = RootParamID::numParams;
	globalRootSigDesc.pParameters = globalRootParams.data();
	globalRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	mGlobalRS = buildRootSignatures(globalRootSigDesc);

	//Local
	vector<D3D12_ROOT_PARAMETER> localRootParams;
	localRootParams.resize(HitGroupParamID::numParams);
	localRootParams[HitGroupParamID::constantForObject].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	localRootParams[HitGroupParamID::constantForObject].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	localRootParams[HitGroupParamID::constantForObject].Constants.Num32BitValues = (sizeof(ObjectConstants) + 3) / 4;
	localRootParams[HitGroupParamID::constantForObject].Constants.ShaderRegister = 1;
	localRootParams[HitGroupParamID::constantForObject].Constants.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_DESC localRootSigDesc = {};
	localRootSigDesc.NumParameters = HitGroupParamID::numParams;
	localRootSigDesc.pParameters = localRootParams.data();
	localRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	mHitGroupRS = buildRootSignatures(localRootSigDesc);
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
	pipelineCfg.MaxTraceRecursionDepth = D3D12_RAYTRACING_MAX_DECLARABLE_TRACE_RECURSION_DEPTH;
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
	shaderCfg.MaxPayloadSizeInBytes = 56;
	shaderCfg.MaxAttributeSizeInBytes = 8;
	subObjShaderCfg.pDesc = (void*)&shaderCfg;
	subObjShaderCfg.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;

	subObjects[index++] = subObjShaderCfg;

	///Local Root Signature
	D3D12_STATE_SUBOBJECT subObjLocalRS = {};

	D3D12_LOCAL_ROOT_SIGNATURE lrsDesc;
	lrsDesc.pLocalRootSignature = mHitGroupRS.Get();
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
	mCamera.setLens(0.5f * XM_PI, float(mTracerOutW) / mTracerOutH, 1.0f, 1000.0f);
	mCamera.lookAt(mCamera.getPosition(), mCamera.getLook(), mCamera.getUp());

	mGlobalConstants.backgroundLight = float3(0.8f, 0.1f, 0.5f);
	mGlobalConstants.maxPathLength = 48;
	mGlobalConstants.numSamplesPerFrame = 8;

	mGlobalConstantsBuffer = createCommittedBuffer(sizeof(GlobalConstants));

	mMaxBufferSize = _bpp(mTracerOutFormat) * 1920 * 1080;
	mMaxBufferSize = _align(mMaxBufferSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	mReadBackBuffer = createCommittedBuffer(mMaxBufferSize, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
}

void DXRPathTracer::update()
{
	mCamera.update();

	if (mCamera.notifyChanged())
	{
		mGlobalConstants.cameraPos = mCamera.getPosition3f();
		mGlobalConstants.accumulatedFrame = 0;

		XMMATRIX view = mCamera.getView();
		XMMATRIX proj = mCamera.getProj();

		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		XMStoreFloat4x4(&mGlobalConstants.invViewProj, XMMatrixTranspose(invViewProj));
	}
	else
		mGlobalConstants.accumulatedFrame++;

	uint8* pGlobalConstants;
	ThrowIfFailed(mGlobalConstantsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pGlobalConstants)));
	memcpy(pGlobalConstants, &mGlobalConstants, sizeof(GlobalConstants));
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
	desc.HitGroupTable.StrideInBytes = _align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + (uint)sizeof(ObjectConstants), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
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

	//pointerForAccelerationStructure
	mCmdList_v4->SetComputeRootShaderResourceView((uint)RootParamID::pointerForAccelerationStructure, mTopLevelAccelerationStructure->GetGPUVirtualAddress());

	//tableForGeometryInputs
	D3D12_GPU_DESCRIPTOR_HANDLE objGeometryInputsHandle = mSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	objGeometryInputsHandle.ptr += ((uint)DescriptorID::sceneObjectBuff) * mSrvDescriptorSize;
	mCmdList_v4->SetComputeRootDescriptorTable((uint)RootParamID::tableForGeometryInputs, objGeometryInputsHandle);

	//pointerForGlobalConstants
	mCmdList_v4->SetComputeRootConstantBufferView((uint)RootParamID::pointerForGlobalConstants, mGlobalConstantsBuffer->GetGPUVirtualAddress());

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

	uint numObjs = mScene->numObjects();

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
		uint nNumShaderRecords = numObjs;
		uint nShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(ObjectConstants);
		nShaderRecordSize = _align(nShaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		n64AllocSize = nNumShaderRecords * nShaderRecordSize;

		mHitGroupShaderTable = createPlacedBuffer(mShaderTableHeap_v1, n64HeapOffset, n64AllocSize, D3D12_RESOURCE_STATE_GENERIC_READ);
		pBufs = nullptr;
		ThrowIfFailed(mHitGroupShaderTable->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs)));

		objConsts.resize(numObjs);
		for (uint i = 0; i < numObjs; ++i)
		{
			objConsts[i].objectIdx = i;

			memcpy(pBufs + nShaderRecordSize * i, pHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			memcpy(pBufs + nShaderRecordSize * i + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &objConsts[i], sizeof(ObjectConstants));
		}

		mHitGroupShaderTable->Unmap(0, nullptr);
	}
}

ComPtr<ID3D12Resource> DXRPathTracer::createAS(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& buildInput, ComPtr<ID3D12Resource>* scrach)
{
	ComPtr<ID3D12Resource> AS;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	mDevice_v5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInput, &info);

	*scrach = createCommittedBuffer(info.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	AS = createCommittedBuffer(info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = buildInput;
	asDesc.DestAccelerationStructureData = AS->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = (*scrach)->GetGPUVirtualAddress();;
	mCmdList_v4->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = AS.Get();
	mCmdList_v4->ResourceBarrier(1, &uavBarrier);

	return AS;
}

void DXRPathTracer::buildBLAS(
	ComPtr<ID3D12Resource>* blas,
	ComPtr<ID3D12Resource>* scrach,
	const GPUMesh gpuMeshArr[],
	uint numMeshes,
	uint vertexStride,
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
	D3D12_RAYTRACING_GEOMETRY_DESC meshDesc = {};
	meshDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	meshDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	meshDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	meshDesc.Triangles.VertexBuffer.StrideInBytes = vertexStride;
	meshDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

	vector<D3D12_RAYTRACING_GEOMETRY_DESC> geoDesc(numMeshes);
	for (uint i = 0; i < numMeshes; ++i)
	{
		geoDesc[i] = meshDesc;
		geoDesc[i].Triangles.VertexCount = gpuMeshArr[i].numVertices;
		geoDesc[i].Triangles.VertexBuffer.StartAddress = gpuMeshArr[i].vertexBufferVA;
		geoDesc[i].Triangles.IndexCount = gpuMeshArr[i].numTridices * 3;
		geoDesc[i].Triangles.IndexBuffer = gpuMeshArr[i].tridexBufferVA;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInput = {};
	buildInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	buildInput.NumDescs = numMeshes;
	buildInput.Flags = buildFlags;
	buildInput.pGeometryDescs = geoDesc.data();

	*blas = createAS(buildInput, scrach);
}

void DXRPathTracer::buildTLAS(
	ComPtr<ID3D12Resource>* tlas,
	ComPtr<ID3D12Resource>* scrach,
	ComPtr<ID3D12Resource>* instanceDescArr,
	ID3D12Resource* const blasArr[],
	const dxTransform transformArr[],
	uint numBlas,
	uint instanceMultiplier,
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
	*instanceDescArr = createCommittedBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numBlas);

	D3D12_RAYTRACING_INSTANCE_DESC* pInsDescArr;
	(*instanceDescArr)->Map(0, nullptr, (void**)&pInsDescArr);
	{
		memset(pInsDescArr, 0, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numBlas);
		for (uint i = 0; i < numBlas; ++i)
		{
			pInsDescArr[i].InstanceMask = 0xFF;
			pInsDescArr[i].InstanceContributionToHitGroupIndex = i * instanceMultiplier;
			*(dxTransform*)(pInsDescArr[i].Transform) = transformArr[i];
			pInsDescArr[i].AccelerationStructure = const_cast<ID3D12Resource*>(blasArr[i])->GetGPUVirtualAddress();
		}
	}
	(*instanceDescArr)->Unmap(0, nullptr);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInput = {};
	buildInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	buildInput.NumDescs = numBlas;
	buildInput.Flags = buildFlags;
	buildInput.InstanceDescs = (*instanceDescArr)->GetGPUVirtualAddress();

	*tlas = createAS(buildInput, scrach);
}

void DXRPathTracer::buildAccelerationStructure()
{
	uint numObjs = mScene->numObjects();
	vector<GPUMesh> gpuMeshArr(numObjs);
	vector<dxTransform> transformArr(numObjs);

	D3D12_GPU_VIRTUAL_ADDRESS vtxAddr = mVertexBuffer->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS tdxAddr = mIndexBuffer->GetGPUVirtualAddress();
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = mScene->getObject(objIdx);

		gpuMeshArr[objIdx].numVertices = obj.numVertices;
		gpuMeshArr[objIdx].vertexBufferVA = vtxAddr + obj.vertexOffset * sizeof(Vertex);
		gpuMeshArr[objIdx].numTridices = obj.numTridices;
		gpuMeshArr[objIdx].tridexBufferVA = tdxAddr + obj.tridexOffset * sizeof(Tridex);

		transformArr[objIdx] = obj.modelMatrix;
	}

	assert(mTopLevelAccelerationStructure == nullptr);
	assert(gpuMeshArr.size() == transformArr.size());

	uint numObjsPerBlas = 1;
	uint numBottomLevels = numObjs;
	mBottomLevelAccelerationStructure.resize(numBottomLevels, nullptr);
	Scratch.resize(numBottomLevels + 1, nullptr);

	for (uint i = 0; i < numBottomLevels; ++i)
	{
		buildBLAS(&mBottomLevelAccelerationStructure[i], &Scratch[i], &gpuMeshArr[i], numObjsPerBlas, sizeof(Vertex), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);
	}

	const vector<dxTransform>* topLevelTransform;
	vector<dxTransform> identityArr;
	topLevelTransform = &transformArr;

	buildTLAS(&mTopLevelAccelerationStructure, &Scratch[numBottomLevels], &InstanceDesc, mBottomLevelAccelerationStructure[0].GetAddressOf(), &(*topLevelTransform)[0], numBottomLevels, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);

	ThrowIfFailed(mCmdList_v4->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList_v4.Get() };
	mCmdQueue_v0->ExecuteCommandLists(1, cmdLists);
	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());
	ThrowIfFailed(mCmdAllocator_v0->Reset());
	ThrowIfFailed(mCmdList_v4->Reset(mCmdAllocator_v0.Get(), nullptr));
}

void DXRPathTracer::setupScene(const Scene* scene)
{
	uint numObjs = scene->numObjects();

	const vector<Vertex> vtxArr = scene->getVertexArray();
	const vector<Tridex> tdxArr = scene->getTridexArray();
	const vector<Material> mtlArr = scene->getMaterialArray();

	uint64 vtxBuffSize = vtxArr.size() * sizeof(Vertex);
	uint64 tdxBuffSize = tdxArr.size() * sizeof(Tridex);
	uint64 mtlBuffSize = mtlArr.size() * sizeof(Material);
	uint64 objBuffSize = numObjs * sizeof(GPUSceneObject);

	ComPtr<ID3D12Resource> uploader = createCommittedBuffer(
		vtxBuffSize + tdxBuffSize + mtlBuffSize + objBuffSize);
	uint64 uploaderOffset = 0;

	auto initBuffer = [&](ComPtr<ID3D12Resource>& buff, uint64 buffSize, void* srcData)
	{
		if (buffSize == 0)
			return;
		buff = createCommittedBuffer(buffSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON);

		uint8* pBufs = nullptr;
		uploader->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pBufs));
		memcpy(pBufs + uploaderOffset, srcData, buffSize);
		mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buff.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		mCmdList_v4->CopyBufferRegion(buff.Get(), 0, uploader.Get(), uploaderOffset, buffSize);
		mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buff.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
		uploaderOffset += buffSize;
	};

	initBuffer(mVertexBuffer, vtxBuffSize, (void*)vtxArr.data());
	initBuffer(mIndexBuffer, tdxBuffSize, (void*)tdxArr.data());
	initBuffer(mMaterialBuffer, mtlBuffSize, (void*)mtlArr.data());

	mSceneObjectBuffer = createCommittedBuffer(objBuffSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON);

	void* cpuAddress;
	uploader->Map(0, &CD3DX12_RANGE(0, 0), &cpuAddress);
	GPUSceneObject* copyDst = (GPUSceneObject*)((uint8*)cpuAddress + uploaderOffset);
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = scene->getObject(objIdx);

		GPUSceneObject gpuObj = {};
		gpuObj.vertexOffset = obj.vertexOffset;
		gpuObj.tridexOffset = obj.tridexOffset;
		gpuObj.materialIdx = obj.materialIdx;
		gpuObj.modelMatrix = obj.modelMatrix;

		copyDst[objIdx] = gpuObj;
	}

	mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSceneObjectBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCmdList_v4->CopyBufferRegion(mSceneObjectBuffer.Get(), 0, uploader.Get(), uploaderOffset, objBuffSize);
	mCmdList_v4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSceneObjectBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

	ThrowIfFailed(mCmdList_v4->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList_v4.Get() };
	mCmdQueue_v0->ExecuteCommandLists(1, cmdLists);
	mFence_v0.waitCommandQueue(mCmdQueue_v0.Get());
	ThrowIfFailed(mCmdAllocator_v0->Reset());
	ThrowIfFailed(mCmdList_v4->Reset(mCmdAllocator_v0.Get(), nullptr));

	this->mScene = const_cast<Scene*>(scene);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//SceneObjectBuffer
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(GPUSceneObject);
		srvDesc.Buffer.NumElements = numObjs;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE sceneObjectHandle = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	sceneObjectHandle.ptr += (uint)DescriptorID::sceneObjectBuff * mSrvDescriptorSize;
	mDevice_v5->CreateShaderResourceView(mSceneObjectBuffer.Get(), &srvDesc, sceneObjectHandle);

	//Vertex srv
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.NumElements = (uint)vtxArr.size();
		srvDesc.Buffer.StructureByteStride = sizeof(Vertex);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE vertexSrvHandle = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	vertexSrvHandle.ptr += uint(DescriptorID::vertexBuff) * mSrvDescriptorSize;
	mDevice_v5->CreateShaderResourceView(mVertexBuffer.Get(), &srvDesc, vertexSrvHandle);

	//Index srv
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
		srvDesc.Buffer.NumElements = (uint)tdxArr.size();
		srvDesc.Buffer.StructureByteStride = 0;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE indexSrvHandle = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	indexSrvHandle.ptr += (uint)DescriptorID::tridexBuff * mSrvDescriptorSize;
	mDevice_v5->CreateShaderResourceView(mIndexBuffer.Get(), &srvDesc, indexSrvHandle);

	//MaterialBuffer
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(Material);
		srvDesc.Buffer.NumElements = (uint)mtlArr.size();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE cpuMaterialBuffHandle = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	cpuMaterialBuffHandle.ptr += (uint)DescriptorID::materialBuff * mSrvDescriptorSize;
	mDevice_v5->CreateShaderResourceView(mMaterialBuffer.Get(), &srvDesc, cpuMaterialBuffHandle);

	setupShaderTable();

	buildAccelerationStructure();
}