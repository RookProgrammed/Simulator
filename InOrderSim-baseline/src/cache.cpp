/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include "repl_policy.h"
#include "next_line_prefetcher.h"
#include "cache.h"

Cache::Cache(uint32_t size, uint32_t associativity, uint32_t blkSize,
		enum ReplacementPolicy replType, uint32_t delay):
		AbstractMemory(delay, 100),cSize(size),
		associativity(associativity), blkSize(blkSize) {

	numSets = cSize / (blkSize * associativity);
	blocks = new Block**[numSets];
	for (int i = 0; i < (int) numSets; i++) {
		blocks[i] = new Block*[associativity];
		for (int j = 0; j < associativity; j++)
			blocks[i][j] = new Block(blkSize);
	}

	switch (replType) {
	case RandomReplPolicy:
		replPolicy = new RandomRepl(this);
		break;
	default:
		assert(false && "Unknown Replacement Policy");
	}

	/*
	 * add other required initialization here
	 * (e.g: prefetcher and mshr)
	 */

}

Cache::~Cache() {
	delete replPolicy;
	for (int i = 0; i < (int) numSets; i++) {
		for (int j = 0; j < associativity; j++)
			delete blocks[i][j];
		delete blocks[i];
	}
	delete blocks;

}

uint32_t Cache::getAssociativity() {
	return associativity;
}

uint32_t Cache::getNumSets() {
	return numSets;
}

uint32_t Cache::getBlockSize() {
	return blkSize;
}

int Cache::getWay(uint32_t addr) {
	uint32_t _addr = addr / blkSize;
	uint32_t setIndex = (numSets - 1) & _addr;
	uint32_t addrTag = _addr / numSets;
	for (int i = 0; i < (int) associativity; i++) {
		if ((blocks[setIndex][i]->getValid() == true)
				&& (blocks[setIndex][i]->getTag() == addrTag)) {
			return i;
		}
	}
	return -1;
}


/*You should complete this function*/
bool Cache::sendReq(Packet * pkt){

	return true;
}

/*You should complete this function*/
void Cache::recvResp(Packet* readRespPkt){

	return;
}

/*You should complete this function*/
void Cache::Tick(){
	return;
}

/*You should complete this function*/
void Cache::dumpRead(uint32_t addr, uint32_t size, uint8_t* data){

}
