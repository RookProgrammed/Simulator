/*MIPS pipeline cycle-accurate simulator*/

#include "dynamic_branch_predictor.h"
#include <iostream>

#include <math.h>
#include <cstdio>

//---check
BranchPredict_NT::BranchPredict_NT(uint32_t histTab_BTBSize, int Width_bht, int Width_pht, int rasSize) { // Not sure about this as both might have same sizes
    
    notTake_Character = 0;take_Character = 1;
    RASSize = rasSize; notTake_Character = 0;
    MaskTag = (histTab_BTBSize - 1)<<2;
    
    Width_PHT = Width_pht; Width_BHT = Width_bht;
    
    OutcomeCount = pow(2, Width_PHT);
    for (int i = 0; i<OutcomeCount; i++)
    {
        if(i<=(OutcomeCount/2)-1)
        {
            outcome_Bool[i] = false;
        }else{
            outcome_Bool[i] = true;
        }
    }
    int num_PHTEntries = pow(2, Width_BHT);std::vector<int> pattern (Width_BHT);
    initial_PHT(Width_BHT, pattern, 0);
    for (int m=0; m < histTab_BTBSize; m++)
    {
        BranchHistory row;
        std::vector<int> pattern;
        for (int n =0; n<num_PHTEntries; n++)
        {
          pattern.push_back(0);
        }
       row.Addres_Target = 0; row.branchPatternHist = pattern;globalHist_BranchTargetTab[m] = row;
        
    }
    AddrTag_mostRecent = (uint32_t)0;
}

void BranchPredict_NT::initial_PHT(int num_PHTEntries, std::vector<int> pattern, int pos)
{
    if(pos == num_PHTEntries)
    {
        patternHist_Tab[pattern] = 0;
        return;
    }
    pattern[pos] = 0;//false;
    initial_PHT(num_PHTEntries, pattern, pos+1);
    
    pattern[pos] = 1;//true;
    initial_PHT(num_PHTEntries, pattern, pos+1);
    return;
}

BranchPredict_NT::~BranchPredict_NT() {
   
}

uint32_t BranchPredict_NT::getTarget(uint32_t PC, int op_Code, int subop_Code) {
    uint32_t addrTag = getTag(PC);

    if (InstructReturn_check(op_Code, subop_Code))
    {
        
        uint32_t Addres_Target = RAS.front();
        std::cout<<"Addres_Target RAS:" << Addres_Target << '\n';
        RAS.pop_front();
        return Addres_Target;
    }
    else if(JumpInstrunction_check(op_Code))
    {
        std::cout<<"JUMP instruction \n";
        if(RAS.size() == RASSize)
        {
            RAS.pop_back();
            
        }
        RAS.push_front(PC+4);
    }
    
    if ( PCcheck_NotBranch(op_Code, subop_Code)  || PCcheckInBTB_GHT(addrTag) )
    {
        std::cout << "IN if branch \n";
        return -1;
    }else{
        int x;
        uint32_t target = PredictCheck_GetTarget(addrTag);
        return target;
    }
}

bool BranchPredict_NT::InstructReturn_check(int op_Code, int subop_Code)
{
    if (op_Code == OP_SPECIAL && (subop_Code == SUBOP_JALR || subop_Code == SUBOP_JR))
    {
        return true;
    }
    return false;
}


bool BranchPredict_NT::JumpInstrunction_check(int op_Code)
{
    if(op_Code == OP_JAL || op_Code == OP_J)
    {
        return true;
    }
    return false;
        
}

bool BranchPredict_NT::PCcheck_NotBranch(int op_Code, int subop_Code)
{
    if (op_Code == OP_BRSPEC) {
        if(subop_Code == BROP_BLTZ || subop_Code== BROP_BGEZ || subop_Code == BROP_BLTZAL || subop_Code == BROP_BGEZAL )
        {
            return false;
        }
    }
    else if(op_Code == OP_BEQ || op_Code== OP_BNE || op_Code == OP_BLEZ || op_Code == OP_BGTZ )
    {
        return false;

    }
    return true;
}

uint32_t BranchPredict_NT::getTag(uint32_t PC)
{

  return (PC & MaskTag)>>2;
}
bool BranchPredict_NT::PCcheckInBTB_GHT(uint32_t addrTag)
{
 return globalHist_BranchTargetTab.find(addrTag) == globalHist_BranchTargetTab.end() ? true: false;
}

uint32_t BranchPredict_NT::PredictCheck_GetTarget(uint32_t addrTag)
{ std::vector<int>patternHistory = PatternHistory_fetch(addrTag);
    int lookupTab = Branch_Outcome(patternHistory);

    for (int i =0; i< patternHistory.size(); i++)
    {
        std::cout << patternHistory[i] << ' ';
    }

    if (outcome_Bool[lookupTab] == true)
    {
        uint32_t target = globalHist_BranchTargetTab[addrTag].Addres_Target;
        std::cout << "target:" << target << '\n';
        
        if(target)
        {
        return target;
        }
        return -1;
        
    }else {
        return -1;
    }
}

std::vector<int> BranchPredict_NT::PatternHistory_fetch(uint32_t addrTag)
{
    return globalHist_BranchTargetTab[addrTag].branchPatternHist;
}

int BranchPredict_NT::Branch_Outcome(std::vector<int> patternHistory)
{
    
    return patternHist_Tab[patternHistory];
}

void BranchPredict_NT::update(uint32_t PC, bool take, uint32_t target, int op_Code, int subop_Code) {
    
    
    if (not PCcheck_NotBranch(op_Code, subop_Code))
    {
    uint32_t addrTag = getTag(PC);
    if (PCcheckInBTB_GHT(addrTag))
    {
        /* change BTB for the PC tag andPC target
        // theen Replace Global history table with PC tag and initialize with "NNN"?!
        // Check to update for branch,have to try
        //negative 1 target for branch*/
        addMissingPC_GlobalHistBranchTab(addrTag, target, take);
        
    }else{
        std::vector<int> patternHistory = PatternHistory_fetch(addrTag);
        int lookupTab = Branch_Outcome(patternHistory);checkPrediction_Update(lookupTab, patternHistory, take);
        GlobalHistTab_update(addrTag, patternHistory, take, target);
    }
    AddrTag_mostRecent = addrTag;
    }
}

void BranchPredict_NT::addMissingPC_GlobalHistBranchTab(uint32_t addrTag, uint32_t target, bool take)
{
    uint8_t addrTagToRemove = TagToRemove_getAddr();
    globalHist_BranchTargetTab.erase(addrTagToRemove);
    BranchHistory row;
    row.Addres_Target = target;
    std::vector<int> previousBranchPattern (Width_BHT);
    for (int j =0; j<Width_BHT-1; j++)
    {
        previousBranchPattern.push_back(0);
    }
    previousBranchPattern.push_back((take)? take_Character : notTake_Character);
    row.branchPatternHist = previousBranchPattern;
    globalHist_BranchTargetTab[addrTag] = row;
}

uint32_t BranchPredict_NT::TagToRemove_getAddr()
{
    // to check for rows which are free
    std::map<uint32_t, BranchHistory>::iterator it;
    for (it = globalHist_BranchTargetTab.begin(); it != globalHist_BranchTargetTab.end(); it++)
    {
        if (it->second.Addres_Target == -1) {
            return it->first;
        }
    }
    
    while (true)
    {
        auto it = globalHist_BranchTargetTab.begin();
        std::advance(it, rand() % globalHist_BranchTargetTab.size());
        uint32_t random_key = it->first;
        if(random_key!=AddrTag_mostRecent)
        {
            return random_key;
        }
    }
}


void BranchPredict_NT::GlobalHistTab_update(uint32_t addrTag, std::vector<int> patternHistory, bool take, uint32_t target)
{
    BranchHistory row;
    row.Addres_Target = target;
    std::vector<int> pattern = patternHistory;
    pattern.erase(pattern.begin());
    pattern.push_back((take)? take_Character : notTake_Character);
    row.branchPatternHist= pattern;
    globalHist_BranchTargetTab[addrTag] =row;
}

void BranchPredict_NT::checkPrediction_Update(int lookupTab, std::vector<int> patternHistory, bool take)
{
    if (outcome_Bool[lookupTab] == take)
    {
        PHTForCorrectPrediction_update(lookupTab, patternHistory);
        
    }else{ // Mis prediction
        PHTForMisPrediction_update(lookupTab, patternHistory);
    }
}

void BranchPredict_NT::PHTForCorrectPrediction_update(int lookupTab, std::vector<int> patternHistory)
{
    // there are only 2 possible sceneros
    // correct take predict
    if (outcome_Bool[lookupTab] == false) // Or take is false, reduce counter
    {
        patternHist_Tab[patternHistory] = (lookupTab-1<0) ? 0 : lookupTab-1;
    }else{
        // correct take predict
        patternHist_Tab[patternHistory] = (lookupTab+1> OutcomeCount-1)? OutcomeCount-1:lookupTab+1;
    }
}

void BranchPredict_NT::PHTForMisPrediction_update(int lookupTab, std::vector<int> patternHistory)
{
    // False Not take predict
    if (outcome_Bool[lookupTab] == false)
    {
        patternHist_Tab[patternHistory] =  lookupTab+1;
    }else{ // False take predict
        patternHist_Tab[patternHistory] =  lookupTab-1;
    }
}
