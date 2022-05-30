#include "main.h"

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
    int chunkSize = 64;

    // storing the octrees that we would delete after mesh construction
    std::vector<OctreeNode *> temporaryNodesList;

    // storing the octree roots here for search
    std::unordered_map<uint64_t, OctreeNode *> chunksListHashMap;
    std::unordered_map<uint64_t, ChunkDamageBuffer> chunksDamageBufferHashMap;

    void setChunkSize(int newChunkSize) {
        chunkSize = newChunkSize;
    }
    uint8_t *constructOutputBuffer(VertexBuffer &vertexBuffer)
    {
        return vertexBuffer.getBuffer();
    }
    void generateChunkData(float x, float y, float z)
    {
        const vm::ivec3 octreeMin = vm::ivec3(x, y, z);
        CachedNoise chunkNoise(octreeMin, chunkSize);
        ChunkDamageBuffer &damageBuffer = getChunkDamageBuffer(octreeMin);
        OctreeNode *chunk = constructOctreeDownwards(octreeMin, chunkSize, chunkNoise, damageBuffer);
        if (!chunk)
        {
            return;
        }
        addChunkRootToHashMap(chunk, chunksListHashMap);
    }

    void setChunkLod(float x, float y, float z, const int lod)
    {
        const vm::ivec3 octreeMin = vm::ivec3(x, y, z);
        OctreeNode *chunkRoot = getChunkRootFromHashMap(octreeMin, chunksListHashMap);

        if (chunkRoot == nullptr)
        {
            return;
        }
        chunkRoot->lod = static_cast<LodLevel>(lod);
    }

    void clearTemporaryChunkData()
    {
        std::vector<OctreeNode *> nodesList;
        for (int i = 0; i < temporaryNodesList.size(); i++)
        {
            addNodesToVector(temporaryNodesList[i], nodesList);
        }
        // sort and remove duplicates
        std::sort(nodesList.begin(), nodesList.end());
        nodesList.erase(std::unique(nodesList.begin(), nodesList.end()), nodesList.end());
        for (int i = 0; i < nodesList.size(); i++)
        {
            const OctreeNode *node = nodesList[i];
            if (node->drawInfo)
            {
                delete node->drawInfo;
            }
            delete node;
        }
        temporaryNodesList.clear();
    }

    void clearChunkRoot(float x, float y, float z)
    {
        // we destroy the chunk root separately because we might need it for LOD switch if it's already generated
        const vm::ivec3 octreeMin = vm::ivec3(x, y, z);
        OctreeNode *chunkRoot = getChunkRootFromHashMap(octreeMin, chunksListHashMap);
        if (!chunkRoot)
        {
            return;
        }
        removeOctreeFromHashMap(octreeMin, chunksListHashMap);
        destroyOctree(chunkRoot);
    }

    uint8_t *createChunkMesh(float x, float y, float z)
    {
        // XXX the below causes a crash
        /* // filter and delete all temporary octrees for this chunk
        for (int i = 0; i < temporaryNodesList.size(); i++)
        {
            OctreeNode *node = temporaryNodesList[i];
            if (node->min.x == x && node->min.y == y && node->min.z == z)
            {
                if (node->drawInfo)
                {
                    delete node->drawInfo;
                }
                delete node;
                
                temporaryNodesList.erase(temporaryNodesList.begin() + i);
                i--;
            }
        } */

        VertexBuffer vertexBuffer;

        const vm::ivec3 octreeMin = vm::ivec3(x, y, z);
        OctreeNode *chunkRoot = getChunkRootFromHashMap(octreeMin, chunksListHashMap);
        if (!chunkRoot)
        {
            return nullptr;
        }

        OctreeNode *chunkWithLod = createChunkWithLod(chunkRoot);
        generateMeshFromOctree(chunkWithLod, false, vertexBuffer);

        // std::vector<OctreeNode *> neighbouringChunks;
        // std::vector<OctreeNode *> seamNodes = findSeamNodes(chunkWithLod, neighbouringChunks, chunksListHashMap, getChunkRootFromHashMap);
        // OctreeNode *seamRoot = constructOctreeUpwards(seamRoot, seamNodes, chunkWithLod->min, chunkWithLod->size * 2);
        // generateMeshFromOctree(seamRoot, true, vertexBuffer);

        // adding the chunk clone + neighbouring chunk clones to the destroyable list
        // for (int i = 0; i < neighbouringChunks.size(); i++)
        // {
        //     temporaryNodesList.push_back(neighbouringChunks[i]);
        // }
        // add the chunk clone octree to the destroyable list
        temporaryNodesList.push_back(chunkWithLod);
        // add the seam octree to the destroyable list
        // temporaryNodesList.push_back(seamRoot);

        return constructOutputBuffer(vertexBuffer);
    }

    ChunkDamageBuffer &getChunkDamageBuffer(vm::ivec3 min) {
        uint64_t minHash = hashOctreeMin(min);

        const auto &iter = chunksDamageBufferHashMap.find(minHash);
        if (iter == chunksDamageBufferHashMap.end()) {
            chunksDamageBufferHashMap.emplace(std::make_pair(minHash, ChunkDamageBuffer(min, chunkSize)));
        }

        ChunkDamageBuffer &damageBuffer = chunksDamageBufferHashMap.find(minHash)->second;
        return damageBuffer;
    }

    bool drawDamageSphere(const float &x, const float &y, const float &z, const float radius, const float value, float *outPositions, unsigned int *outPositionsCount) {
        unsigned int maxPositionsCount = *outPositionsCount;
        *outPositionsCount = 0;

        bool drew = false;
        std::set<uint64_t> seenHashes;
        for (float dx = -1; dx <= 1; dx += 2) { 
            for (float dz = -1; dz <= 1; dz += 2) { 
                for (float dy = -1; dy <= 1; dy += 2) {
                    float ax = x + dx * radius;
                    float ay = y + dy * radius;
                    float az = z + dz * radius;
                    vm::ivec3 min = vm::ivec3(std::floor(ax / (float)chunkSize), std::floor(ay / (float)chunkSize), std::floor(az / (float)chunkSize)) * chunkSize;
                    uint64_t minHash = hashOctreeMin(min);
                    if (seenHashes.find(minHash) == seenHashes.end()) {
                        seenHashes.insert(minHash);

                        ChunkDamageBuffer &damageBuffer = getChunkDamageBuffer(min);
                        // std::cout << "pre draw damage " << ax << " " << ay << " " << az << " - " << min.x << " " << min.y << " " << min.z << std::endl;
                        if (damageBuffer.drawDamage(ax, ay, az, radius, value)) {
                            // std::cout << "draw damage yes 1" << std::endl;
                            if (*outPositionsCount < maxPositionsCount) {
                                // std::cout << "draw damage yes 2" << std::endl;
                                outPositions[(*outPositionsCount)++] = min.x;
                                outPositions[(*outPositionsCount)++] = min.y;
                                outPositions[(*outPositionsCount)++] = min.z;
                            } /* else {
                                std::cout << "draw damage no 2" << std::endl;
                            } */

                            drew = true;
                        } /* else {
                            std::cout << "draw damage no 1" << std::endl;
                        } */
                    }
                }
            } 
        }
        return drew;
    }
}

// int main()
// {
//     // CachedNoise sampleNoise;
//     // const double noise = sampleNoise.simplex(30,500);
//     // std::cout << noise << std::endl;

//     // std::vector<std::pair<unsigned char, float>> sortedSamples = {
//     //     std::make_pair(2, 2.f),
//     //     std::make_pair(3, 3.f),
//     //     std::make_pair(2, 2.f),
//     //     std::make_pair(2, 2.f),
//     //     std::make_pair(4, 4.f),
//     //     std::make_pair(2, 2.f),
//     // };

//     // std::unordered_map<unsigned char, size_t> count;

//     // for (std::pair<unsigned char, float> s : sortedSamples)
//     // {
//     //     count[s.first]++;
//     // }

//     // std::sort(
//     //     sortedSamples.begin(),
//     //     sortedSamples.end(),
//     //     [&count](const std::pair<unsigned char, float> &a, const std::pair<unsigned char, float> &b)
//     //     {
//     //         if (a.first == b.first)
//     //         {
//     //             return false;
//     //         }
//     //         if (count[a.first] > count[b.first])
//     //         {
//     //             return true;
//     //         }
//     //         else if (count[a.first] < count[b.first])
//     //         {
//     //             return false;
//     //         }
//     //         return a.first < b.first;
//     //     });

//     // sortedSamples.erase(std::unique(sortedSamples.begin(), sortedSamples.end()), sortedSamples.end());

//     // int selectorCounter = 0;
//     // for (size_t i = 0; i < sortedSamples.size(); i++)
//     // {
//     //     std::cout << +sortedSamples.at(i).first << std::endl;
//     //     selectorCounter++;
//     // }
//     // for (size_t i = 0; i < 4 - selectorCounter; i++)
//     // {
//     //     std::cout << +sortedSamples.at(i).first << std::endl;
//     // }
    
    

//     const vm::ivec3 OFFSETS[8] =
//         {
//             vm::ivec3(0, 0, 0), vm::ivec3(1, 0, 0), vm::ivec3(0, 0, 1), vm::ivec3(1, 0, 1),
//             vm::ivec3(0, 1, 0), vm::ivec3(1, 1, 0), vm::ivec3(0, 1, 1), vm::ivec3(1, 1, 1)};

//     const int min = -((64 * 2) / 2);
//     const int max = (64 * 2) / 2;

//     for (int x = min; x < max; x += 64)
//     {
//         for (int y = min; y < max; y += 64)
//         {
//             for (int z = min; z < max; z += 64)
//             {
//                 // std::cout << "Generating" << std::endl;
//                 DualContouring::generateChunkData(x + 32, y + 32, z + 32);
//             }
//         }
//     }

//     for (int x = min; x < max; x += 64)
//     {
//         for (int y = min; y < max; y += 64)
//         {
//             for (int z = min; z < max; z += 64)
//             {
//                 const int lod = abs(std::max(std::max(x, y), z)) / 64;
//                 DualContouring::setChunkLod(x + 32, y + 32, z + 32, 4);
//             }
//         }
//     }

//     for (int x = min; x < max; x += 64)
//     {
//         for (int y = min; y < max; y += 64)
//         {
//             for (int z = min; z < max; z += 64)
//             {
//                 std::cout << "\n"
//                           << DualContouring::createChunkMesh(x + 32, y + 32, z + 32)
//                           << std::endl;
//                 DualContouring::clearTemporaryChunkData();
//                 DualContouring::clearChunkRoot(x + 32, y + 32, z + 32);
//             }
//         }
//     }

//     return 0;
// }
