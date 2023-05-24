#pragma once
#include "basic_math.h"

//Mesh
struct Vertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

typedef uint3 Tridex;

struct Mesh
{
	vector<Vertex> vtxArr;
	vector<Tridex> tdxArr;
};

Mesh loadMeshFromOBJFile(const char* filename, bool optimizeVertexCount);

//generateMesh
enum FaceDir { unknown = -1, down, up, front, back, left, right };

Mesh generateParallelogramMesh(const float3& corner, const float3& side1, const float3& side2);
Mesh generateRectangleMesh(const float3& center, const float3& size, FaceDir dir);
Mesh generateBoxMesh(const float3& lowerCorner, const float3& upperCorner);
Mesh generateCubeMesh(const float3& center, const float3& size, bool bottomCenter = false);
Mesh generateSphereMesh(const float3& center, float radius, uint numSegmentsInMeridian = 50, uint numSegmentsInEquator = 100);