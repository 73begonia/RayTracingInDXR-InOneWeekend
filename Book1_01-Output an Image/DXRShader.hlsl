RWBuffer<float4> tracerOutBuffer : register(u0);

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

	float3 color = float3(uv, 0.25);

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