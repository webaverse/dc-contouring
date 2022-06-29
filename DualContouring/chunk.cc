#include "main.h"
#include "vectorMath.h"
// #include "../FastNoise.h"
#include "../hash.h"
#include "../util.h"
#include "../vector.h"
#include "../worley.h"
#include "biomes.h"
#include "chunk.h"
#include "density.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>
#include <deque>

//

/* NoiseField::NoiseField() {}
NoiseField::NoiseField(int size) : // size(size),
                                   temperature(size * size),
                                   humidity(size * size),
                                   ocean(size * size),
                                   river(size * size)
{
}
NoiseField::NoiseField(int size, std::vector<float> &&temperature, std::vector<float> &&humidity, std::vector<float> &&ocean, std::vector<float> &&river) : // size(size),
                                                                                                                                                            temperature(std::move(temperature)),
                                                                                                                                                            humidity(std::move(humidity)),
                                                                                                                                                            ocean(std::move(ocean)),
                                                                                                                                                            river(std::move(river))
{
}
NoiseField::NoiseField(const NoiseField &&other) : // size(other.size),
                                                   temperature(std::move(other.temperature)),
                                                   humidity(std::move(other.humidity)),
                                                   ocean(std::move(other.ocean)),
                                                   river(std::move(other.river))
{
} */
/* size_t NoiseField::size() const
{
    return temperature.size() + humidity.size() + ocean.size() + river.size();
} */

/* NoiseField &NoiseField::operator=(const NoiseField &&other) {
    temperature = std::move(other.temperature);
    humidity = std::move(other.humidity);
    ocean = std::move(other.ocean);
    river = std::move(other.river);
    return *this;
}
Heightfield &Heightfield::operator=(const Heightfield &&other) {
    heightField = std::move(other.heightField);
    biomesVectorField = std::move(other.biomesVectorField);
    biomesWeightsVectorField = std::move(other.biomesWeightsVectorField);
    return *this;
} */

//

int resolveGenerateFlags(int flags) {
    if (flags & GenerateFlags::GF_AOFIELD)
    {
        flags |= (GenerateFlags)GenerateFlags::GF_SDF;
    }
    if (flags & GenerateFlags::GF_SDF) {
        flags |= (GenerateFlags)GenerateFlags::GF_HEIGHTFIELD;
    }
    if (flags & GenerateFlags::GF_LIQUIDS) {
        flags |= (GenerateFlags)GenerateFlags::GF_WATERFIELD;
    }
    if ((flags & GenerateFlags::GF_HEIGHTFIELD) | (flags & GenerateFlags::GF_WATERFIELD)) {
        flags |= (GenerateFlags)GenerateFlags::GF_BIOMES;
    }
    if (flags & GenerateFlags::GF_BIOMES)
    {
        flags |= (GenerateFlags)GenerateFlags::GF_NOISE;
    }
    return flags;
}

//

/* Chunk2D::Chunk2D(Chunk2D &&other) :
                              min(other.min),
                              size(other.size),
                              lod(other.lod),
                              gridPoints(other.gridPoints),
                              cachedNoiseField(std::move(other.cachedNoiseField)),
                              cachedBiomesField(std::move(other.cachedBiomesField)),
                            //   cachedBiomesVectorField(std::move(other.cachedBiomesVectorField)),
                            //   cachedBiomesWeightsVectorField(std::move(other.cachedBiomesWeightsVectorField)),
                              cachedWaterField(std::move(other.cachedWaterField)),
                              cachedHeightField(std::move(other.cachedHeightField))
{
} */
Chunk2D::Chunk2D(const vm::ivec2 chunkMin, const int lod) :
                                                         min(chunkMin),
                                                         //  size(DualContouring::chunkSize),
                                                         lod(lod)
                                                         //  gridPoints(size + 3 + lod)
                                                         {}
/* Chunk2D &Chunk2D::operator=(const Chunk2D &&other)
{
    min = other.min;
    lod = other.lod;
    cachedNoiseField = std::move(other.cachedNoiseField);
    cachedBiomesField = std::move(other.cachedBiomesField);
    cachedWaterField = std::move(other.cachedWaterField);
    cachedHeightField = std::move(other.cachedHeightField);
    return *this;
} */

void Chunk2D::generate(DCInstance *inst, int flags)
{
    // dependencies
    flags = resolveGenerateFlags(flags);

    // generate
    if (flags & GenerateFlags::GF_NOISE) {
        cachedNoiseField.ensure(inst, this);
    }
    if (flags & GenerateFlags::GF_BIOMES) {
        cachedBiomesField.ensure(inst, this);
    }
    if (flags & GenerateFlags::GF_HEIGHTFIELD) {
        cachedHeightField.ensure(inst, this);
    }
    if (flags & GenerateFlags::GF_WATERFIELD) {
        cachedWaterField.ensure(inst, this);
    }
}
NoiseField Chunk2D::initNoiseField(DCInstance *inst, Chunk2D *chunk) {
    const int &size = DualContouring::chunkSize;
    const vm::ivec2 &min = chunk->min;
    const int &lod = chunk->lod;
    
    NoiseField noiseField;
    noiseField.temperature.resize(size * size);
    noiseField.humidity.resize(size * size);
    noiseField.ocean.resize(size * size);
    noiseField.river.resize(size * size);
    for (int z = 0; z < size; z++)
    {
        for (int x = 0; x < size; x++)
        {
            int index = x + z * size;
            int ax = (x * lod) + min.x;
            int az = (z * lod) + min.y;

            float tNoise = (float)DualContouring::noises->temperatureNoise.in2D(ax, az);
            noiseField.temperature[index] = tNoise;

            float hNoise = (float)DualContouring::noises->humidityNoise.in2D(ax, az);
            noiseField.humidity[index] = hNoise;

            float oNoise = (float)DualContouring::noises->oceanNoise.in2D(ax, az);
            noiseField.ocean[index] = oNoise;

            float rNoise = (float)DualContouring::noises->riverNoise.in2D(ax, az);
            noiseField.river[index] = rNoise;
        }
    }

    return noiseField;
}
std::vector<uint8_t> Chunk2D::initBiomesField(DCInstance *inst, Chunk2D *chunk) {
    const int &size = DualContouring::chunkSize;
    const auto &cachedNoiseField = chunk->cachedNoiseField;
    
    std::vector<uint8_t> biomesField(size * size);
    for (int z = 0; z < size; z++)
    {
        for (int x = 0; x < size; x++)
        {
            int index = x + z * size;
            unsigned char &biome = biomesField[index];
            biome = 0xFF;

            float temperatureNoise = cachedNoiseField.value.temperature[index];
            float humidityNoise = cachedNoiseField.value.humidity[index];
            float oceanNoise = cachedNoiseField.value.ocean[index];
            float riverNoise = cachedNoiseField.value.river[index];

            if (oceanNoise < (80.0f / 255.0f))
            {
                biome = (unsigned char)BIOME::biOcean;
            }
            if (biome == 0xFF)
            {
                const float range = 0.022f;
                if (riverNoise > 0.5f - range && riverNoise < 0.5f + range)
                {
                    biome = (unsigned char)BIOME::biRiver;
                }
            }
            if (std::pow(temperatureNoise, 1.3f) < ((4.0f * 16.0f) / 255.0f))
            {
                if (biome == (unsigned char)BIOME::biOcean)
                {
                    biome = (unsigned char)BIOME::biFrozenOcean;
                }
                else if (biome == (unsigned char)BIOME::biRiver)
                {
                    biome = (unsigned char)BIOME::biFrozenRiver;
                }
            }
            if (biome == 0xFF)
            {
                float temperatureNoise2 = vm::clamp(std::pow(temperatureNoise, 1.3f), 0.f, 1.f);
                float humidityNoise2 = vm::clamp(std::pow(humidityNoise, 1.3f), 0.f, 1.f);

                int t = (int)std::floor(temperatureNoise2 * 16.0f);
                int h = (int)std::floor(humidityNoise2 * 16.0f);
                biome = (unsigned char)BIOMES_TEMPERATURE_HUMIDITY[t + 16 * h];
            }
        }
    }
    return biomesField;
}
Heightfield Chunk2D::initHeightField(DCInstance *inst, Chunk2D *chunk) {
    const int &size = DualContouring::chunkSize;
    const int &gridPoints = DualContouring::gridPoints;
    const int &lod = chunk->lod;
    const vm::ivec2 &min = chunk->min;
    
    Heightfield heightfield;
    heightfield.biomesVectorField.resize(gridPoints * gridPoints * 4);
    heightfield.biomesWeightsVectorField.resize(gridPoints * gridPoints * 4);
    heightfield.heightField.resize(gridPoints * gridPoints);
    for (int z = 0; z < gridPoints; z++)
    {
        for (int x = 0; x < gridPoints; x++)
        {
            int index2D = x + z * gridPoints;
            int ax = (x - 1) * lod + min.x;
            int az = (z - 1) * lod + min.y;
            
            // int lx = x - 1;
            // int lz = z - 1;
            // int index2D2 = lx + lz * size;
            // bool isInRange = lx >= 0 && lx < size && lz >= 0 && lz < size;

            std::unordered_map<unsigned char, unsigned int> biomeCounts(numBiomes);
            int numSamples = 0;
            for (int dz = -size/2; dz < size/2; dz++)
            {
                for (int dx = -size/2; dx < size/2; dx++)
                {
                    vm::vec2 worldPosition(ax + dx, az + dz);
                    unsigned char b = inst->getBiome(worldPosition, lod);

                    biomeCounts[b]++;
                    numSamples++;
                }
            }

            std::vector<unsigned char> biomes;
            biomes.resize(biomeCounts.size());
            int index = 0;
            for (auto kv : biomeCounts)
            {
                biomes[index++] = kv.first;
            }
            // sort by increasing occurence count of the biome
            std::sort(biomes.begin(), biomes.end(), [&biomeCounts](unsigned char b1, unsigned char b2)
                      { return biomeCounts[b1] > biomeCounts[b2]; });

            for (size_t i = 0; i < 4; i++)
            {
                if (i < biomes.size())
                {
                    heightfield.biomesVectorField[index2D * 4 + i] = biomes[i];
                    heightfield.biomesWeightsVectorField[index2D * 4 + i] = (float)biomeCounts[biomes[i]] / (float)numSamples;
                }
                else
                {
                    heightfield.biomesVectorField[index2D * 4 + i] = 0;
                    heightfield.biomesWeightsVectorField[index2D * 4 + i] = 0;
                }
            }

            float elevationSum = 0.f;
            vm::vec2 fWorldPosition(ax, az);
            for (auto const &iter : biomeCounts)
            {
                elevationSum += iter.second * DualContouring::getComputedBiomeHeight(iter.first, fWorldPosition, lod);
            }

            float elevation = elevationSum / (float)numSamples;
            heightfield.heightField[index2D] = elevation;
        }
    }
    return heightfield;
}
std::vector<float> Chunk2D::initWaterField(DCInstance *inst, Chunk2D *chunk) {
    const int &size = DualContouring::chunkSize;
    const int &gridPoints = DualContouring::gridPoints;
    const vm::ivec2 &min = chunk->min;
    const int &lod = chunk->lod;

    std::vector<float> waterField(gridPoints * gridPoints, 0);
    for (int z = 0; z < gridPoints; z++)
    {
        for (int x = 0; x < gridPoints; x++)
        {
            int ax = min.x + (x - 1) * lod;
            int az = min.y + (z - 1) * lod;

            int index2D = x + z * gridPoints;
            
            // std::unordered_map<unsigned char, unsigned int> biomeCounts(numBiomes);
            // int numSamples = 0;
            for (int dz = -size/2; dz < size/2; dz++)
            {
                for (int dx = -size/2; dx < size/2; dx++)
                {
                    vm::vec2 worldPosition(ax + dx, az + dz);
                    unsigned char b = inst->getBiome(worldPosition, lod);

                    if (isWaterBiome(b)) {
                        waterField[index2D]++;
                    }
                }
            }
        }
    }
    return waterField;
}

// noises
float Chunk2D::getTemperatureLocal(const int lx, const int lz) const {
    const int &size = DualContouring::chunkSize;
    int index = lx + lz * size;
    return cachedNoiseField.value.temperature[index];
}
float Chunk2D::getHumidityLocal(const int lx, const int lz) const {
    const int &size = DualContouring::chunkSize;
    int index = lx + lz * size;
    return cachedNoiseField.value.humidity[index];
}
/* float Chunk2D::getWaterFieldLocal(const int lx, const int lz) const
{
    int index = lx + lz * size;
    return cachedWaterField[index];
} */

// biomes
unsigned char Chunk2D::getCachedBiome(const int lx, const int lz) const {
    const int &size = DualContouring::chunkSize;
    int index = lx + lz * size;
    return cachedBiomesField.value[index];
}
void Chunk2D::getCachedInterpolatedBiome2D(const vm::vec2 &worldPosition, vm::ivec4 &biome, vm::vec4 &biomeWeights) const {
    const int &gridPoints = DualContouring::gridPoints;
    
    const float &x = worldPosition.x;
    const float &z = worldPosition.y;
    int lx = int(x) - min.x + 1;
    int lz = int(z) - min.y + 1;
    int index2D = lx + lz * gridPoints;

    biome.x = cachedHeightField.value.biomesVectorField[index2D * 4];
    biome.y = cachedHeightField.value.biomesVectorField[index2D * 4 + 1];
    biome.z = cachedHeightField.value.biomesVectorField[index2D * 4 + 2];
    biome.w = cachedHeightField.value.biomesVectorField[index2D * 4 + 3];

    biomeWeights.x = cachedHeightField.value.biomesWeightsVectorField[index2D * 4];
    biomeWeights.y = cachedHeightField.value.biomesWeightsVectorField[index2D * 4 + 1];
    biomeWeights.z = cachedHeightField.value.biomesWeightsVectorField[index2D * 4 + 2];
    biomeWeights.w = cachedHeightField.value.biomesWeightsVectorField[index2D * 4 + 3];
}

//

/* Chunk3D::Chunk3D(Chunk3D &&other) :
                              min(other.min),
                              size(other.size),
                              lod(other.lod),
                              gridPoints(other.gridPoints),
                              cachedSkylightField(std::move(other.cachedSkylightField)),
                              cachedAoField(std::move(other.cachedAoField)),
                              cachedSdf(std::move(other.cachedSdf)),
                              cachedWaterSdf(std::move(other.cachedWaterSdf)),
                              cachedDamageSdf(std::move(other.cachedDamageSdf)),
                              chunk2d(other.chunk2d)
{
} */
Chunk3D::Chunk3D(const vm::ivec3 chunkMin, const int lod, Chunk2D *chunk2d) :
                                                         min(chunkMin),
                                                        //  size(DualContouring::chunkSize),
                                                         lod(lod),
                                                        //  gridPoints(size + 3 + lod),
                                                         chunk2d(chunk2d)
                                                         {}
/* Chunk3D &Chunk3D::operator=(const Chunk3D &&other)
{
    min = other.min;
    lod = other.lod;
    chunk2d = other.chunk2d;
    cachedSkylightField = std::move(other.cachedSkylightField);
    cachedAoField = std::move(other.cachedAoField);
    cachedSdf = std::move(other.cachedSdf);
    cachedWaterSdf = std::move(other.cachedWaterSdf);
    cachedDamageSdf = std::move(other.cachedDamageSdf);
    return *this;
} */

void Chunk3D::generate(DCInstance *inst, int flags)
{
    chunk2d->generate(inst, flags);

    // dependencies
    flags = resolveGenerateFlags(flags);

    // generate
    if (flags & GenerateFlags::GF_SDF) {
        cachedSdf.ensure(inst, this);
        cachedDamageSdf.ensure(inst, this);
    }
    if (flags & GenerateFlags::GF_LIQUIDS) {
        /* EM_ASM({
            console.log('ensure cachedWaterSdf A', $0, $1, $2, $3);
        }, min.x, min.y, min.z, lod); */
        cachedWaterSdf.ensure(inst, this);
        /* EM_ASM({
            console.log('ensure cachedWaterSdf B', $0, $1, $2, $3, $4);
        }, min.x, min.y, min.z, lod, cachedWaterSdf.value.size()); */
    }
    if (flags & GenerateFlags::GF_AOFIELD) {
        cachedSkylightField.ensure(inst, this);
        cachedAoField.ensure(inst, this);
    }
}
std::vector<uint8_t> Chunk3D::initSkylightField(DCInstance *inst, Chunk3D *chunk) {
    const int &gridPoints = DualContouring::gridPoints;
    const vm::ivec3 &min = chunk->min;
    auto &chunk2d = chunk->chunk2d;
    auto &cachedSdf = chunk->cachedSdf;

    constexpr float maxSkyLight = 8.f;
    std::vector<uint8_t> skylightField(gridPoints * gridPoints * gridPoints, maxSkyLight);
    for (int z = 0; z < gridPoints; z++)
    {
        // int lz = z + 1;

        for (int x = 0; x < gridPoints; x++)
        {
            // int lx = x + 1;

            int index2D = x + z * gridPoints;
            float height = chunk2d->cachedHeightField.value.heightField[index2D];
            
            int topAY = min.y + gridPoints - 1;
            float skylight = std::min(std::max((float)topAY - height + maxSkyLight, 0.f), maxSkyLight);

            for (int y = gridPoints - 1; y >= 0; y--)
            {
                // int ly = y + 1;

                int sdfIndex = x + z * gridPoints + y * gridPoints * gridPoints;
                if (cachedSdf.value[sdfIndex] < 0.f)
                {
                    skylight = std::min(std::max(skylight - 1.f, 0.f), maxSkyLight);
                }

                int skylightIndex = x + z * gridPoints + y * gridPoints * gridPoints;
                skylightField[skylightIndex] = skylight;
            }
        }
    }

    // XXX should flood fill the light

    /* for (int x = 0; x < gridPoints; x++)
    {
        for (int z = 0; z < gridPoints; z++)
        {
            // int lz = z + 1;
            // int lx = x + 1;

            // for (int y = gridPoints - 1; y >= 0; y--) {
            for (int y = 0; y < gridPoints; y++)
            {
                // int ly = y + 1;

                int skylightIndex = x + z * gridPoints + y * gridPoints * gridPoints;
                float maxNeighborSkylight = cachedSkylightField[skylightIndex];
                for (int dz = -1; dz <= 1; dz += 2)
                {
                    for (int dx = -1; dx <= 1; dx += 2)
                    {
                        for (int dy = -1; dy <= 1; dy += 2)
                        {
                            int lx = x + dx;
                            int ly = y + dy;
                            int lz = z + dz;

                            float deltaRadius = std::sqrt(dx * dx + dy * dy + dz * dz);

                            if (lx >= 0 && lx < gridPoints && ly >= 0 && ly < gridPoints && lz >= 0 && lz < gridPoints)
                            {
                                int neighborIndex = lx + lz * gridPoints + ly * gridPoints * gridPoints;
                                float skylight = cachedSkylightField[neighborIndex];
                                maxNeighborSkylight = std::max(maxNeighborSkylight, skylight - deltaRadius);
                            }
                        }
                    }
                }

                cachedSkylightField[skylightIndex] = maxNeighborSkylight;
            }
        }
    } */
    return skylightField;
}
std::vector<uint8_t> Chunk3D::initAoField(DCInstance *inst, Chunk3D *chunk) {
    const int &size = DualContouring::chunkSize;
    const int &gridPoints = DualContouring::gridPoints;
    auto &cachedSdf = chunk->cachedSdf;
    
    std::vector<uint8_t> aoField(size * size * size, 3 * 3 * 3);
    for (int y = 0; y < size; y++)
    {
        int ly = y + 1;

        for (int z = 0; z < size; z++)
        {
            int lz = z + 1;

            for (int x = 0; x < size; x++)
            {
                int lx = x + 1;

                unsigned char numOpens = 0;
                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            int sdfIndex = (lx + dx) + (lz + dz) * gridPoints + (ly + dy) * gridPoints * gridPoints;
                            numOpens += (unsigned char)(cachedSdf.value[sdfIndex] >= 0.f);
                        }
                    }
                }

                int aoIndex = x + z * size + y * size * size;
                aoField[aoIndex] = numOpens;
            }
        }
    }
    return aoField;
}
std::vector<float> Chunk3D::initSdf(DCInstance *inst, Chunk3D *chunk) {
    const int &gridPoints = DualContouring::gridPoints;
    auto &chunk2d = chunk->chunk2d;
    const vm::ivec3 &min = chunk->min;
    const int &lod = chunk->lod;

    std::vector<float> sdf(gridPoints * gridPoints * gridPoints, MAX_HEIGHT);
    for (int z = 0; z < gridPoints; z++)
    {
        for (int x = 0; x < gridPoints; x++)
        {
            int index2D = x + z * gridPoints;
            float height = chunk2d->cachedHeightField.value.heightField[index2D];

            for (int y = 0; y < gridPoints; y++)
            {
                int index3D = x + z * gridPoints + y * gridPoints * gridPoints;

                int ax = min.x + (x - 1) * lod;
                int ay = min.y + (y - 1) * lod;
                int az = min.z + (z - 1) * lod;

                // height
                float heightValue = (float)ay - height;
                heightValue = std::min(
                    std::max(
                        heightValue,
                        (float)-1),
                    (float)1);

                // caves
                float caveValue = DualContouring::getComputedCaveNoise(ax, ay, az) * 1.1f;
                float f = heightValue + caveValue;
                /* f = std::min( // XXX does not fix
                    std::max(
                        f,
                        -1.f
                    ),
                    1.f
                ); */

                // result
                sdf[index3D] = f;
            }
        }
    }
    return sdf;
}
std::vector<float> Chunk3D::initDamageSdf(DCInstance *inst, Chunk3D *chunk) {
    const int &gridPoints = DualContouring::gridPoints;
    std::vector<float> damageSdf(gridPoints * gridPoints * gridPoints, MAX_HEIGHT);
    return damageSdf;
}

// liquids
std::vector<float> Chunk3D::initWaterSdf(DCInstance *inst, Chunk3D *chunk) {
    /* EM_ASM({
        console.log('init water sdf');
    }); */
    const int &gridPoints = DualContouring::gridPoints;
    auto &chunk2d = chunk->chunk2d;
    const vm::ivec3 &min = chunk->min;

    const float fSize = (float)gridPoints;

    std::vector<float> waterSdf(gridPoints * gridPoints * gridPoints, MAX_HEIGHT);
    /* EM_ASM({
        console.log('water sdf set size', $0, $1, $2, $3, $4);
    }, chunk->min.x, chunk->min.y, chunk->min.z, chunk->lod, waterSdf.size()); */
    for (int z = 0; z < gridPoints; z++)
    {
        for (int x = 0; x < gridPoints; x++)
        {
            // int lx = x + 1;
            // int lz = z + 1;
            int index2D = x + z * gridPoints;
            float waterValue = -chunk2d->cachedWaterField.value[index2D] / fSize;
            // float waterValue = -inst->getWater(vm::vec2(ax, az), lod) / fSize;
            // waterValue *= -1.1f;
            for (int y = 0; y < gridPoints; y++)
            {
                int ax = min.x + (x - 1) * chunk->lod;
                int ay = min.y + (y - 1) * chunk->lod;
                int az = min.z + (z - 1) * chunk->lod;

                float heightValue = (float)ay - waterBaseHeight;
                heightValue = std::min(
                    std::max(
                        heightValue,
                        -1.f
                    ),
                    1.f
                );
                
                float value = std::max(waterValue, heightValue);

                int index3D = x + z * gridPoints + y * gridPoints * gridPoints;
                waterSdf[index3D] = value;
            }
        }
    }
    return waterSdf;
}

// biomes
void Chunk3D::getCachedInterpolatedBiome3D(const vm::vec3 &worldPosition, vm::ivec4 &biome, vm::vec4 &biomeWeights) const {
    const int &gridPoints = DualContouring::gridPoints;

    const float &x = worldPosition.x;
    const float &z = worldPosition.z;
    const float &y = worldPosition.y;
    if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
        EM_ASM({
           console.log('got nan getCachedInterpolatedBiome3D', $0, $1, $2);
        }, x, y, z);
        abort();
    }

    // XXX water field needs height field too, see below
    // XXX we will crash before that though, on this call, which also requires the heighfield cache to be populated
    // XXX we can get around this by not outputting the water biome

    chunk2d->getCachedInterpolatedBiome2D(vm::vec2(worldPosition.x, worldPosition.z), biome, biomeWeights);

    int lx = int(x) - min.x + 1;
    int ly = int(y) - min.y + 1;
    int lz = int(z) - min.z + 1;
    int heightfieldIndex = lx + lz * gridPoints;
    float heightValue = chunk2d->cachedHeightField.value.heightField[heightfieldIndex];
    int sdfIndex = lx + lz * gridPoints + ly * gridPoints * gridPoints;
    float sdfValue = cachedSdf.value[sdfIndex];

    bool neighborHeightsValid = true;
    for (int dx = -1; dx <= 1; dx += 2)
    {
        for (int dz = -1; dz <= 1; dz += 2)
        {
            int lx2 = lx + dx;
            int lz2 = lz + dz;
            int neighborHeightfieldIndex = lx2 + lz2 * gridPoints;
            float heightValue = chunk2d->cachedHeightField.value.heightField[heightfieldIndex];
            if (y + 3 > heightValue)
            {
                neighborHeightsValid = false;
                break;
            }
        }
        if (!neighborHeightsValid)
        {
            break;
        }
    }

    if (neighborHeightsValid)
    {
        if (y < heightValue - 12)
        {
            unsigned char firstBiome = (unsigned char)BIOME::teStone;
            biome.w = biome.z;
            biome.z = biome.y;
            biome.y = biome.x;
            biome.x = firstBiome;
        }
        else if (y < heightValue - 2)
        {
            unsigned char firstBiome = (unsigned char)BIOME::teDirt;
            biome.w = biome.z;
            biome.z = biome.y;
            biome.y = biome.x;
            biome.x = firstBiome;
        }
    }
}
void Chunk2D::getCachedHeightfield(float *heights) const {
    const int &size = DualContouring::chunkSize;
    const int &gridPoints = DualContouring::gridPoints;

    for (int z = 0; z < size; z++)
    {
        for (int x = 0; x < size; x++)
        {
            int index2D = x + z * size;

            int gridX = x + 1;
            int gridZ = z + 1;
            int gridIndex = gridX + gridZ * gridPoints;

            heights[index2D] = cachedHeightField.value.heightField[gridIndex];
        }
    }
}

// lighting
void Chunk3D::getCachedSkylight(unsigned char *skylights) const {
    const int &size = DualContouring::chunkSize;
    const int &gridPoints = DualContouring::gridPoints;

    for (int z = 0; z < size; z++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                int dstIndex = x + y * size + z * size * size;

                int lx = x + 1;
                int ly = y + 1;
                int lz = z + 1;
                int srcIndex = lx + lz * gridPoints + ly * gridPoints * gridPoints; // note: output is y-first, but storage is z-first

                skylights[dstIndex] = cachedSkylightField.value[srcIndex];
            }
        }
    }
}
void Chunk3D::getCachedAo(unsigned char *aos) const {
    const int &size = DualContouring::chunkSize;

    for (int z = 0; z < size; z++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                int dstIndex = x + y * size + z * size * size;
                int srcIndex = x + z * size + y * size * size; // note: output is y-first, but storage is z-first

                aos[dstIndex] = cachedAoField.value[srcIndex];
            }
        }
    }
}

// sdf
float Chunk3D::getCachedInterpolatedSdf(const float x, const float y, const float z) const {
    const int &gridPoints = DualContouring::gridPoints;
    
    const float localX = (x - min.x) / lod + 1;
    const float localY = (y - min.y) / lod + 1;
    const float localZ = (z - min.z) / lod + 1;
    return trilinear<float>(
        vm::vec3(localX, localY, localZ),
        cachedSdf.value,
        gridPoints
    );
}
float Chunk3D::getCachedWaterInterpolatedSdf(const float x, const float y, const float z) const {
    const int &gridPoints = DualContouring::gridPoints;
    
    if (cachedWaterSdf.value.size() == 0) {
        EM_ASM({
            console.log('water sdf size is 0', $0, $1, $2);
        }, x, y, z);
        abort();
    }
    if (isnan(cachedWaterSdf.value[0])) {
        EM_ASM({
            console.log('water sdf [0] is nan', $0);
        }, cachedWaterSdf.value.size());
        abort();
    }

    const float localX = (x - min.x) / lod + 1;
    const float localY = (y - min.y) / lod + 1;
    const float localZ = (z - min.z) / lod + 1;
    return trilinear<float>(
        vm::vec3(localX, localY, localZ),
        cachedWaterSdf.value,
        gridPoints
    );
}
float Chunk3D::getCachedDamageInterpolatedSdf(const float x, const float y, const float z) const {
    const int &gridPoints = DualContouring::gridPoints;

    const float localX = (x - min.x) / lod + 1;
    const float localY = (y - min.y) / lod + 1;
    const float localZ = (z - min.z) / lod + 1;
    return trilinear<float>(
        vm::vec3(localX, localY, localZ),
        cachedDamageSdf.value,
        gridPoints
    );
}

// skylight
/* unsigned char Chunk3D::getSkylightLocal(const int lx, const int ly, const int lz) const
{
    int index = lx + lz * gridPoints + ly * gridPoints * gridPoints;
    return cachedSkylightField[index];
}

// ao
unsigned char Chunk3D::getAoLocal(const int lx, const int ly, const int lz) const
{
    int index = lx + lz * size + ly * size * size;
    return cachedAoField[index];
} */

// signed distance field function for a box at the origin
// returns negative for points inside the box, zero at the box's surface, and positive for points outside the box
// sx sy sz is the size of the box. the box goes from -sx/2 to sx/2, -sy/2 to sy/2, -sz/2 to sz/2
// px py pz is the point to check
float Chunk3D::signedDistanceToBox(float sx, float sy, float sz, float px, float py, float pz)
{
    float dx = std::abs(px) - sx / 2;
    float dy = std::abs(py) - sy / 2;
    float dz = std::abs(pz) - sz / 2;
    float d = std::max(std::max(dx, dy), dz);
    return d;
}

// signed distance to sphere
// returns negative for points inside the sphere, zero at the sphere's surface, and positive for points outside the sphere
// cx, cy, cz is the center of the sphere. r is the radius. px, py, pz is the point to check
float Chunk3D::signedDistanceToSphere(float cx, float cy, float cz, float r, float px, float py, float pz)
{
    float dx = px - cx;
    float dy = py - cy;
    float dz = pz - cz;
    float d = sqrt(dx * dx + dy * dy + dz * dz);
    return d - r;
}

void Chunk3D::patchFrontier(std::vector<bool> &erased) {
    const int &gridPoints = DualContouring::gridPoints;

    std::function<void(const vm::ivec3 &)> tryIndex = [&](const vm::ivec3 &v) -> void
    {
        int index = v.x + v.y * gridPoints + v.z * gridPoints * gridPoints;
        if (erased[index])
        {
            std::vector<vm::ivec3> unerasedNeighbors;
            for (int dx = -1; dx <= 1; dx += 2)
            {
                for (int dy = -1; dy <= 1; dy += 2)
                {
                    for (int dz = -1; dz <= 1; dz += 2)
                    {
                        int ax = v.x + dx;
                        int ay = v.y + dy;
                        int az = v.z + dz;

                        int neighborIndex = ax + ay * gridPoints + az * gridPoints * gridPoints;
                        if (!erased[neighborIndex])
                        {
                            unerasedNeighbors.push_back(vm::ivec3(dx, dy, dz));
                            break;
                        }
                    }
                }
            }
            if (unerasedNeighbors.size() > 0)
            {
                // compute the current sdf min distance from the neighbors
                float minDistance = cachedDamageSdf.value[index];
                for (auto &neighborOffset : unerasedNeighbors)
                {
                    int ax = v.x + neighborOffset.x;
                    int ay = v.y + neighborOffset.y;
                    int az = v.z + neighborOffset.z;

                    int neighborIndex = ax + ay * gridPoints + az * gridPoints * gridPoints;
                    float neighborDamageSdf = cachedDamageSdf.value[neighborIndex];
                    float extraDistance = length(neighborOffset);
                    minDistance = std::min(minDistance, neighborDamageSdf + extraDistance);
                }
                cachedDamageSdf.value[index] = minDistance;
                erased[index] = false;
                unerasedNeighbors.clear();

                for (int dx = -1; dx <= 1; dx += 2)
                {
                    for (int dy = -1; dy <= 1; dy += 2)
                    {
                        for (int dz = -1; dz <= 1; dz += 2)
                        {
                            int ax = v.x + dx;
                            int ay = v.y + dy;
                            int az = v.z + dz;
                            tryIndex(vm::ivec3(ax, ay, az));
                        }
                    }
                }
            }
        }
    };

    for (int ly = 0; ly < gridPoints; ly++)
    {
        for (int lz = 0; lz < gridPoints; lz++)
        {
            for (int lx = 0; lx < gridPoints; lx++)
            {
                tryIndex(vm::ivec3(lx, ly, lz));
            }
        }
    }
}

bool Chunk3D::addSphereDamage(const float &x, const float &y, const float &z, const float radius)
{
    const int &gridPoints = DualContouring::gridPoints;

    bool drew = false;
    for (int ly = 0; ly < gridPoints; ly++)
    {
        for (int lz = 0; lz < gridPoints; lz++)
        {
            for (int lx = 0; lx < gridPoints; lx++)
            {
                int ax = min.x + lx - 1;
                int ay = min.y + ly - 1;
                int az = min.z + lz - 1;

                float newDistance = signedDistanceToSphere(x, y, z, radius, ax, ay, az);

                int index = lx + lz * gridPoints + ly * gridPoints * gridPoints;
                float oldDistance = cachedDamageSdf.value[index];

                if (newDistance < oldDistance)
                {
                    cachedDamageSdf.value[index] = newDistance;
                    drew = true;
                }
            }
        }
    }
    return drew;
}
bool Chunk3D::removeSphereDamage(const float &x, const float &y, const float &z, const float radius)
{
    const int &gridPoints = DualContouring::gridPoints;
    const int &size = DualContouring::chunkSize;

    std::vector<bool> erased(gridPoints * gridPoints * gridPoints, false);

    bool drew = false;
    for (int ly = 0; ly < gridPoints; ly++)
    {
        for (int lz = 0; lz < gridPoints; lz++)
        {
            for (int lx = 0; lx < gridPoints; lx++)
            {
                int ax = min.x + lx - 1;
                int ay = min.y + ly - 1;
                int az = min.z + lz - 1;

                float newDistance = signedDistanceToSphere(x, y, z, radius, ax, ay, az);

                int index = lx + lz * gridPoints + ly * gridPoints * gridPoints;
                float oldDistance = cachedDamageSdf.value[index];

                if (
                    newDistance <= 0.f ||      // new point is inside the sphere
                    newDistance <= oldDistance // new point affects this index
                )
                {
                    cachedDamageSdf.value[index] = (float)size; // max outside distance for a chunk
                    erased[index] = true;
                    drew = true;
                }
            }
        }
    }

    if (drew)
    {
        patchFrontier(erased);
    }

    return drew;
}

bool Chunk3D::addCubeDamage(
    const float &x, const float &y, const float &z,
    const float &qx, const float &qy, const float &qz, const float &qw,
    const float &sx, const float &sy, const float &sz)
{
    const int &gridPoints = DualContouring::gridPoints;

    Matrix m(Vec{x, y, z}, Quat{qx, qy, qz, qw}, Vec{1, 1, 1});
    Matrix mInverse = m;
    mInverse.invert();

    bool drew = true;
    for (int ly = 0; ly < gridPoints; ly++)
    {
        for (int lz = 0; lz < gridPoints; lz++)
        {
            for (int lx = 0; lx < gridPoints; lx++)
            {
                int ax = min.x + lx - 1;
                int ay = min.y + ly - 1;
                int az = min.z + lz - 1;

                Vec p = Vec(ax, ay, az).applyMatrix(mInverse);
                float newDistance = signedDistanceToBox(sx, sy, sz, p.x, p.y, p.z);

                int index = lx + lz * gridPoints + ly * gridPoints * gridPoints;
                float oldDistance = cachedDamageSdf.value[index];

                if (newDistance < oldDistance)
                {
                    cachedDamageSdf.value[index] = newDistance;
                    drew = true;
                }
            }
        }
    }
    return drew;
}
bool Chunk3D::removeCubeDamage(
    const float &x, const float &y, const float &z,
    const float &qx, const float &qy, const float &qz, const float &qw,
    const float &sx, const float &sy, const float &sz)
{
    const int &gridPoints = DualContouring::gridPoints;
    const int &size = DualContouring::chunkSize;

    Matrix m(Vec{x, y, z}, Quat{qx, qy, qz, qw}, Vec{1, 1, 1});
    Matrix mInverse = m;
    mInverse.invert();

    std::vector<bool> erased(gridPoints * gridPoints * gridPoints, false);

    bool drew = true;
    for (int ly = 0; ly < gridPoints; ly++)
    {
        for (int lz = 0; lz < gridPoints; lz++)
        {
            for (int lx = 0; lx < gridPoints; lx++)
            {
                int ax = min.x + lx - 1;
                int ay = min.y + ly - 1;
                int az = min.z + lz - 1;

                Vec p = Vec(ax, ay, az).applyMatrix(mInverse);
                float newDistance = signedDistanceToBox(sx, sy, sz, p.x, p.y, p.z);

                int index = lx + lz * gridPoints + ly * gridPoints * gridPoints;
                float oldDistance = cachedDamageSdf.value[index];

                if (newDistance <= 0.f || oldDistance >= newDistance)
                {
                    cachedDamageSdf.value[index] = (float)size;
                    erased[index] = true;
                    drew = true;
                }
            }
        }
    }

    if (drew) {
        patchFrontier(erased);
    }

    return drew;
}

/* void Chunk3D::injectDamage(float *damageBuffer)
{
    cachedDamageSdf.resize(gridPoints * gridPoints * gridPoints, MAX_HEIGHT);
    memcpy(cachedDamageSdf.data(), damageBuffer, cachedDamageSdf.size() * sizeof(float));
} */