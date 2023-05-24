#include "Helpers.hlsli"

RaytracingAccelerationStructure scene : register(t0, space100);
RWBuffer<float4> tracerOutBuffer : register(u0);

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texCoord;
};

enum MaterialType
{
	Lambertian = 0,
	Metal = 1,
	Dielectric = 2
};

struct Material
{
	float3 Ks;
	float3 Kr;
	float3 Kt;
	float3 albedo;
	float3 opacity;
	float3 eta;
	float fuzz;
	float refractionIndex;

	MaterialType type;
};

struct GPUSceneObject
{
	uint vertexOffset;
	uint tridexOffset;
	uint materialIdx;

	row_major float4x4 modelMatrix;
};

StructuredBuffer<GPUSceneObject> objectBuffer : register(t0);
StructuredBuffer<Vertex> vertexBuffer		  : register(t1);
Buffer<uint3> tridexBuffer					  : register(t2);
StructuredBuffer<Material> materialBuffer	  : register(t3);

cbuffer GLOBAL_CONSTANTS : register(b0)
{
	float3 backgroundLight;
	float3 cameraPos;
	float4x4 invViewProj;
	uint accumulatedFrames;
	uint numSamplesPerFrame;
	uint maxPathLength;
}

cbuffer OBJECT_CONSTANTS : register(b1)
{
	uint objIdx;
}

struct RayPayload
{
	float3 radiance;
	float3 attenuation;
	float3 hitPos;
	float3 bounceDir;
	uint rayDepth;
	uint seed;
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

float3 tracePath(in float3 startPos, in float3 startDir, inout uint seed)
{
	float3 radiance = 0.0f;
	float3 attenuation = 1.0f;

	RayDesc ray = Ray(startPos, startDir, 1e-4f, 1e27f);
	RayPayload prd;
	prd.seed = seed;
	prd.rayDepth = 0;

	while (prd.rayDepth < maxPathLength)
	{
		TraceRay(scene, 0, ~0, 0, 1, 0, ray, prd);

		radiance += attenuation * prd.radiance;
		attenuation *= prd.attenuation;

		ray.Origin = prd.hitPos;
		ray.Direction = prd.bounceDir;
		++prd.rayDepth;
	}

	return radiance;
}

[shader("raygeneration")]
void rayGen()
{
	float2 launchIdx = DispatchRaysIndex().xy;
	float2 launchDim = DispatchRaysDimensions().xy;
	uint bufferOffset = launchDim.x * launchIdx.y + launchIdx.x;

	uint seed = getNewSeed(bufferOffset, accumulatedFrames, 8);

	float3 newRadiance = 0.0f;

	RayPayload payload;

	for (uint i = 0; i < numSamplesPerFrame; i++)
	{
		float2 uv = ((launchIdx + float2(rand(seed), rand(seed))) / launchDim) * 2.f - 1.f;
		uv.y = -uv.y;

		float4 world = mul(float4(uv, 1.0f, 1.0f), invViewProj);

		newRadiance += tracePath(cameraPos, normalize(world).xyz, seed);
	}

	newRadiance *= 1.0f / float(numSamplesPerFrame);

	float3 avrRadiance;
	if (accumulatedFrames == 0)
		avrRadiance = newRadiance;
	else
		avrRadiance = lerp(tracerOutBuffer[bufferOffset].xyz, newRadiance, 1.f / (accumulatedFrames + 1.0f));

	tracerOutBuffer[bufferOffset] = float4(avrRadiance, 1.0f);
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 hitNormal;
	computeNormal(hitNormal, attribs);

	GPUSceneObject obj = objectBuffer[objIdx];
	uint mtlIdx = obj.materialIdx;
	Material material = materialBuffer[mtlIdx];

	payload.radiance = 0.f;
	payload.attenuation = 1.f;
	payload.hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	if (++payload.rayDepth == maxPathLength)
	{
		payload.attenuation = 0.f;
		return;
	}

	if (material.type == MaterialType::Lambertian)
	{
		payload.attenuation = material.albedo;

		float3 target = hitNormal + random_unit_vector(payload.seed);
		payload.bounceDir = target;
	}
	else if (material.type == MaterialType::Metal)
	{
		payload.attenuation = material.albedo;

		float3 reflected = reflect(WorldRayDirection(), hitNormal);
		payload.bounceDir = normalize(reflected + random_in_unit_sphere(payload.seed) * material.fuzz);
	}
}

[shader("miss")]
void missRay(inout RayPayload payload)
{
	float2 launchIdx = DispatchRaysIndex().xy;
	float2 launchDim = DispatchRaysDimensions().xy;

	float2 uv = launchIdx / launchDim;
	uv.y = 1 - uv.y;

	payload.radiance = (1.0 - uv.y) * float3(1.0, 1.0, 1.0) + uv.y * float3(0.5, 0.7, 1.0);
	payload.rayDepth = maxPathLength;
}