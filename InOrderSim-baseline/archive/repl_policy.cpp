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

void RandomRepl::update(uint32_t addr, int way, bool isWrite) {
	//there is no metadata to update:D
	return;
}
