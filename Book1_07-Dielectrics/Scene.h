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

struct GPUMesh
{
	uint numVertices;
	D3D12_GPU_VIRTUAL_ADDRESS vertexBufferVA;
	uint numTridices;
	D3D12_GPU_VIRTUAL_ADDRESS tridexBufferVA;
};

Mesh loadMeshFromOBJFile(const char* filename, bool optimizeVertexCount);

//generateMesh
enum FaceDir { unknown = -1, down, up, front, back, left, right };

Mesh generateParallelogramMesh(const float3& corner, const float3& side1, const float3& side2);
Mesh generateRectangleMesh(const float3& center, const float3& size, FaceDir dir);
Mesh generateBoxMesh(const float3& lowerCorner, const float3& upperCorner);
Mesh generateCubeMesh(const float3& center, const float3& size, bool bottomCenter = false);
Mesh generateSphereMesh(const float3& center, float radius, uint numSegmentsInMeridian = 180, uint numSegmentsInEquator = 100);

//Material
namespace MaterialType
{
	enum Type
	{
		Lambertian,
		Metal,
		Dielectric,

		Count
	};
}

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
	
	MaterialType::Type type;
};

//Scene
struct GPUSceneObject
{
	uint vertexOffset;
	uint tridexOffset;
	uint materialIdx;

	Transform modelMatrix;
};

struct SceneObject
{
	uint vertexOffset;
	uint tridexOffset;
	uint numVertices;
	uint numTridices;

	uint materialIdx = uint(-1);
	float3 translation = float3(0.0f);
	float4 rotation = float4(0.f, 0.f, 0.f, 1.f);
	float scale = 1.0f;
	Transform modelMatrix = Transform::identity();
};

class Scene
{
	vector<SceneObject> objArr;
	vector<Vertex> vtxArr;
	vector<Tridex> tdxArr;
	vector<Material> mtlArr;

	friend class SceneLoader;

public:
	void clear()
	{
		objArr.clear();
		vtxArr.clear();
		tdxArr.clear();
		mtlArr.clear();
	}

	const vector<Vertex>& getVertexArray() const { return vtxArr; }
	const vector<Tridex>& getTridexArray() const { return tdxArr; }
	const vector<Material>& getMaterialArray() const { return mtlArr; }
	const SceneObject& getObject(uint i) const { return objArr[i]; }
	uint numObjects() const { return (uint)objArr.size(); }
};

class SceneLoader
{
	vector<Scene*> sceneArr;

	void initializeGeometryFromMeshes(Scene* scene, const vector<Mesh*>& meshes);
	void computeModelMatrices(Scene* scene);

public:
	Scene* getScene(uint sceneIdx) const { return sceneArr[sceneIdx]; }
	Scene* push_simpleSphere();
};