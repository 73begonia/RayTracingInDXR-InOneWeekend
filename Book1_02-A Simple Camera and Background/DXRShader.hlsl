RWBuffer<float4> tracerOutBuffer : register(u0);

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

[shader("raygeneration")]
void rayGen()
{
	float2 launchIdx = DispatchRaysIndex().xy;
	float2 launchDim = DispatchRaysDimensions().xy;
	uint bufferOffset = launchDim.x * launchIdx.y + launchIdx.x;

	float2 uv = launchIdx / launchDim;
	uv.y = 1 - uv.y;

	float3 color = (1.0 - uv.y) * float3(1.0, 1.0, 1.0) + uv.y * float3(0.5, 0.7, 1.0);

	tracerOutBuffer[bufferOffset] = float4(color, 1.0);
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
}

[shader("miss")]
void missRay(inout RayPayload payload)
{
}