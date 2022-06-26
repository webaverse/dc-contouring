#ifndef RESULT_H
#define RESULT_H

#include "vectorMath.h"
#include <vector>
#include <deque>
#include <unordered_map>

//

class ResultQueue {
public:
    uint32_t ids;

    ResultQueue();
    ~ResultQueue();
    
    uint32_t getNextId();
    void pushResult(uint32_t id, void *result);
};

#endif // RESULT_H
