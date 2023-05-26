#include "Helpers.hlsli"

RaytracingAccelerationStructure scene : register(t0, space100);
RWBuffer<float4> tracerOutBuffer : register(u0);

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texCoord;
};

struct GPUSceneObject
{
	uint vertexOffset;
	uint tridexOffset;

	row_major float4x4 modelMatrix;
};

StructuredBuffer<GPUSceneObject> objectBuffer : register(t0);
StructuredBuffer<Vertex> vertexBuffer		  : register(t1);
Buffer<uint3> tridexBuffer					  : register(t2);

cbuffer GLOBAL_CONSTANTS : register(b0)
{
	float3 backgroundLight;
	float3 cameraPos;
	float4x4 invViewProj;
	uint accumulatedFrame;
	uint numSamplesPerFrame;
	uint maxPathLength;
}

cbuffer OBJECT_CONSTANTS : register(b1)
{
	uint objIdx;
}

struct RayPayload
{
	float4 color;
};

RayDesc Ray(in float3 origin, in float3 direction, in float tMin, in float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = tMin;
	ray.TMax = tMax;
	return ray;
}

void computeNormal(out float3 normal, in BuiltInTriangleIntersectionAttributes attr)
{
	GPUSceneObject obj = objectBuffer[objIdx];

	uint3 tridex = tridexBuffer[obj.tridexOffset + PrimitiveIndex()];
	Vertex vtx0 = vertexBuffer[obj.vertexOffset + tridex.x];
	Vertex vtx1 = vertexBuffer[obj.vertexOffset + tridex.y];
	Vertex vtx2 = vertexBuffer[obj.vertexOffset + tridex.z];

	float t0 = 1.0f - attr.barycentrics.x - attr.barycentrics.y;
	float t1 = attr.barycentrics.x;
	float t2 = attr.barycentrics.y;

	float3x3 transform = (float3x3) obj.modelMatrix;

	normal = normalize(mul(transform, t0 * vtx0.normal + t1 * vtx1.normal + t2 * vtx2.normal));
}

[shader("raygeneration")]
void rayGen()
{
	float2 launchIdx = DispatchRaysIndex().xy;
	float2 launchDim = DispatchRaysDimensions().xy;
	uint bufferOffset = launchDim.x * launchIdx.y + launchIdx.x;

	uint seed = getNewSeed(bufferOffset, accumulatedFrame, 8);

	float4 outColor = 0;

	RayPayload payload;
	payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

	for (uint i = 0; i < numSamplesPerFrame; i++)
	{
		float2 uv = ((launchIdx + float2(rand(seed), rand(seed))) / launchDim) * 2.f - 1.f;
		uv.y = -uv.y;

		RayDesc ray;
		ray.Origin = cameraPos;

		float4 world = mul(float4(uv, 1.0f, 1.0f), invViewProj);
		ray.Direction = normalize(world).xyz;

		ray.TMin = 0;
		ray.TMax = 100000;
		
		TraceRay(scene, 0, 0xFF, 0, 0, 0, ray, payload);

		outColor += payload.color;
	}

	outColor /= numSamplesPerFrame;

	tracerOutBuffer[bufferOffset] = outColor;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 hitNormal;
	computeNormal(hitNormal, attribs);

	if (dot(- WorldRayDirection(), hitNormal) < 0.f)
	{
		hitNormal = -hitNormal;
	}

	float3 color = 0.5 * float3(hitNormal.x + 1, hitNormal.y + 1, hitNormal.z + 1);

	payload.color = float4(color, 1.0);
}

[shader("miss")]
void missRay(inout RayPayload payload)
{
	float2 launchIdx = DispatchRaysIndex().xy;
	float2 launchDim = DispatchRaysDimensions().xy;

	float2 uv = launchIdx / launchDim;
	uv.y = 1 - uv.y;

	payload.color = float4((1.0 - uv.y) * float3(1.0, 1.0, 1.0) + uv.y * float3(0.5, 0.7, 1.0), 1.f);
}