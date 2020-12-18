/*
MIPS pipeline
 */
#include <bitset>
#include "mips.h"
#include <iterator>
#include <list>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "abstract_branch_predictor.h"



/* branch Preditor */
struct BranchHistory {
    std::vector<int> branchPatternHist;
    uint32_t Addres_Target;
};
class BranchPredict_NT: public AbstractBranchPredictor {
public:
    BranchPredict_NT(uint32_t histTab_BTBSize, int Width_bht, int Width_pht, int rasSize);
    virtual ~BranchPredict_NT();
    virtual uint32_t getTarget(uint32_t PC, int opCode, int subopCode) override;
    virtual void update(uint32_t PC, bool taken, uint32_t target, int opCode, int subopCode) override;
private:
    /* Variabls*/
    int Width_BHT;int Width_PHT;
    int OutcomeCount;int RASSize;
    
    uint32_t MaskTag;
    std::list <uint32_t> RAS; std::map<uint32_t, BranchHistory> globalHist_BranchTargetTab;
    std::map<int, bool> outcome_Bool;
	
    std::map<std::vector<int>, int> patternHist_Tab; 
    std::vector<char>  lookupTab;
    uint32_t AddrTag_mostRecent;
    int take_Character;
    int notTake_Character;
    
    /* Functions*/
    uint32_t TagToRemove_getAddr();uint32_t getTag(uint32_t PC);
    
    void initial_PHT(int num_PHTEntries, std::vector<int> pattern, int pos);
    bool JumpInstrunction_check(int opCode);
    std::vector<int> PatternHistory_fetch(uint32_t addrTag);
    int Branch_Outcome(std::vector<int> patternHistory);
    bool PCcheckInBTB_GHT(uint32_t addrTag);bool PCcheck_NotBranch(int opCode, int subopCode);
    uint32_t PredictCheck_GetTarget(uint32_t addrTag);bool InstructReturn_check(int opCode, int subopCode);
    void PHTForMisPrediction_update(int branchOutcome, std::vector<int> patternHistory);void PHTForCorrectPrediction_update(int branchOutcome, std::vector<int> patternHistory);
    void checkPrediction_Update(int branchOutcome, std::vector<int> patternHistory, bool taken);void addMissingPC_GlobalHistBranchTab(uint32_t addrTag, uint32_t target, bool taken);
    void GlobalHistTab_update(uint32_t addrTag, std::vector<int> patternHistory, bool taken, uint32_t target);


};

