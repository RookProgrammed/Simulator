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
struct BranchHistory 
{
    std::vector<int> branchPatternHist;
    uint32_t Addres_Target;
};

class BranchPredict_NT: public AbstractBranchPredictor 
{
public:
    BranchPredict_NT(uint32_t histTab_BTBSize, int Width_bht, int Width_pht, int rasSize);
    virtual ~BranchPredict_NT();
    virtual uint32_t getTarget(uint32_t PC, int op_Code, int subop_Code) override;
    virtual void update(uint32_t PC, bool take, uint32_t target, int op_Code, int subop_Code) override;
private:
    /* Variables*/
    int Width_BHT;
    int Width_PHT;
    int OutcomeCount;
    int RASSize;
    
    uint32_t MaskTag;
    std::list <uint32_t> RAS; 
    std::map<uint32_t, BranchHistory> globalHist_BranchTargetTab;
    std::map<int, bool> outcome_Bool;
	/* predictor over search hence we utilise map over vector*/
    std::map<std::vector<int>, int> patternHist_Tab; 
    std::vector<char>  lookupTab;
    uint32_t AddrTag_mostRecent;
    int take_Character;
    int notTake_Character;
    
    /* Functions*/
    uint32_t TagToRemove_getAddr();
    uint32_t getTag(uint32_t PC);
    void initial_PHT(int num_PHTEntries, std::vector<int> pattern, int pos);
    bool JumpInstrunction_check(int op_Code);
    std::vector<int> PatternHistory_fetch(uint32_t addrTag);
    int Branch_Outcome(std::vector<int> patternHistory);
    bool PCcheckInBTB_GHT(uint32_t addrTag);
    bool PCcheck_NotBranch(int op_Code, int subop_Code);
    uint32_t PredictCheck_GetTarget(uint32_t addrTag);
    bool InstructReturn_check(int op_Code, int subop_Code);
    void PHTForMisPrediction_update(int branchOutcome, std::vector<int> patternHistory);
    void PHTForCorrectPrediction_update(int branchOutcome, std::vector<int> patternHistory);
    void checkPrediction_Update(int branchOutcome, std::vector<int> patternHistory, bool take);
    void addMissingPC_GlobalHistBranchTab(uint32_t addrTag, uint32_t target, bool take);
    void GlobalHistTab_update(uint32_t addrTag, std::vector<int> patternHistory, bool take, uint32_t target);
};
