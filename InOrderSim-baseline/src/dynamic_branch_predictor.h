/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include "mips.h"
#include "abstract_branch_predictor.h"
#include <map>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <list>
#include <bitset>
/*
 * Dynamic branch predictor
 */
struct isBranchAndHistoryRow {
    std::vector<int> branchPatterHistory;
    uint32_t targetAddress;
};
class DynamicNTBranchPredictor: public AbstractBranchPredictor {
public:
    DynamicNTBranchPredictor(uint32_t historyTableAndBTBSize, int bhtWidth, int phtWidth, int rasSize);
    virtual ~DynamicNTBranchPredictor();
    virtual uint32_t getTarget(uint32_t PC, int opCode, int subopCode) override;
    virtual void update(uint32_t PC, bool taken, uint32_t target, int opCode, int subopCode) override;
private:
    //Member Variables
    int BHTWidth;
    int PHTWidth;
    int RASSize;
    int numberOfOutcomes;
    uint32_t tagMask;
    std::list <uint32_t> RAS;
    std::map<uint32_t, isBranchAndHistoryRow> globalHistoryAndBranchTargetTable;
    std::map<int, bool> outcomeToBool;
    std::map<std::vector<int>, int> patternHistoryTable; // More access to the predictor than search. So a map not a vector used.
    std::vector<char>  lookup_table;
    uint32_t mostRecentlyUsedAddrTag;
    int takenCharacter;
    int nottakenCharacter;
    
    // Member Functions
    uint32_t getTag(uint32_t PC);
    uint32_t getAddrTagToRemove();
    void initPHT(int numberPHTEntries, std::vector<int> pattern, int pos);
    bool checkIfInstrunctionIsJump(int opCode);
    std::vector<int> getPatternHistory(uint32_t addrTag);
    int getBranchOutcome(std::vector<int> patternHistory);
    bool doesPCNotExistsInBTB_GHT(uint32_t addrTag);
    bool checkIfPCNotABranch(int opCode, int subopCode);
    uint32_t checkPredictorAndGetTarget(uint32_t addrTag);
    bool checkIfInstructionIsReturn(int opCode, int subopCode);
    void updatePHTForMisPrediction(int branchOutcome, std::vector<int> patternHistory);
    void updatePHTForCorrectPrediction(int branchOutcome, std::vector<int> patternHistory);
    void checkPredictionAndUpdate(int branchOutcome, std::vector<int> patternHistory, bool taken);
    void addMissingPCInGlobalHistAndBranchTargetTable(uint32_t addrTag, uint32_t target, bool taken);
    void updateGlobalHistoryTable(uint32_t addrTag, std::vector<int> patternHistory, bool taken, uint32_t target);


};

