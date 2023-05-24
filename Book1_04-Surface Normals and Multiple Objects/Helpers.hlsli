static const float Pi = 3.141592654f;
static const float Pi2 = 6.283185307f;
static const float Pi_2 = 1.570796327f;
static const float Pi_4 = 0.7853981635f;
static const float InvPi = 0.318309886f;
static const float InvPi2 = 0.159154943f;

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

float rand(inout uint seed)
{
	seed = (1664525u * seed + 1013904223u);
	return ((float)(seed & 0x00FFFFFF) / (float)0x01000000);
}

