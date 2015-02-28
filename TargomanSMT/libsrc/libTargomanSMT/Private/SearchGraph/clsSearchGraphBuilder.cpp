/*************************************************************************
 * Copyright © 2012-2015, Targoman.com
 *
 * Published under the terms of TCRL(Targoman Community Research License)
 * You can find a copy of the license file with distributed source or
 * download it from http://targoman.com/License.txt
 *
 *************************************************************************/
/**
 @author S. Mohammad M. Ziabary <smm@ziabary.com>
 @author Behrooz Vedadian <vedadian@gmail.com>
 */

#include <functional>

#include "clsSearchGraphBuilder.h"
#include "../GlobalConfigs.h"
#include "Private/Proxies/intfLMSentenceScorer.hpp"
#include "Private/OOVHandler/OOVHandler.h"

#define PBT_MAXIMUM_COST 1e200

namespace Targoman{
namespace SMT {
namespace Private{
namespace SearchGraph {

using namespace Common;
using namespace Common::Configuration;
using namespace RuleTable;
using namespace Proxies;
using namespace InputDecomposer;
using namespace OOV;

tmplConfigurable<quint8> clsSearchGraphBuilder::ReorderingConstraintMaximumRuns(
        clsSearchGraphBuilder::moduleBaseconfig() + "/ReorderingConstraintMaximumRuns",
        "IBM1 reordering constraint",
        2);
tmplConfigurable<bool>   clsSearchGraphBuilder::DoComputePositionSpecificRestCosts(
        clsSearchGraphBuilder::moduleBaseconfig() + "/DoComputeReorderingRestCosts",
        "TODO Desc",
        true);
tmplConfigurable<quint8> clsPhraseCandidateCollectionData::ObservationHistogramSize(
        clsSearchGraphBuilder::moduleBaseconfig() + "/ObservationHistogramSize",
        "TODO Desc",
        100);

tmplConfigurable<bool>   clsSearchGraphBuilder::PrunePreInsertion(
        clsSearchGraphBuilder::moduleBaseconfig() + "/PrunePreInsertion",
        "TODO Desc",
        true);

FeatureFunction::intfFeatureFunction*  clsSearchGraphBuilder::pPhraseTable = NULL;
RuleTable::intfRuleTable*              clsSearchGraphBuilder::pRuleTable = NULL;
RuleTable::clsRuleNode*                clsSearchGraphBuilder::UnknownWordRuleNode;

/**********************************************************************************/
clsSearchGraphBuilder::clsSearchGraphBuilder(const Sentence_t& _sentence):
    Data(new clsSearchGraphBuilderData(_sentence))
{}
/**
 * @brief Loads rule and phrase tables, inititializes all feature functions and sets #UnknownWordRuleNode.
 * @param _configFilePath Address of config file.
 */
void clsSearchGraphBuilder::init(const QString& _configFilePath)
{
    clsSearchGraphBuilder::pRuleTable = gConfigs.RuleTable.getInstance<intfRuleTable>();
    clsSearchGraphBuilder::pRuleTable->initializeSchema();
    foreach (FeatureFunction::intfFeatureFunction* FF, gConfigs.ActiveFeatureFunctions)
        FF->initialize(_configFilePath);
    clsSearchGraphBuilder::pRuleTable->loadTableData();
    clsSearchGraphBuilder::pPhraseTable = gConfigs.ActiveFeatureFunctions.value("PhraseTable");

    RulesPrefixTree_t::Node* Node = clsSearchGraphBuilder::pRuleTable->getPrefixTree().rootNode();
    if(Node == NULL)
        throw exSearchGraph("Invalid empty Rule Table");

    Node = Node->follow(0); //Search for UNKNOWN word Index
    if (Node == NULL)
        throw exSearchGraph("No Rule defined for UNKNOWN word");

    clsSearchGraphBuilder::UnknownWordRuleNode = new clsRuleNode(Node->getData());

    //InvalidTargetRuleData has been marshalled here because it depends on loading RuleTable
    RuleTable::InvalidTargetRuleData = new RuleTable::clsTargetRuleData;

    //pInvalidTargetRule has been marshalled here because it depends on instantiation of InvalidTargetRuleData
    RuleTable::pInvalidTargetRule = new RuleTable::clsTargetRule;

    //InvalidSearchGraphNodeData has been marshalled here because it depends on initialization of gConfigs
    InvalidSearchGraphNodeData = new clsSearchGraphNodeData;

    pInvalidSearchGraphNode = new clsSearchGraphNode;
}

/**
 * @brief Looks up prefix tree for all phrases that matches with some parts of input sentence and stores them in the
 * PhraseCandidateCollections of #Data. This function also calculates maximum length of matching source phrase with phrase table.
 */

void clsSearchGraphBuilder::collectPhraseCandidates()
{
    this->Data->MaxMatchingSourcePhraseCardinality = 0;
    for (size_t FirstPosition = 0; FirstPosition < (size_t)this->Data->Sentence.size(); ++FirstPosition) {
        this->Data->PhraseCandidateCollections.append(QVector<clsPhraseCandidateCollection>(this->Data->Sentence.size() - FirstPosition));
        RulesPrefixTree_t::Node* PrevNode = this->pRuleTable->getPrefixTree().rootNode();

        if(true /* On 1-grams */)
        {
            PrevNode = PrevNode->follow(this->Data->Sentence.at(FirstPosition).wordIndex());
            if (PrevNode == NULL){
                clsRuleNode OOVRuleNode =
                        OOVHandler::instance().getRuleNode(this->Data->Sentence.at(FirstPosition).wordIndex());
                if (OOVRuleNode.isInvalid())
                    this->Data->PhraseCandidateCollections[FirstPosition][0] = clsPhraseCandidateCollection(FirstPosition, FirstPosition + 1, *clsSearchGraphBuilder::UnknownWordRuleNode);
                else
                    this->Data->PhraseCandidateCollections[FirstPosition][0] = clsPhraseCandidateCollection(FirstPosition, FirstPosition + 1, OOVRuleNode);
            }else
                this->Data->PhraseCandidateCollections[FirstPosition][0] = clsPhraseCandidateCollection(FirstPosition, FirstPosition + 1, PrevNode->getData());

            if (this->Data->PhraseCandidateCollections[FirstPosition][0].isInvalid() == false)
                this->Data->MaxMatchingSourcePhraseCardinality = qMax(this->Data->MaxMatchingSourcePhraseCardinality, 1);

            if (PrevNode == NULL)
                continue;
        }

        //Max PhraseTable order will be implicitly checked by follow
        for (size_t LastPosition = FirstPosition + 1; LastPosition < (size_t)this->Data->Sentence.size() ; ++LastPosition){
            PrevNode = PrevNode->follow(this->Data->Sentence.at(LastPosition).wordIndex());

            if (PrevNode == NULL)
                break; // appending next word breaks phrase lookup

            this->Data->PhraseCandidateCollections[FirstPosition][LastPosition - FirstPosition] = clsPhraseCandidateCollection(FirstPosition, LastPosition + 1, PrevNode->getData());
            if (this->Data->PhraseCandidateCollections[FirstPosition][LastPosition - FirstPosition].isInvalid() == false)
                this->Data->MaxMatchingSourcePhraseCardinality = qMax(this->Data->MaxMatchingSourcePhraseCardinality,
                                                                      (int)(LastPosition - FirstPosition + 1));
        }
    }
}


/**
 * @brief This function checks IBM1 Constraints.
 *
 * Checks whether number of zeros before last 1 in the input coverage is less than #ReorderingConstraintMaximumRuns or not.
 * @param _newCoverage
 * @return
 */
bool clsSearchGraphBuilder::conformsIBM1Constraint(const Coverage_t& _newCoverage)
{
    //Find last bit set then check how many bits are zero before this.
    for(int i=_newCoverage.size() - 1; i>=0; --i)
        if(_newCoverage.testBit(i)){
            size_t CoutOfPrevZeros = _newCoverage.count(false) + i - _newCoverage.size() + 1;
            return (CoutOfPrevZeros <= this->ReorderingConstraintMaximumRuns.value());
        }
    return true;
}


/**
 * @brief This is the main function that performs the decoding process.
 *
 * This function first initializes the rest cost matrix in order to be able to calculate rest costs in each path of translation.
 * Then in order to seperate all search graph nodes, this function seperates them by their number of translated words. This notion is called cardinality.
 * For each cardinality loops over all posible previous cardinalities and for each one of them finds all nodes with those cardinalities.
 * In the next step, these founded previous nodes, based on their translation coverage, will be expanded to make new search
 * graph nodes with NewCardinality size. In this process, some of new Nodes will be prunned to have valid and more
 * promising search graph nodes.
 *
 * @return Returns whether it was able to find a translation or not.
 */

bool clsSearchGraphBuilder::decode()
{
    this->Data->HypothesisHolder.clear();
    this->Data->HypothesisHolder.resize(this->Data->Sentence.size() + 1);

    this->initializeRestCostsMatrix();


    for (int NewCardinality = 1; NewCardinality <= this->Data->Sentence.size(); ++NewCardinality){

        int PrunedByIBMConstraint;
        int PrunedByLexicalHypothesis;

        bool IsFinal = (NewCardinality == this->Data->Sentence.size());
        int MinPrevCardinality = qMax(NewCardinality - this->Data->MaxMatchingSourcePhraseCardinality, 0);

        for (int PrevCardinality = MinPrevCardinality;
             PrevCardinality < NewCardinality; ++PrevCardinality) {

            unsigned short NewPhraseCardinality = NewCardinality - PrevCardinality;

            //This happens when we have for ex. 2 bi-grams and a quad-gram but no similar 3-gram. due to bad training
            if(this->Data->HypothesisHolder[PrevCardinality].isEmpty()) {

                //std::cout<<__LINE__<< ": ERROR: cardinality Container for previous cardinality empty.\n";
                continue;
            }

            for(CoverageLexicalHypothesisMap_t::Iterator PrevCoverageIter = this->Data->HypothesisHolder[PrevCardinality].lexicalHypotheses().begin();
                PrevCoverageIter != this->Data->HypothesisHolder[PrevCardinality].lexicalHypotheses().end();
                ++PrevCoverageIter){


                const Coverage_t& PrevCoverage = PrevCoverageIter.key();
                clsLexicalHypothesisContainer& PrevLexHypoContainer = PrevCoverageIter.value();

                Q_ASSERT(PrevCoverage.count(true) == PrevCardinality);
                Q_ASSERT(PrevLexHypoContainer.nodes().size());


                // TODO: this can be removed if pruning works properly
                if (PrevLexHypoContainer.nodes().isEmpty()){
                    //TODO convert to log
                    TargomanLogWarn(1, "PrevLexHypoContainer is empty. PrevCard: " << PrevCardinality
                                  << "PrevCov: " << bitArray2Str(PrevCoverage)
                                  <<" Addr:" <<(void*)PrevLexHypoContainer.Data.data());

                    continue;
                }

                /*
                 * TODO at this point pbt performs cardinality pruning (cf. discardedAtA / cardhist, cardthres pruning)
                 * this is not implemented here.
                 */
                for (size_t NewPhraseBeginPos = 0;
                     NewPhraseBeginPos <= (size_t)this->Data->Sentence.size() - NewPhraseCardinality;
                     ++NewPhraseBeginPos){
                    size_t NewPhraseEndPos = NewPhraseBeginPos + NewPhraseCardinality;
                    //TargomanDebug(1, "start position: "<< StartPos);

                    // skip if phrase coverage is not compatible with previous sentence coverage
                    bool SkipStep = false;
                    for (size_t i= NewPhraseBeginPos; i<NewPhraseEndPos; ++i)
                        if (PrevCoverage.testBit(i)){
                            SkipStep = true;
                            break;
                        }
                    if (SkipStep)
                        continue;//TODO if NewPhraseCardinality has not continous place breaK

                    Coverage_t NewCoverage(PrevCoverage);
                    for (size_t i=NewPhraseBeginPos; i<NewPhraseEndPos; ++i)
                        NewCoverage.setBit(i);

                    if (this->conformsIBM1Constraint(NewCoverage) == false){
                        ++PrunedByIBMConstraint;
                        continue;
                    }

                    clsPhraseCandidateCollection& PhraseCandidates =
                            this->Data->PhraseCandidateCollections[NewPhraseBeginPos][NewPhraseCardinality - 1];

                    //There is no rule defined in rule table for current phrase
                    if (PhraseCandidates.isInvalid())
                        continue; //TODO If there are no more places to fill after this startpos break

                    clsLexicalHypothesisContainer& NewLexHypoContainer =
                            this->Data->HypothesisHolder[NewCardinality][NewCoverage];

                    Cost_t RestCost =  this->calculateRestCost(NewCoverage, NewPhraseBeginPos, NewPhraseEndPos);

                    foreach (const clsSearchGraphNode& PrevLexHypoNode, PrevLexHypoContainer.nodes()) {

                        size_t MaxCandidates = qMin((int)PhraseCandidates.usableTargetRuleCount(),
                                                    PhraseCandidates.targetRules().size());
                        bool MustBreak = false;

                        for(size_t i = 0; i<MaxCandidates; ++i){

                            const clsTargetRule& CurrentPhraseCandidate = PhraseCandidates.targetRules().at(i);

                            clsSearchGraphNode NewHypoNode(PrevLexHypoNode,
                                                           NewPhraseBeginPos,
                                                           NewPhraseEndPos,
                                                           NewCoverage,
                                                           CurrentPhraseCandidate,
                                                           IsFinal,
                                                           RestCost);

                            TargomanDebug(7,
                                          "\nNewHypoNode: " <<
                                          "Cardinality: [" << NewCardinality << "] " <<
                                          "Coverage[" << NewCoverage << "] " <<
                                          "Cost: [" << NewHypoNode.getCost() << "] " <<
                                          "RestCost: [" << RestCost << "] " <<
                                          "TargetPhrase: [" << CurrentPhraseCandidate.toStr() << "]\n" <<
                                          "\t\t\tScores: " << NewHypoNode.costElements()
                                          );


                            // If current NewHypoNode is worse than worst stored node ignore it
                            if (clsSearchGraphBuilder::PrunePreInsertion.value() &&
                                NewLexHypoContainer.mustBePruned(NewHypoNode.getTotalCost())){
                                ++PrunedByLexicalHypothesis;
                                MustBreak = true;
                                break;
                            }

                            if(this->Data->HypothesisHolder[NewCardinality].insertNewHypothesis(NewCoverage,
                                                                                                NewLexHypoContainer,
                                                                                                NewHypoNode)) {
                                TargomanDebug(7, "\nNew Hypothesis Inserted.");
                            }
                        }

                        // In case the currently created node is not worth putting in the stack,
                        // its inferiors are definitely are not worth also
                        if(MustBreak)
                                break;

                    }//foreach PrevLexHypoNode
                    if (NewLexHypoContainer.nodes().isEmpty())
                        this->Data->HypothesisHolder[NewCardinality].remove(NewCoverage);

                }//for NewPhraseBeginPos
            }//for PrevCoverageIter
        }//for PrevCardinality
        // Vedadian
        if(NewCardinality == 1)
            exit(0);
    }//for NewCardinality

    Coverage_t FullCoverage;
    FullCoverage.fill(1, this->Data->Sentence.size());

    if(this->Data->HypothesisHolder[this->Data->Sentence.size()].lexicalHypotheses().size() > 0 &&
            this->Data->HypothesisHolder[this->Data->Sentence.size()][FullCoverage].nodes().isEmpty() == false)
    {
        this->Data->GoalNode = &this->Data->HypothesisHolder[this->Data->Sentence.size()][FullCoverage].bestNode();
        this->Data->HypothesisHolder[this->Data->Sentence.size()][FullCoverage].finalizeRecombination();
        return true;
    } else {
        TargomanLogWarn(1, "No translation option for: " << this->Data->Sentence);
        return false;
    }
}

/**
 * @brief Initializes rest cost matrix
 *
 * For every possible range of words of input sentence, finds approximate cost of every feature fucntions, then
 * tries to reduce that computed rest cost if sum of rest cost of splited phrase is less than whole phrase rest cost.
 */
void clsSearchGraphBuilder::initializeRestCostsMatrix()
{
    this->Data->RestCostMatrix.resize(this->Data->Sentence.size());
    for (int SentenceStartPos=0; SentenceStartPos<this->Data->Sentence.size(); ++SentenceStartPos)
        this->Data->RestCostMatrix[SentenceStartPos].fill(
                PBT_MAXIMUM_COST,
                this->Data->Sentence.size() - SentenceStartPos);

    for(size_t FirstPosition = 0; FirstPosition < (size_t)this->Data->Sentence.size(); ++FirstPosition){
        size_t MaxLength = qMin(this->Data->Sentence.size() - FirstPosition,
                                (size_t)this->Data->MaxMatchingSourcePhraseCardinality);
        for(size_t Length = 1; Length <= MaxLength; ++Length){
            this->Data->RestCostMatrix[FirstPosition][Length - 1]  = this->Data->PhraseCandidateCollections[FirstPosition][Length-1].bestApproximateCost();
        }
    }

    for(size_t Length = 2; Length <= (size_t)this->Data->Sentence.size(); ++Length)
        for(size_t FirstPosition = 0; FirstPosition + Length <= (size_t)this->Data->Sentence.size(); ++FirstPosition)
            for(size_t SplitPosition = 1; SplitPosition < Length; ++SplitPosition){
                Cost_t SumSplit = this->Data->RestCostMatrix[FirstPosition][SplitPosition - 1] +
                        this->Data->RestCostMatrix[FirstPosition + SplitPosition][Length - 1 - SplitPosition];
                this->Data->RestCostMatrix[FirstPosition][Length - 1]  =
                        qMin(
                            this->Data->RestCostMatrix[FirstPosition][Length - 1],
                            SumSplit);
            }
}

/**
 * @brief This function approximates rest cost of translation for every feature function.
 * This approximation is based on covered word of transltion and begin and end position of source sentence words.
 * @param _coverage Covered word for translation.
 * @param _beginPos start postion of source sentence.
 * @param _endPos end position of source sentence
 * @note _beginPos and _endPos helps us to infer previous node coverage.
 * @return returns approximate cost of rest cost.
 */

Cost_t clsSearchGraphBuilder::calculateRestCost(const Coverage_t& _coverage, size_t _beginPos, size_t _endPos) const
{
    Cost_t RestCosts = 0.0;
    size_t StartPosition = 0;
    size_t Length = 0;

    for(size_t i=0; i < (size_t)_coverage.size(); ++i)
        if(_coverage.testBit(i) == false){
            if(Length == 0)
                StartPosition = i;
            ++Length;
        }else if(Length){
            RestCosts += this->Data->RestCostMatrix[StartPosition][Length-1];
            Length = 0;
        }
    if(Length)
        RestCosts += this->Data->RestCostMatrix[StartPosition][Length-1];

    if(clsSearchGraphBuilder::DoComputePositionSpecificRestCosts.value()) {
        foreach(FeatureFunction::intfFeatureFunction* FF, gConfigs.ActiveFeatureFunctions.values()) {
            if(FF->canComputePositionSpecificRestCost())
                RestCosts += FF->getRestCostForPosition(_coverage, _beginPos, _endPos);
        }
    }
    return RestCosts;
}

clsPhraseCandidateCollectionData::clsPhraseCandidateCollectionData(size_t _beginPos, size_t _endPos, const clsRuleNode &_ruleNode)
{
    this->TargetRules = _ruleNode.targetRules();
    this->UsableTargetRuleCount = qMin(
                (int)clsPhraseCandidateCollectionData::ObservationHistogramSize.value(),
                this->TargetRules.size()
                );
    this->BestApproximateCost = INFINITY;
    // _observationHistogramSize must be taken care of to not exceed this->TargetRules.size()
    for(int Count = 0; Count < this->UsableTargetRuleCount; ++Count) {
        clsTargetRule& TargetRule = this->TargetRules[Count];
        // Compute the approximate cost for current target rule
        Cost_t ApproximateCost = random() / (double)RAND_MAX;
        foreach (FeatureFunction::intfFeatureFunction* FF , gConfigs.ActiveFeatureFunctions)
            if(FF->canComputePositionSpecificRestCost() == false)
                ApproximateCost += FF->getApproximateCost(_beginPos, _endPos, TargetRule);
        this->BestApproximateCost = qMin(this->BestApproximateCost, ApproximateCost);
    }
}

}
}
}
}




/*************************************************
* TODO PREMATURE OPTIMIZATION that does not work properly
*                 size_t StartLookingPos = 0;
size_t NeededSpace = NewPhraseCardinality;
while(StartLookingPos <= (size_t)this->Data->Sentence.size() - NewPhraseCardinality){
    size_t LookingPos = StartLookingPos;
    while (LookingPos < PrevCoverage.size()) {
        if (PrevCoverage.testBit(LookingPos)){
            NeededSpace = NewPhraseCardinality;
        }else{
            --NeededSpace;
            if(NeededSpace == 0)
                break;
        }
        ++LookingPos;
    }

    if (NeededSpace > 0)
        break;

    NeededSpace = 1;
    StartLookingPos = LookingPos+1;
    size_t NewPhraseBeginPos = LookingPos - NewPhraseCardinality + 1;
    size_t NewPhraseEndPos = NewPhraseBeginPos + NewPhraseCardinality;
*************************************************/