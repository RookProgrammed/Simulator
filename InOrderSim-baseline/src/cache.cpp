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
	
	// Handle for STALL
	// This allows the cache to write with no hit/miss-under-miss functionality
	// This should be revised for MSHR/write buffer

	// This first if statement rejects packets if stalling
	// Note* All operations go through this if statement eventually
	if ((reqQueue.size() < reqQueueCapacity) && !stall) {
		// Miss or hit, add time for checking this cache
		pkt->ready_time += accessDelay;

		// Start the STALL if we see a write action
		if (pkt->isWrite && is_bottom) {
			// Signal for stall
			stall = 1;

			// Bottom signifies the L1 caches
			// A write to an L1 cache adds a large delay to clear the memory pipeline
			// After the memory pipeline clears, the write may execute
			// Hence, the long delay.  This is for WRITE ALLOCATE
			if (is_bottom) {
				AbstractMemory* next_cache = (AbstractMemory*) next;
				AbstractMemory* main_mem = (AbstractMemory*) next_cache->next;
				stall += accessDelay + 2 * ((AbstractMemory*) next)->accessDelay
					+ main_mem->accessDelay; 
			}
		}

		// Push the packet for action in Tick
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
// This is for responses from deeper levels of the memory hierarchy
void Cache::recvResp(Packet* readRespPkt){
	DPRINTF(DEBUG_CACHE,
			"Cache received a response for pkt : addr = %x, type = %d\n",
			readRespPkt->addr, readRespPkt->type);

	// Act on this packet based on its type
	switch (readRespPkt->type) {
		case PacketTypeStore:
			// This if statement is for different actions for different caches
			// The L2 will write to L1D at "prev2"
			// The L1 cache writes to pipe at "prev"
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
			// Pre-calculate the SET and WAY for where this packet would settle into the cache
			int setNo = (numSets - 1) & (pkt->addr / blkSize);
			int way = getWay(pkt->addr);
			// Nested if structure tests READ/WRITE, then REQUEST/RESPONSE
			if (pkt->isWrite) {
				Block *b;
				if (way >= 0) {
					b = blocks[setNo][way];
				}
				if (way < 0 || !b->getValid()) {
					// Return if we already handled for write-allocate
					// This forces a stall loop until the cache block for writing is filled with valid data
					// The push and pop allow the memory response to be handled
					// This action is for WRITE ALLOCATE
					// stall = 2 means the cache is waiting on the cache block to be filled before writing data
					// for write allocate action
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
					// Try to release the write allocate packet, or delete and try again if next level can't accept
					if (next->sendReq(wa_pkt)) {
						stall = 2;
						return;
					} else {
						delete wa_pkt;
						return;
					}
				}
				// See if the packet is a request (it should be, a response should have been handled in recvResp)
				if (pkt->isReq) {
					// Clear the stall!  We can proceed because the cache block is now valid
					stall = 0;
					// Send the pkt to the next level of the hierarch for writing there
					if (next->sendReq(pkt)) {
						reqQueue.pop();
					} else {
						return;
					}
					int word_index = pkt->addr % blkSize;
					// Replace the block data with packet data
					for (uint32_t i = 0; i < pkt->size; i++) {
						*(b->getData() + word_index + i) = *(pkt->data + i);
					}
					b->setTag(pkt->addr, numSets);
					return;
				}
			} else {
				// Now for FETCH/LOAD packets
				if (pkt->isReq) {
					// A way of < 0 signals a write miss
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
						if (is_bottom) {
							// copy block data into packet
							for (uint32_t i = 0; i < pkt->size; i++) {
								*(pkt->data + i) = *(b->getData() + word_index + i);
							}
							// Signal it is now a response packet
							pkt->isReq = false;
							// send to proper location
							if ((pkt->type == PacketTypeLoad) && !is_bottom)
								prev2->recvResp(pkt);
							else prev->recvResp(pkt);
						} else {
							delete pkt->data;
							pkt->data = new uint8_t[blkSize];
							for (uint32_t i = 0; i < blkSize; i++) {
								*(pkt->data + i) = *(b->getData() + i);
							}
							pkt->isReq = false;
							if ((pkt->type == PacketTypeLoad)) {
								prev2->recvResp(pkt);
							} else {
								prev->recvResp(pkt);
							}
						}
					}
				} else {
					// Now handling a response to a read miss, and write-allocate responses
					Block *b = replPolicy->getVictim(pkt->addr, true);
					// NOTE: THIS SPOT IS USEFUL FOR VICTIM BLOCK HANDLING
					// If receiving a response to a read miss, we
					// must place this data into this cache
					for (uint32_t i = 0; i < blkSize; i++) {
						*(b->getData() + i) = *(pkt->data + i);
					}
					// tag and validate cache block
					b->setTag(pkt->addr, numSets);
					b->setValid(true);
					// take our blkSize sized packet and make is packet sized (64 bytes to 4 bytes on original config)
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
