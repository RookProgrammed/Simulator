/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include <iostream>
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
	DPRINTF(DEBUG_CACHE,
			"request for cache for pkt : addr = %x, type = %d\n",
			pkt->addr, pkt->type);

	// Implement TRACE?

	if (reqQueue.size() < reqQueueCapacity) {
		// Miss or hit, add time for checking this cache
		pkt->ready_time += accessDelay;

		reqQueue.push(pkt);

		DPRINTF(DEBUG_CACHE,
				"packet is added to cache reqQueue with readyTime %d\n",
				pkt->ready_time);
		// return true since cache received the request successfully

		return true;
	} else {
		DPRINTF(DEBUG_CACHE,
				"packet is rejected; reqQueue in this cache is full\n");
		/*
		 * return false since cache could not add the packet
		 * to request queue, the source of the packet should retry
		 */
		
		return false;
	}

	return false;
}

/*You should complete this function*/
void Cache::recvResp(Packet* readRespPkt){

	DPRINTF(DEBUG_CACHE,
			"Cache received a response for pkt : addr = %x, type = %d\n",
			readRespPkt->addr, readRespPkt->type);

	//prev->recvResp(readRespPkt);
	
	// Act on this packet based on its type
	switch (readRespPkt->type) {
		case PacketTypeStore:
			prev->recvResp(readRespPkt);
			break;
		case PacketTypeFetch:
		case PacketTypeLoad:
			// Process a fetch or load response
			// Add time to write to the cache
			// readRespPkt->ready_time += accessDelay;

			// NOTE: Forcing response packet into reqQueue to simulate
			// a separate queue for packets from lower layers
			reqQueue.push(readRespPkt);
			break;
		default:
			assert(false && "Invalid response from lower memory level");
	}

	return;
}

/*You should complete this function*/
void Cache::Tick(){
	while (!reqQueue.empty()) {
		// check if any packet is ready to be serviced
		if (reqQueue.front()->ready_time <= currCycle) {
			Packet *pkt = reqQueue.front();
			// Nested if structure tests READ/WRITE, then REQUEST/RESPONSE
			if (pkt->isWrite) {
				if (pkt->isReq) {
					if (next->sendReq(pkt)) {
						reqQueue.pop();
					} else {
						return;
					}

					Block *b = replPolicy->getVictim(pkt->addr, true);
					// NOTE: THIS SPOT IS USEFUL FOR VICTIM BLOCK HANDLING
					int word_index = pkt->addr % blkSize;
					for (uint32_t i = 0; i < pkt->size; i++) {
						*(b->getData() + word_index + i) = *(pkt->data + i);
					}
					b->setTag(pkt->addr, numSets);
					return;
				}
			} else {
				int setNo = (numSets - 1) & (pkt->addr / blkSize);
				int way = getWay(pkt->addr);
				if (pkt->isReq) {
					if (way < 0) {
						// If we miss, check in the next lower level
						if (next->sendReq(pkt)) {
							reqQueue.pop();
							return;
						} else {
							return;
						}
					} else {
						// If we hit, respond with the information
						Block *b = blocks[setNo][way];
						int word_index = pkt->addr % blkSize;
						for (uint32_t i = 0; i < pkt->size; i++) {
							*(pkt->data + i) = *(b->getData() + word_index + i);
						}
						pkt->isReq = false;
						prev->recvResp(pkt);
					}
				} else {
					Block *b = replPolicy->getVictim(pkt->addr, true);
					// NOTE: THIS SPOT IS USEFUL FOR VICTIM BLOCK HANDLING
					// If receiving a response to a read miss, we
					// must place this data into this cache
					int word_index = pkt->addr % blkSize;
					for (uint32_t i = 0; i < pkt->size; i++) {
						*(b->getData() + word_index + i) = *(pkt->data + i);
					}
					b->setTag(pkt->addr, numSets);
					prev->recvResp(pkt);
				}
			}
			reqQueue.pop();
		} else {
			/*
			 * Assume that reqQueue is sorted by ready_time for now
			 * (because the pipeline is in-order)
			 */
			break;
		}
	}

	return;
}

/*You should complete this function*/
void Cache::dumpRead(uint32_t addr, uint32_t size, uint8_t* data){
	int setNo = (numSets - 1) & (addr / blkSize);
	int word_index = addr % blkSize;
	Block *b = blocks[setNo][getWay(addr)];
	for (uint32_t i = 0; i < size; i++) {
		*(data + i) = *(b->getData() + word_index + i);
	}
}
