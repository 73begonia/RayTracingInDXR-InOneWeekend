RaytracingAccelerationStructure scene : register(t0, space100);
RWBuffer<float4> tracerOutBuffer : register(u0);

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texCoord;
};

StructuredBuffer<Vertex> vertexBuffer		  : register(t0);
Buffer<uint3> tridexBuffer					  : register(t1);

cbuffer GLOBAL_CONSTANTS : register(b0)
{
	float3 backgroundLight;
	float3 cameraPos;
	float4x4 invViewProj;
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

	tracerOutBuffer[bufferOffset] = payload.color;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float4(1.0, 0.0, 0.0, 1.0);
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