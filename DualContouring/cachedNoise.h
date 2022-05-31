#ifndef CHACHEDNOISE_H
#define CHACHEDNOISE_H

#include "main.h"
#include "vectorMath.h"
#include "../FastNoise.h"
#include "../hash.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

class CachedNoise
{
public:
    CachedNoise() = delete;
    CachedNoise(const CachedNoise &other) = delete;
    CachedNoise(const vm::ivec3 chunkMin) : rng(100),
                                       fastNoise(rng()),
                                       min(chunkMin),
                                       size(DualContouring::chunkSize),
                                       gridPoints(size + 4)
    {
        init();
    };

    // get given world point noise, given cache
    float getInterpolated(const float &x, const float &z)
    {
        const float localX = x - min.x + 1;
        const float localZ = z - min.z + 1;
        return interpolate2D(localX, localZ);
    }
    float getRaw(const int &x, const int &z)
    {
        const int localX = x - min.x + 1;
        const int localZ = z - min.z + 1;
        const int index = localX + localZ * gridPoints;
        return (cachedNoiseField.at(index) + 1.f) / 2.f;
    }

    // private:
    int size;
    int gridPoints;
    vm::ivec3 min;
    std::vector<float> cachedNoiseField;
    std::mt19937 rng;
    FastNoise fastNoise;

    void init()
    {
        fastNoise.SetFrequency(0.02);
        fastNoise.SetFractalOctaves(4);

        cachedNoiseField.resize(gridPoints * gridPoints);

        for (int dz = 0; dz < gridPoints; dz++)
        {
            for (int dx = 0; dx < gridPoints; dx++)
            {
                cachedNoiseField[dx + dz * gridPoints] = (float)fastNoise.GetSimplexFractal(dx + min.x - 1, dz + min.z - 1);
            }
        }
    }
    float lerp(const float &a, const float &b, const float &f)
    {
        return a + f * (b - a);
    }
    float interpolate1D(const float &x, const float &z)
    {
        const int xf = std::floor(x);
        const int xc = std::ceil(x);
        const int indexF = xf + z * gridPoints;
        const int indexC = xc + z * gridPoints;
        const float dx = x - xf;
        return lerp(cachedNoiseField.at(indexF), cachedNoiseField.at(indexC), dx);
    }
    float interpolate2D(const float &x, const float &z)
    {
        const int zf = std::floor(z);
        const int zc = std::ceil(z);
        const float dz = z - zf;
        return lerp(interpolate1D(x, zf), interpolate1D(x, zc), dz);
    }
};
#endif // CHACHEDNOISE_H