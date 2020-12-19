/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include "repl_policy.h"
#include "cache.h"

AbstarctReplacementPolicy::AbstarctReplacementPolicy(Cache* cache) :
		cache(cache) {
}


RandomRepl::RandomRepl(Cache* cache) :
		AbstarctReplacementPolicy(cache) {
}


Block* RandomRepl::getVictim(uint32_t addr, bool isWrite) {
	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	//first check if there is a free block to allocate
	for (int i = 0; i < (int) cache->getAssociativity(); i++) {
		if (cache->blocks[setIndex][i]->getValid() == false) {
			return cache->blocks[setIndex][i];
		}
	}
	//randomly choose a block
	int victim_index = rand() % cache->getAssociativity();
	return cache->blocks[setIndex][victim_index];
}

void RandomRepl::update(uint32_t addr, int way, bool isWrite, bool isInvalid) {
	//there is no metadata to update:D
	return;
}


/*
 * LRU
 */
LRUReplacement::LRUReplacement(Cache* cache):
    AbstarctReplacementPolicy(cache)
    {
            for (int setIndex = 0; setIndex < (int) cache->getNumSets(); setIndex++) {
            std::vector<Block*> lruQueue;
            for (int j = 0; j < (int)cache->getAssociativity(); j++)
            {
                lruQueue.push_back(cache->blocks[setIndex][j]);
            }
            lruQueueForSet[setIndex] = lruQueue;
        }
    }


int LRUReplacement::getPositionToRemoveFromQueue(std::vector<Block*> lru_queue)
       {
        int posToRemove = -1;
        for(int i=0; i< lru_queue.size(); i++)
        {
            if(!lru_queue[i]->getValid())
            {
                posToRemove = i;
                break;
            }
        }
        return posToRemove;
    }

Block* LRUReplacement::getVictim(uint32_t addr, bool isWrite)
    {
    
        Block* victimBlock;
        uint64_t setIndex = cache->getSetIndex(addr);
        std::vector<Block*> lru_queue = lruQueueForSet[setIndex];
        int posToRemove = getPositionToRemoveFromQueue(lru_queue);
        if(posToRemove!=-1)
        {
            victimBlock = lru_queue[posToRemove];
            lru_queue.erase(lru_queue.begin()+ posToRemove);
        }else
        {
            victimBlock = lru_queue[0];
            lru_queue.erase(lru_queue.begin());
        }
        lruQueueForSet[setIndex] = lru_queue;
        return victimBlock;
}


void LRUReplacement::update(uint32_t addr, int way, bool isWrite, bool isInvalid) { // Better To get the block directly
    /*
       CASE 1: LRU block is popped off in getVictim, so the MRU block can be added to last position in .
       CASE 2: When a cahce hit, nothing is evicted, so the Q is full. Now the Q has just to be reordered.
       See if block in LRUQ,
            - If not, add to free Position;
            - If yes, then check if its isInvalid
                    - If yes, then remove from Q ans shift Q
                    - If no, make it the MRU block
    */
    uint64_t setIndex = cache->getSetIndex(addr);
    uint32_t tag = cache->getTag(addr);
    std::vector<Block*> lru_queue = lruQueueForSet[setIndex];
    int posInQueue = -1;
    for(int i=0; i< lru_queue.size(); i++)
    {
        if(lru_queue[i]->getTag() == tag)
        {
            posInQueue = i;
            break;
        }
    }
    
    if(posInQueue == -1) // if block not in Q, it means there is an empty position in the Q
    {
        Block* blockToWrite = cache->blocks[setIndex][way];
        lru_queue.push_back(blockToWrite);
        assert(lruQueueForSet[setIndex].size() <= cache->getAssociativity() && "Accessing invalid memory");
    }else{
        Block* LRUBlock = lru_queue[posInQueue];
        lru_queue.erase(lru_queue.begin()+posInQueue);
        lru_queue.push_back(LRUBlock);
    }
    lruQueueForSet[setIndex] = lru_queue;
    return;
}


/*
 * PseudoLRU
 */
PS_LRURep_Policy::PS_LRURep_Policy(Cache* cache):
    AbstarctReplacementPolicy(cache)
    {
        for (int setIndex = 0; setIndex < (int) cache->getNumSets(); setIndex++) {
            psuedoLRUPointerForSet[setIndex] = cache->blocks[setIndex][0];
        }
    }


Block* PS_LRURep_Policy::getVictim(uint32_t addr, bool isWrite) {
    uint64_t setIndex = cache->getSetIndex(addr);

    //first check if there is a free block to allocate
    for (int i = 0; i < (int) cache->getAssociativity(); i++) {
        if (cache->blocks[setIndex][i]->getValid() == false) {
            return cache->blocks[setIndex][i];
        }
    }
    
    // Choose LRU Block
    Block* mostRecent_Block = psuedoLRUPointerForSet[setIndex];
    return FetchRandomVict_NOTMRU(setIndex, mostRecent_Block);
    
}

Block * PS_LRURep_Policy::FetchRandomVict_NOTMRU(uint64_t setIndex, Block* mostRecent_Block)
{
    std::vector<Block*> tmp_array;
    for (int i = 0; i < (int) cache->getAssociativity(); i++) {
        if (cache->blocks[setIndex][i]->getTag() != mostRecent_Block->getTag()) {
            tmp_array.push_back(cache->blocks[setIndex][i]);;
        }
        
    }
    int victim_index = rand() % tmp_array.size();
    return tmp_array[victim_index];
}

void PS_LRURep_Policy::update(uint32_t addr, int way, bool isWrite, bool isInvalid) {
    uint64_t setIndex = cache->getSetIndex(addr);
    uint32_t tag = cache->getTag(addr);
    Block* mostRecent_Block = psuedoLRUPointerForSet[setIndex];;
    if(isInvalid && mostRecent_Block->getTag() == cache->blocks[setIndex][way]->getTag())
    {
        mostRecent_Block = FetchRandomVict_NOTMRU(setIndex, mostRecent_Block);
    }else{
        mostRecent_Block = cache->blocks[setIndex][way];
    }
    psuedoLRUPointerForSet[setIndex] = mostRecent_Block;
    return;
}
