/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */
#include <vector>
#include <map>
#include <iostream>
#ifndef __REPL_POLICY_H__
#define __REPL_POLICY_H__

#include"block.h"

class Cache;

/*
 * AbstarctReplacementPolicy should be inherited by your
 * implemented replacement policy
 */
class AbstarctReplacementPolicy {
public:
	AbstarctReplacementPolicy(Cache* cache);
	virtual ~AbstarctReplacementPolicy() {}
	//pointer to the Cache
	Cache* cache;
	/*
	 * should return the victim block based on the replacement
	 * policy- the caller should invalidate this block
	 */
	virtual Block* getVictim(uint32_t addr, bool isWrite) = 0;
	/*
	 * Called for both hit and miss.
	 * Should update the replacement policy metadata.
	 */
	virtual void update(uint32_t addr, int way, bool isWrite, bool isInvalid) = 0;
};

/*
 * Random replacement policy
 */
class RandomRepl: public AbstarctReplacementPolicy {
public:
	RandomRepl(Cache* cache);
	~RandomRepl() {}
	virtual Block* getVictim(uint32_t addr, bool isWrite) override;
	virtual void update(uint32_t addr, int way, bool isWrite, bool isInvalid) override;
};
//replacement policy(LRU )
class LRUReplacement: public AbstarctReplacementPolicy {
public:
    std::map<uint64_t, std::vector<Block*> > lruQueueForSet;
    LRUReplacement(Cache* cache);~LRUReplacement() {}
    virtual Block* getVictim(uint32_t addr, bool isWrite) override;virtual void update(uint32_t addr, int way, bool isWrite, bool isInvalid) override;
private:
    int getPositionToRemoveFromQueue(std::vector<Block*> lru_queue);
};

/*
 * replacement policy(PseudoLRU)PseudoLRU 
 */
class PS_LRURep_Policy: public AbstarctReplacementPolicy {
public:
    std::map<uint64_t, Block* > psuedoLRUPointerForSet;
    PS_LRURep_Policy(Cache* cache);
    ~PS_LRURep_Policy() {}
    virtual Block* getVictim(uint32_t addr, bool isWrite) override;virtual void update(uint32_t addr, int way, bool isWrite, bool isInvalid) override;
private:
    Block * FetchRandomVict_NOTMRU(uint64_t setIndex, Block* mostRecent_Block);
    };


#endif

#endif
