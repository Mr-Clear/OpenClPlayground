
uint rnd(uint seed)
{
    seed ^= 0xA3C59AC3u;
    seed *= 0x9E3779B9u;
    seed ^= seed >> 16;
    seed *= 0x9E3779B9u;
    seed ^= seed >> 16;
    seed *= 0x9E3779B9u;
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
