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
			"cache receive pkt: addr = %x, ready_time = %d, type = %d\n",
			pkt->addr, pkt->ready_time, pkt->type);
	//add the packet to request queue if it has free entry
	if (reqQueue.size() < reqQueueCapacity) {
		reqQueue.push(pkt);
		//return true since memory received the request successfully
		return true;
	} 
	else {
		printf("bottom not received packet\n");
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
	if (is_L1) printf("\nL1:\n");
	else       printf("\nL2:\n");
	printf("Received a response from the lower level in: %ld\n",currCycle);
	if (readRespPkt->data == nullptr) return; 
	// means that this level has a cache miss
	// the packet contains the information about the changing block
    Block *victim_block = replPolicy->getVictim(readRespPkt->addr, true);
	uint32_t victim_tag = victim_block->getTag();                        //the victim block's tag
	uint32_t set_Index = (readRespPkt->addr /blkSize) & (numSets -1);    //the set that the victim cache and the new cache is in
	uint32_t victim_addr = (victim_tag * numSets + set_Index) * blkSize; //the victim block's address
	
	/************this is used for inclusive cache*************/
	if (!is_L1){
		//currently, we are changing L2
		//make sure that the victim cache is not valide in L1
		
		int L1D_way = (prev==nullptr)? -1:((Cache *)prev)->getWay(victim_addr);
		int L1I_way = (prev2==nullptr)?-1:((Cache *)prev2)->getWay(victim_addr);
		if (L1D_way >= 0){
			printf("%d\n",L1D_way);
			//this means there is a corresponding cache in the L1D
			((Cache *)prev)->blocks[set_Index][L1D_way]->setValid(false);
		}
		if (L1I_way >=0){
			((Cache *)prev2)->blocks[set_Index][L1I_way]->setValid(false);
		}
	}
	if (victim_block->getDirty()){
		printf("A dirty block!\n");
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
        next->sendReq(wb_packet);
	}
	// then we write to the victim_block
	for (uint32_t i=0; i<blkSize;i++){
		victim_block->getData()[i]= *(readRespPkt->data +i);
		printf("0x%02x ", victim_block->getData()[i]);
		if (i%4 == 3) printf("\n");
	}
	uint32_t new_tag = (readRespPkt->addr/blkSize)/numSets;
	victim_block->clear(new_tag);



	// then we should report the more upper level
	// depending on the package's formation
	if (!readRespPkt->isWrite){
		//means that this is a read, and we should return the result to the upper level
		if (is_L1){
			// if it is L1, then we should change the pattern for the upper level
			printf("this is L1 cache!\n");
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
		//report the write response
		printf("send a response packet!\n");
		printf("readPresPkt->type:%d\n", readRespPkt->type);
		prev->recvResp(readRespPkt);
	}
	return;
}




/*You should complete this function*/
void Cache::Tick(){
	while (!reqQueue.empty()) {
		//check if any packet is ready to be serviced
		
		if (reqQueue.front()->ready_time <= currCycle) {
            printf("popped front:%d\n", reqQueue.front()->ready_time);
			Packet* respPkt = reqQueue.front();
			reqQueue.pop();
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
					next->sendReq(respPkt);
				}
				else{
					//this level contain the corresponding block
					//then we should set the block dirt, because the lower level's data is older.
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
					printf("A store encountered\n");
					/*********The following is used for handling a write*********/
					if (way == -1 || !blocks[setIndex][way]->getValid()){
						// this means that we encountered a write miss
						printf("A write miss\n");
						// cache miss, add MSHR here!!!
						next->sendReq(respPkt); 
						// out reqQueue is already a LSQ, not seperated.
						// just send the request to the next level
					}
					else{
						printf("no write miss\n");
						Block* cur_block = blocks[setIndex][way];
						//the index inside the block, means the start point.
						int index = respPkt->addr % blkSize;
						//perform the write in the cache

						//the back write information is still useful. 
						for (uint32_t i = 0; i < respPkt->size; i++) {
							cur_block->getData()[index + i] = *(respPkt->data + i);
						}
						//this means that the block has been modified
					    cur_block->setDirty(true);
						//then we should response to the upper level
						//change this pkt to respond pkt
						respPkt->isReq = false;
						//the data part is no longer needed
						delete respPkt->data;
						respPkt->data = nullptr;
						respPkt->data = new uint8_t[blkSize];
						for (uint32_t i=0; i<blkSize; i++){
							*(respPkt->data + i) = cur_block->getData()[i];
						}
							/*
							* send the respond to the previous base_object which is waiting
							* for this respond packet. For now, prev for memory is core but
							* you should update the prev since you are adding the caches
							*/
						prev->recvResp(respPkt);
					}
				} 
				else {
					/***********then we handle a read ***********/
					printf("A read at cycle:%ld\n", currCycle);
					if (way == -1 || !blocks[setIndex][way]->getValid()){
						//cache miss, add MSHR here!!!!
						printf("there is a miss!\n");
						next->sendReq(respPkt);
					}
					else{
						// this means that there is a cache hit
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
	Block *b = blocks[setNo][getWay(addr)];
	for (uint32_t i = 0; i < size; i++) {
		*(data + i) = *(b->getData() + word_index + i);
	}
}
