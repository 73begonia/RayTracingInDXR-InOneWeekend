uint getNewSeed(uint param1, uint param2, uint numPermutation)
{
	uint s0 = 0;
	uint v0 = param1;
	uint v1 = param2;

	for (uint perm = 0; perm < numPermutation; perm++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

//[0, 1]
float rand(inout uint seed)
{
	seed = (1664525u * seed + 1013904223u);
	return ((float)(seed & 0x00FFFFFF) / (float)0x01000000);
}

//[min, max]
float rand(inout uint seed, float min, float max)
{
	return (rand(seed) * (max - min)) + min;
}

float3 random_in_unit_sphere(inout uint seed)
{
	float3 p;
	do 
	{
		p = float3
			(
				rand(seed, -1, 1),
				rand(seed, -1, 1),
				rand(seed, -1, 1)
			);
	} while (dot(p, p) >= 1.0);
	return p;
}

float3 random_unit_vector(inout uint seed)
{
	float3 v = random_in_unit_sphere(seed);
	return normalize(v);
}

float3 random_in_hemisphere(inout uint seed, float3 normal)
{
	float3 v = random_in_unit_sphere(seed);
	if (dot(v, normal) < 0.0)
	{
		v = -v;
	}
	return v;
}

float3 refract(float3 uv, float3 n, float etai_over_etat)
{
	float cos_theta = min(dot(-uv, n), 1.f);
	float3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
	float3 r_out_parallel = -sqrt(abs(1.f - dot(r_out_perp, r_out_perp))) * n;
	return r_out_perp + r_out_parallel;
}

float reflectance(float cosine, float ref_idx)
{
	float r0 = (1 - ref_idx) / (1.f + ref_idx);
	r0 = r0 * r0;
	return r0 + (1.f - r0) * pow((1.f - cosine), 5.f);
}