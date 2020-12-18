/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#ifndef __CACHE_WT_H__
#define __CACHE_WT_H__

#include "block.h"
#include "abstract_memory.h"
#include "abstract_prefetcher.h"
#include "repl_policy.h"
#include "cache.h"
#include <cstdint>



/*
 * You should implement Cache
 */
class Cache_wt: public Cache {
private:
	AbstarctReplacementPolicy *replPolicy;
	AbstractPrefetcher* prefetcher;
	MSHR* mshr;
	uint64_t cSize, associativity, blkSize, numSets;
	bool is_bottom;
	uint32_t packet_size = 0, stall = 0;	


public:
	//Pointer to an array of pointers
	Block ***blocks;
	std::queue<Block*> write_back_buffer;
	Cache_wt(uint32_t _Size, uint32_t _associativity, uint32_t _blkSize,
			enum ReplacementPolicy _replPolicy, uint32_t _delay, bool is_L1);
	virtual ~Cache_wt();
	virtual bool sendReq(Packet * pkt) override;
	virtual void recvResp(Packet* readRespPkt) override;
	virtual void Tick() override;
	int getSetNo(uint32_t addr);
	int getWay(uint32_t addr);
	virtual bool policy();
	void invalidate(uint32_t addr);
	virtual uint32_t getAssociativity();
	virtual uint32_t getNumSets();
	virtual uint32_t getBlockSize();
	/*
	 * read the data if it is in the cache. If it is not, read from memory.
	 * this is not a normal read operation, this is for debug, do not use
	 * mshr for implementing this
	 */
	virtual void dumpRead(uint32_t addr, uint32_t size, uint8_t* data) override;
	//place other functions here if necessary

};

#endif
