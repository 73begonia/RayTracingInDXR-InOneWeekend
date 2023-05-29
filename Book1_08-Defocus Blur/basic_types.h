#pragma once

typedef wchar_t wchar;
typedef unsigned char uint8;
typedef unsigned int uint;
typedef unsigned long long uint64;

struct float2
{
	union
	{
		float data[2];
		struct { float x, y; };
	};

	float2() {}
	template<typename X>
	float2(X x) : x((float)x), y((float)x) {}
	template<typename X, typename Y>
	float2(X x, Y y) : x((float)x), y((float)y) {}
	float& operator[](int order) { return data[order]; }
	const float operator[](int order) const { return data[order]; }
};

struct float3
{
	union
	{
		float data[3];
		struct
		{
			union
			{
				struct { float x, y; };
				float2 xy;
			};
			float z;
		};
	};

	float3() {}
	template<typename X>
	float3(X x) : x((float)x), y((float)x), z((float)x) {}
	template<typename X, typename Y, typename Z>
	float3(X x, Y y, Z z) : x((float)x), y((float)y), z((float)z) {}
	float3(const float2& xy, float z) : xy(xy), z(z) {}
	float& operator[](int order) { return data[order]; }
	const float operator[](int order) const { return data[order]; }
};

struct float4
{
	union
	{
		float data[4];
		struct
		{
			union
			{
				struct { float x, y, z; };
				float3 xyz;
			};
			float w;
		};
	};

	float4() {}
	template<typename X>
	float4(X x) : x((float)x), y((float)x), z((float)x), w((float)x) {}
	template<typename X, typename Y, typename Z, typename W>
	float4(X x, Y y, Z z, W w) : x((float)x), y((float)y), z((float)z), w((float)w) {}
	float4(const float3& xyz, float w) : xyz(xyz), w(w) {}
	float& operator[](int order) { return data[order]; }
	const float operator[](int order) const { return data[order]; }
};

struct uint2;
struct int2
{
	union
	{
		int data[2];
		struct { int x, y; };
		struct { int i, j; };
	};

	int2() {}
	template<typename I, typename J>
	int2(I i, J j) : i((int)i), j((int)j) {}
	int& operator[](int order) { return data[order]; }
	const int operator[](int order) const { return data[order]; }
	explicit operator uint2();
};

struct uint2
{
	union
	{
		uint data[2];
		struct { uint x, y; };
		struct { uint i, j; };
	};

	uint2() {}
	template<typename I, typename J>
	uint2(I i, J j) : i((uint)i), j((uint)j) {}
	uint& operator[](int order) { return data[order]; }
	const uint operator[](int order) const { return data[order]; }
	explicit operator int2();
};

inline int2::operator uint2()
{
	return uint2((uint)i, (uint)j);
}

inline uint2::operator int2()
{
	return int2((int)i, (int)j);
}

struct uint3
{
	union
	{
		uint data[3];
		struct { uint x, y, z; };
		struct { uint i, j, k; };
	};

	uint3() {}
	template<typename I, typename J, typename K>
	uint3(I i, J j, K k) : i((uint)i), j((uint)j), k((uint)k) {}
	uint& operator[](int order) { return data[order]; }
	const uint operator[](int order) const { return data[order]; }
};

struct Transform
{
	float mat[4][4];
	Transform() {}
	constexpr Transform(float scala) : mat{}
	{
		mat[0][0] = mat[1][1] = mat[2][2] = scala;
		mat[3][3] = 1.0f;
	}
	static constexpr Transform identity()
	{
		return Transform(1.0f);
	}
};

