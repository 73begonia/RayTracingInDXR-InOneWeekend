#include "Helpers.hlsli"

RaytracingAccelerationStructure scene : register(t0, space100);
RWBuffer<float4> tracerOutBuffer : register(u0);

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texCoord;
};

struct Material
{
	float3 emissive;
	float3 baseColor;
	float subsurface;
	float metallic;
	float specular;
	float specularTint;
	float roughness;
	float anisotropic;
	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;
	float IOR;
	float transmission;
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

	float2 uv = ((launchIdx / launchDim) * 2.f - 1.f);
	uv.y = -uv.y;

	RayDesc ray;
	ray.Origin = cameraPos;

	float4 world = mul(float4(uv, 1.0f, 1.0f), invViewProj);
	ray.Direction = normalize(world).xyz;

	ray.TMin = 0;
	ray.TMax = 100000;

	RayPayload payload;
	payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
	TraceRay(scene, 0, 0xFF, 0, 0, 0, ray, payload);
	float4 col = payload.color;

	tracerOutBuffer[bufferOffset] = col;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 hitNormal;
	computeNormal(hitNormal, attribs);

	float4 phongColor = CalculatePhongLighting(primitiveAlbedo, hitNormal, diffuseCoef, specularCoef, specularPower);

	payload.color = float4(hitNormal, 1.0f);
}

[shader("closesthit")]
void closestHitGlass(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
}

[shader("miss")]
void missRay(inout RayPayload payload)
{
	payload.color = float4(0.4, 0.6, 0.2, 1.0);
}