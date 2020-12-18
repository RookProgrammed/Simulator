
#include "dynamic_branch_predictor.h"
#include <iostream>
#include <cstdio> 
#include <math.h>

DynamicNTBranchPredictor::DynamicNTBranchPredictor(uint32_t historyTableAndBTBSize, int bhtWidth, int phtWidth, int rasSize) { // MAYBE, DOES NOT REQUIRE TWO SIZES AS BOTH SIZES MUST BE SAME.
    
    takenCharacter = 1;
    nottakenCharacter = 0;
    RASSize = rasSize;
    tagMask = (historyTableAndBTBSize - 1)<<2;
    BHTWidth = bhtWidth;
    PHTWidth = phtWidth;
    
    numberOfOutcomes = pow(2, PHTWidth);
    for (int i = 0; i<numberOfOutcomes; i++)
    {
            outcomeToBool[i] = true;
    }
    int numberPHTEntries = pow(2, BHTWidth);
    
std::vector<int> pattern (BHTWidth);

    initPHT(BHTWidth, pattern, 0);

    for (int i=0; i < historyTableAndBTBSize; i++)

    {
        isBranchAndHistoryRow row;std::vector<int> pattern;

        for (int j =0; j<numberPHTEntries; j++)
        {
            pattern.push_back(0);
        }
        row.branchPatterHistory = pattern;

        row.targetAddress = 0;

        globalHistoryAndBranchTargetTable[i] = row;
    }

    mostRecentlyUsedAddrTag = (uint32_t)0;

}

void DynamicNTBranchPredictor::initPHT(int numberPHTEntries, std::vector<int> pattern, int pos)
{
    if(pos == numberPHTEntries)
    
{

        patternHistoryTable[pattern] = 0;

        return;

    }
    pattern[pos] = 0;//false; 
    
    pattern[pos] = 1;//true;
    
initPHT(numberPHTEntries, pattern, pos+1);    return;
}

DynamicNTBranchPredictor::~DynamicNTBranchPredictor() {
    // Auto-gene destruct
}

uint32_t DynamicNTBranchPredictor::getTarget(uint32_t PC, int opCode, int subopCode) {
    uint32_t addrTag = getTag(PC);
    if (checkIfInstructionIsReturn(opCode, subopCode))
    {
        
        uint32_t targetAddress = RAS.front();
        std::cout<<"targetAddress RAS:" << targetAddress << '\n';
        RAS.pop_front();
        return targetAddress;
    }
    else if(checkIfInstrunctionIsJump(opCode))
    {
        std::cout<<"JUMP instruction \n";
        if(RAS.size() == RASSize)
        {
            RAS.pop_back();
            
        }
        RAS.push_front(PC+4);
    }
    
    if ( checkIfPCNotABranch(opCode, subopCode)  || doesPCNotExistsInBTB_GHT(addrTag) )
    {
        std::cout << "IN if branch \n";
        return -1;
    }else{
        int x;
        uint32_t target = checkPredictorAndGetTarget(addrTag);
        return target;
    }
}

bool DynamicNTBranchPredictor::checkIfInstructionIsReturn(int opCode, int subopCode)
{
    if (opCode == OP_SPECIAL && (subopCode == SUBOP_JALR || subopCode == SUBOP_JR))
    {
        return true;
    }
    return false;
}


bool DynamicNTBranchPredictor::checkIfInstrunctionIsJump(int opCode)
{
    if(opCode == OP_JAL || opCode == OP_J)
    {
        return true;
    }
    return false;
        
}

bool DynamicNTBranchPredictor::checkIfPCNotABranch(int opCode, int subopCode)
{
    if (opCode == OP_BRSPEC) {
        if(subopCode == BROP_BLTZ || subopCode== BROP_BGEZ || subopCode == BROP_BLTZAL || subopCode == BROP_BGEZAL )
        {
            return false;
        }
    }
    else if(opCode == OP_BEQ || opCode== OP_BNE || opCode == OP_BLEZ || opCode == OP_BGTZ )
    {
        return false;

    }
    return true;
}

uint32_t DynamicNTBranchPredictor::getTag(uint32_t PC)
{
        return (PC & tagMask)>>2;
}

bool DynamicNTBranchPredictor::doesPCNotExistsInBTB_GHT(uint32_t addrTag)
{
    return globalHistoryAndBranchTargetTable.find(addrTag) == globalHistoryAndBranchTargetTable.end() ? true: false;
}

uint32_t DynamicNTBranchPredictor::checkPredictorAndGetTarget(uint32_t addrTag)
{
    std::vector<int>patternHistory = getPatternHistory(addrTag);
    int branchOutcome = getBranchOutcome(patternHistory);
    for (int i =0; i< patternHistory.size(); i++)
    {
        std::cout << patternHistory[i] << ' ';
    }
    if (outcomeToBool[branchOutcome] == true)
    {
        uint32_t target = globalHistoryAndBranchTargetTable[addrTag].targetAddress;
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

std::vector<int> DynamicNTBranchPredictor::getPatternHistory(uint32_t addrTag)
{
    return globalHistoryAndBranchTargetTable[addrTag].branchPatterHistory;
}

int DynamicNTBranchPredictor::getBranchOutcome(std::vector<int> patternHistory)
{
    
    return patternHistoryTable[patternHistory];
}

void DynamicNTBranchPredictor::update(uint32_t PC, bool taken, uint32_t target, int opCode, int subopCode) {
    
    if (not checkIfPCNotABranch(opCode, subopCode))
    {
    uint32_t addrTag = getTag(PC);
    if (doesPCNotExistsInBTB_GHT(addrTag))
    {
        addMissingPCInGlobalHistAndBranchTargetTable(addrTag, target, taken);
        
    }else{
        std::vector<int> patternHistory = getPatternHistory(addrTag);
        int branchOutcome = getBranchOutcome(patternHistory);
        checkPredictionAndUpdate(branchOutcome, patternHistory, taken);
        updateGlobalHistoryTable(addrTag, patternHistory, taken, target);
    }
    mostRecentlyUsedAddrTag = addrTag;
    }
}

void DynamicNTBranchPredictor::addMissingPCInGlobalHistAndBranchTargetTable(uint32_t addrTag, uint32_t target, bool taken)
{
    uint8_t addrTagToRemove = getAddrTagToRemove();
    globalHistoryAndBranchTargetTable.erase(addrTagToRemove);
    isBranchAndHistoryRow row;
    row.targetAddress = target;
    std::vector<int> previousBranchPattern (BHTWidth);
    for (int j =0; j<BHTWidth-1; j++)
    {
        previousBranchPattern.push_back(0);
    }
    previousBranchPattern.push_back((taken)? takenCharacter : nottakenCharacter);
    row.branchPatterHistory = previousBranchPattern;
    globalHistoryAndBranchTargetTable[addrTag] = row;
}

uint32_t DynamicNTBranchPredictor::getAddrTagToRemove()
{
    // Check if any existing free rows
    std::map<uint32_t, isBranchAndHistoryRow>::iterator it;
    for (it = globalHistoryAndBranchTargetTable.begin(); it != globalHistoryAndBranchTargetTable.end(); it++)
    {
        if (it->second.targetAddress == -1) {
            return it->first;
        }
    }
    
    while (true)
    {
        auto it = globalHistoryAndBranchTargetTable.begin();
        std::advance(it, rand() % globalHistoryAndBranchTargetTable.size());
        uint32_t random_key = it->first;
        if(random_key!=mostRecentlyUsedAddrTag)
        {
            return random_key;
        }
    }
}


void DynamicNTBranchPredictor::updateGlobalHistoryTable(uint32_t addrTag, std::vector<int> patternHistory, bool taken, uint32_t target)
{
    isBranchAndHistoryRow row;
    row.targetAddress = target;
    std::vector<int> pattern = patternHistory;
    pattern.erase(pattern.begin());
    pattern.push_back((taken)? takenCharacter : nottakenCharacter);
    row.branchPatterHistory= pattern;
    globalHistoryAndBranchTargetTable[addrTag] =row;
}

void DynamicNTBranchPredictor::checkPredictionAndUpdate(int branchOutcome, std::vector<int> patternHistory, bool taken)
{
    if (outcomeToBool[branchOutcome] == taken)
    {
        updatePHTForCorrectPrediction(branchOutcome, patternHistory);
        
    }else{ // Mis prediction
        updatePHTForMisPrediction(branchOutcome, patternHistory);
    }
}

void DynamicNTBranchPredictor::updatePHTForCorrectPrediction(int branchOutcome, std::vector<int> patternHistory)
{
    // 2 scenarios
    // 1. predicted not taken and not taken
    if (outcomeToBool[branchOutcome] == false)
    {
        patternHistoryTable[patternHistory] = (branchOutcome-1<0) ? 0 : branchOutcome-1;
    }else{
        // 2. predicted taken and taken
        patternHistoryTable[patternHistory] = (branchOutcome+1> numberOfOutcomes-1)? numberOfOutcomes-1:branchOutcome+1;
    }
}

void DynamicNTBranchPredictor::updatePHTForMisPrediction(int branchOutcome, std::vector<int> patternHistory)
{
    // Predicted not taken but taken
    if (outcomeToBool[branchOutcome] == false)
    {
        patternHistoryTable[patternHistory] =  branchOutcome+1;
    }else{ // Predicted taken but not taken
        patternHistoryTable[patternHistory] =  branchOutcome-1;
    }
}
