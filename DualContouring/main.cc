#include "main.h"
#include "octree.h"
#include "instance.h"
#include "../worley.h"
#include "noises.h"

// namespace ChunkMesh
// {
//     OctreeNode *chunkRoot;
//     OctreeNode *chunkWithLod;
//     std::vector<OctreeNode *> neighbouringChunks;
//     OctreeNode *seamRoot;
// };

namespace DualContouring
{
    // chunk settings
    int chunkSize = 16;
    Noises *noises = nullptr;

    // storing the octrees that we would delete after mesh construction
    // std::vector<OctreeNode *> neighbourNodes;

    // storing the octree roots here for search
    // std::unordered_map<uint64_t, OctreeNode *> chunksListHashMap;

    void initialize(int newChunkSize, int seed)
    {
        chunkSize = newChunkSize;

        noises = new Noises(seed);
    }

    DCInstance *createInstance() {
        DCInstance *instance = new DCInstance();
        return instance;
    }
    void destroyInstance(DCInstance *instance) {
        delete instance;
    }

    // biomes
    float getComputedBiomeHeight(unsigned char b, const vm::vec2 &worldPosition, const int &lod) {
        const Biome &biome = BIOMES[b];
        float ax = worldPosition.x;
        float az = worldPosition.y;

        float biomeHeight = biome.baseHeight +
            DualContouring::noises->elevationNoise1.in2D(ax * biome.amps[0][0], az * biome.amps[0][0]) * biome.amps[0][1] +
            DualContouring::noises->elevationNoise2.in2D(ax * biome.amps[1][0], az * biome.amps[1][0]) * biome.amps[1][1] +
            DualContouring::noises->elevationNoise3.in2D(ax * biome.amps[2][0], az * biome.amps[2][0]) * biome.amps[2][1];
        return biomeHeight;
    }

    // caves
    float getComputedCaveNoise(int ax, int ay, int az) {
        std::vector<double> at = {
            (double)ax * 0.1f,
            (double)ay * 0.1f,
            (double)az * 0.1f
        };
        const size_t max_order = 3;
        std::vector<double> F;
        F.resize(max_order + 1);
        std::vector<dvec3> delta;
        delta.resize(max_order + 1);
        std::vector<uint32_t> ID;
        ID.resize(max_order + 1);
        // std::cout << "delete 0 " << delta.size() << std::endl;
        Worley(at, max_order, F, delta, ID);
        // std::cout << "delete 1" << std::endl;
        // delete[] F;
        // std::cout << "delete 2" << std::endl;
        // delete[] delta;
        // std::cout << "delete 3 " << delta[0].x << std::endl;

        vm::vec3 deltaPoint1(
            delta[0].x,
            delta[0].y,
            delta[0].z
        );
        float distance1 = length(deltaPoint1);
        
        // std::cout << "delete 3" << std::endl;

        vm::vec3 deltaPoint3(
            delta[2].x,
            delta[2].y,
            delta[2].z
        );
        // std::cout << "delete 4" << std::endl;
        float distance3 = length(deltaPoint3);
        float caveValue = std::min(std::max((distance3 != 0.f ? (distance1 / distance3) : 0.f) * 1.1f, 0.f), 1.f);
        // std::cout << "return" << std::endl;
        return caveValue;
    }
}