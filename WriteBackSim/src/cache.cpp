/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include <cstdio>
#include "repl_policy.h"
#include "next_line_prefetcher.h"
#include "cache.h"
#include "base_memory.h"

Cache::Cache(uint32_t size, uint32_t associativity, uint32_t blkSize,
		enum ReplacementPolicy replType, uint32_t delay, bool is_L1):
		AbstractMemory(delay, 100),cSize(size),
		associativity(associativity), blkSize(blkSize), is_L1(is_L1) {

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
	//this is used when we receive a request from the upper level
	pkt->ready_time += accessDelay;
	DPRINTF(DEBUG_CACHE,
			"cache %d receive pkt: addr = %x, ready_time = %d, type = %d\n",
			!is_L1+1, pkt->addr, pkt->ready_time, pkt->type);
	//add the packet to request queue if it has free entry
	if (reqQueue.size() < reqQueueCapacity) {
		reqQueue.push(pkt);
		//return true since memory received the request successfully
		return true;
	} 
	else {
		/*
		 * return false since memory could not add the packet
		 * to request queue, the source of packet should retry
		 */
		return false;
	}
	return true;
}

/*You should complete this function*/
void Cache::recvResp(Packet* readRespPkt){
	/******used when we recv a new Response from the lower level*****/
	// when receive a response from the lower level, and it is not null
	if (readRespPkt->data == nullptr) return; 
	// means that this level has a cache miss
	// the packet contains the information about the changing block
	uint32_t temp_addr = readRespPkt->addr;
    Block *victim_block = replPolicy->getVictim(temp_addr, true);
	uint32_t victim_tag = victim_block->getTag();                        //the victim block's tag
	uint32_t set_Index = (readRespPkt->addr / blkSize) & (numSets -1);    //the set that the victim cache and the new cache is in
	uint32_t victim_addr = (victim_tag * numSets + set_Index) * blkSize; //the victim block's address
	
	/************this is used for inclusive cache*************/
	bool L1D_newer = false;
	/************this is used for inclusive cache*************/
	bool L1D_newer = false;
	if (!is_L1){
		//currently, we are changing L2
		//make sure that the victim cache is not valide in L1
		int L1D_way = (prev==nullptr)? -1:((Cache *)prev)->getWay(victim_addr);
		int L1I_way = (prev2==nullptr)?-1:((Cache *)prev2)->getWay(victim_addr);
		int upper_Index1 = (readRespPkt->addr / blkSize) & (((Cache *)prev)->numSets -1);
		if (L1D_way >= 0){
			//this means there is a corresponding cache in the L1D
			Block * upper_block = ((Cache *)prev)->blocks[upper_Index][L1D_way];
			upper_block->setValid(false);
			if (upper_block->getDirty()){
				//means that there is a dirty block (newer block) in L1
				L1D_newer = true;
				Block *wb_block = new Block(blkSize);
				for (uint32_t i=0;i<blkSize;i++){
					wb_block->getData()[i] = upper_block->getData()[i];
				}
				wb_block->clear(upper_block->getTag());
				//write the block into the write-back buffer
				write_back_buffer.push(wb_block);
				Packet* wb_packet= new Packet(false, true, PacketTypeStore, victim_addr, blkSize, nullptr, currCycle);
				if(!next->sendReq(wb_packet)) {
					((AbstractMemory *)next)->reqQueue.push(wb_packet);
				};
			}
		}
		int upper_Index2 = (readRespPkt->addr / blkSize) & (((Cache *)prev2)->numSets -1);
		if (L1I_way >=0){
			//no write for the instruction cache known, unless some strage behavior
			((Cache *)prev2)->blocks[upper_Index2][L1I_way]->setValid(false);
		}
	}
	if (victim_block->getDirty() && !L1D_newer){
		// send a request to the lower level, indicating the packet is in the writeback buffer
        // start a new block
		Block *wb_block = new Block(blkSize);
		for (uint32_t i=0;i<blkSize;i++){
			wb_block->getData()[i] = victim_block->getData()[i];
		}
		//set the new tag
		wb_block->clear(victim_block->getTag());
		//write the block into the write-back buffer
		write_back_buffer.push(wb_block);
		Packet* wb_packet= new Packet(false, true, PacketTypeStore, victim_addr, blkSize, nullptr, currCycle);
        if(!next->sendReq(wb_packet)) {
			//because there is no buffer for the response packet
			//so temporarily increase the reqQueue's capacity will be fine
			//thus we can handle the situation when write-back fill request cannot be sent because next->reqQueue is full 
			((AbstractMemory *)next)->reqQueue.push(wb_packet);
		};
	}
	// then we write to the victim_block
	for (uint32_t i=0; i<blkSize;i++){
		victim_block->getData()[i]= *(readRespPkt->data +i);
		// printf("0x%02x ", victim_block->getData()[i]);
		// if (i%4 == 3) printf("\n");
	}
	uint32_t new_tag = (readRespPkt->addr/blkSize)/numSets;
	victim_block->clear(new_tag);
	if (readRespPkt->isWrite && is_L1) victim_block->setDirty(true);
	// then we should report the more upper level
	// depending on the package's formation
	if (!readRespPkt->isWrite){
		//means that this is a read, and we should return the result to the upper level
		if (is_L1){
			// if it is L1, then we should change the pattern for the upper level
			// printf("this is L1 cache!\n");
			uint64_t index =readRespPkt->addr % blkSize;
			// if (!is_L1)delete readRespPkt->data;
			// readRespPkt->data = new uint8_t[readRespPkt->size];
			for (uint32_t i=0; i<readRespPkt->size; i++){
				*(readRespPkt->data + i) =*(readRespPkt->data +i+index);
			}
			prev->recvResp(readRespPkt);
		}
		else{
			// if it is not L1, then we can just report to the upper level
			if (readRespPkt->type == PacketTypeFetch){
			    prev2->recvResp(readRespPkt);
			}
			else{
				prev->recvResp(readRespPkt);
			}
		}
	}
	else{
		prev->recvResp(readRespPkt);
	}
	return;
}




/*You should complete this function*/
void Cache::Tick(){
	while (!reqQueue.empty()) {
		//check if any packet is ready to be serviced
		if (reqQueue.front()->ready_time <= currCycle) {
            // printf("popped front:%d\n", reqQueue.front()->ready_time);
			Packet* respPkt = reqQueue.front();
			DPRINTF(DEBUG_CACHE,
					"cache begin processing pkt: addr = %x, ready_time = %d, type = %d\n",
					respPkt->addr, respPkt->ready_time, respPkt->type);
			int way = getWay(respPkt->addr);
			int setIndex = (numSets-1) & (respPkt->addr /blkSize);
			if (!respPkt->isReq){
				/*****this means that this is a write_back request.******/
				//then we should get the result from the write back buffer
				//so we get the head of the write back buffer:
				//TODO: add error check;
				Block *wb_block = ((Cache *)prev)->write_back_buffer.front();
				((Cache *)prev)->write_back_buffer.pop();
				uint32_t block_addr = respPkt->addr;
				uint32_t set_Index = (block_addr/blkSize) & (numSets-1);
				uint32_t set_tag = wb_block->getTag();
				uint32_t way = getWay(block_addr);
				if (way == -1){
					//this level doesn't contain the corresponding block (no-inclusive mode)
					write_back_buffer.push(wb_block);
					if (next->sendReq(respPkt)) reqQueue.pop();
					else break;
				}
				else{
					//this level contain the corresponding block
					//then we should set the block dirt, because the lower level's data is older.
					reqQueue.pop();
					for (uint32_t i=0; i<blkSize; i++){
						*(blocks[set_Index][way]->getData()+i) = *(wb_block->getData()+i);
					}
					blocks[set_Index][way]->setDirty(true);
					//after we find the block in the current layer, we need not go for a deeper memory
				}
			}
			else{
				/*********Then it is not a write back request********/
				if (respPkt->isWrite) {
					// printf("A store encountered\n");
					/*********The following is used for handling a write*********/
					if (way == -1 || !blocks[setIndex][way]->getValid()){
						// this means that we encountered a write miss
						// printf("A write miss\n");
						// cache miss, add MSHR here!!!
						if (next->sendReq(respPkt)) reqQueue.pop();
					    else break;
						// out reqQueue is already a LSQ, not seperated.
						// just send the request to the next level
					}
					else{
						// printf("no write miss\n");
						reqQueue.pop();
						Block* cur_block = blocks[setIndex][way];
						//the index inside the block, means the start point.
						int index = respPkt->addr % blkSize;
						if (is_L1){
							//perform the write in the cache
							for (uint32_t i = 0; i < respPkt->size; i++) {
							    cur_block->getData()[index + i] = *(respPkt->data + i);
							}
							cur_block->setDirty(true);
							prev->recvResp(respPkt);
						}
						else{
							// in write back, we shouldn't write the other Level, only to bring data upwards
							Block* res_block = new Block(blkSize);
							for (uint32_t i=0; i< blkSize;i++){
								*(res_block->getData()+i) = cur_block->getData()[i];
							}
							for (uint32_t i = 0; i < respPkt->size; i++) {
								*(res_block->getData()+index + i) = *(respPkt->data + i);
							}
							//this means that the block has been modified
							//then we should response to the upper level
							//change this pkt to respond pkt
							respPkt->isReq = false;
							//the data part is no longer needed
							delete respPkt->data;
							respPkt->data = nullptr;
							respPkt->data = new uint8_t[blkSize];
							for (uint32_t i=0; i<blkSize; i++){
								*(respPkt->data + i) = res_block->getData()[i];
							}
							prev->recvResp(respPkt);
						}
					}
				} 
				else {
					/***********then we handle a read ***********/
					// printf("A read at cycle:%ld\n", currCycle);
					if (way == -1 || !blocks[setIndex][way]->getValid()){
						//cache miss, add MSHR here!!!!
						// printf("there is a miss!\n");
						if (next->sendReq(respPkt)) reqQueue.pop();
					    else break;
					}
					else{
						// this means that there is a cache hit
						reqQueue.pop();
						Block* cur_block = blocks[setIndex][way];
						if (is_L1){
							int index = respPkt->addr % blkSize;
							for (uint32_t i = 0; i < respPkt->size; i++) {
							    *(respPkt->data + i) = cur_block->getData()[index + i];
						    }
						    respPkt->isReq = false;
							prev->recvResp(respPkt);
						}
						else{
							delete respPkt->data;
							respPkt->data = new uint8_t[blkSize];
							for (uint32_t i=0; i<blkSize; i++){
								*(respPkt->data + i) = cur_block->getData()[i];
							}
							if (respPkt->type == PacketTypeFetch)
						        prev2->recvResp(respPkt);
					        else prev->recvResp(respPkt);
						}
					}
			    }
			}
		}
		else {
			/*
			 * assume that reqQueue is sorted by ready_time for now
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
	int way = getWay(addr);
	if (way == -1){
		if (is_L1){
			((Cache *)next)-> dumpRead(addr, size, data);
		}
		else{
			((BaseMemory *)next)->dumpRead(addr, size, data);
		}
	}
	else {
		Block *b = blocks[setNo][way];
		for (uint32_t i = 0; i < size; i++) {
			*(data + i) = *(b->getData() + word_index + i);
	    }
	}
	
}
