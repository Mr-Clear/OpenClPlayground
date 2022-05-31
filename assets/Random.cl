
uint rnd(uint seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    return seed;
}

float rndUniformF(int seed, float min, float max)
{
    return (float)rnd(seed) / (float)UINT_MAX * (max - min) + min;
}

float rndNormalF(int seed, float mu, float sigma)
{
    const float u1 = rndUniformF(seed, FLT_EPSILON, 1);
    const float u2 = rndUniformF(seed, 0, 1);
    const float magnitude = sigma * sqrt(-2.0f * log(u1));
    return magnitude * cos(2.0 * M_PI * u2) + mu;
}
