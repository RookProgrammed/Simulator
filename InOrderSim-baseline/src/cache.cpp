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
		enum ReplacementPolicy replType, uint32_t delay, bool is_bottom):
		AbstractMemory(delay, 100),cSize(size),
		associativity(associativity), blkSize(blkSize), is_bottom(is_bottom) {

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
	packet_size = pkt->size;
	DPRINTF(DEBUG_CACHE,
			"request for cache for pkt : addr = %x, type = %d\n",
			pkt->addr, pkt->type);

	if ((reqQueue.size() < reqQueueCapacity) && !stall) {
		// Miss or hit, add time for checking this cache
		pkt->ready_time += accessDelay;

		if (pkt->isWrite && is_bottom) {
			stall = 1;
			if (is_bottom) {
				AbstractMemory* next_cache = (AbstractMemory*) next;
				AbstractMemory* main_mem = (AbstractMemory*) next_cache->next;
				stall += accessDelay + 2 * ((AbstractMemory*) next)->accessDelay
					+ main_mem->accessDelay; 
			}
		}

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
	// Act on this packet based on its type
	switch (readRespPkt->type) {
		case PacketTypeStore:
			if (!is_bottom) {
				prev2->recvResp(readRespPkt);
			} else {
				prev->recvResp(readRespPkt);
			}
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
void Cache::Tick() {
	while (!reqQueue.empty()) {
		// check if any packet is ready to be serviced
		if (reqQueue.front()->ready_time <= currCycle) {
			Packet *pkt = reqQueue.front();
			int setNo = (numSets - 1) & (pkt->addr / blkSize);
			int way = getWay(pkt->addr);
			// Nested if structure tests READ/WRITE, then REQUEST/RESPONSE
			if (pkt->isWrite) {
				Block *b = blocks[setNo][way];
				if (way < 0 || !b->getValid()) {
					// Return if we already handled for write-allocate
					if (stall == 2) {
						reqQueue.pop();
						reqQueue.push(pkt);
						return;
					}
					// Handle in accordance with write-allocate requirements
					Packet *wa_pkt = new Packet(true, false, PacketTypeLoad,
								(uint32_t) (pkt-> addr - (pkt->addr % blkSize)), (uint32_t) blkSize,
								new uint8_t[blkSize], (uint32_t) currCycle);
					wa_pkt->send_to_pipe = false;
					if (next->sendReq(wa_pkt)) {
						stall = 2;
						return;
					} else {
						delete wa_pkt;
						return;
					}
				}
				if (pkt->isReq) {
					stall = 0;
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
				if (pkt->isReq) {
					if (way < 0) {
						// If we miss, stall and check in the next lower level
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
						if ((pkt->type == PacketTypeLoad) && !is_bottom)
							prev2->recvResp(pkt);
						else prev->recvResp(pkt);
					}
				} else {
					Block *b = replPolicy->getVictim(pkt->addr, true);
					// NOTE: THIS SPOT IS USEFUL FOR VICTIM BLOCK HANDLING
					// If receiving a response to a read miss, we
					// must place this data into this cache
					for (uint32_t i = 0; i < blkSize; i++) {
						*(b->getData() + i) = *(pkt->data + i);
					}
					b->setTag(pkt->addr, numSets);
					b->setValid(true);
					if (is_bottom && pkt->send_to_pipe) {
						delete pkt->data;
						pkt->data = new uint8_t[pkt->size];
						int word_index = pkt->addr % blkSize;
						for (uint32_t i = 0; i < pkt->size; i++) {
							*(pkt->data + i) = *(b->getData() + word_index + i);
						}
						if ((pkt->type == PacketTypeLoad) && !is_bottom)
							prev2->recvResp(pkt);
						else prev->recvResp(pkt);
					} else { 
						if (!is_bottom) {
							if (pkt->type == PacketTypeLoad)
								prev2->recvResp(pkt);
							else prev->recvResp(pkt);
						}
					}
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
