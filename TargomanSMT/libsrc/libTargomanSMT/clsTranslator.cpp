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

#include "clsTranslator.h"

#include "Private/clsTranslator_p.h"
#include "libTargomanTextProcessor/TextProcessor.h"
#include "Private/OOVHandler/OOVHandler.h"


namespace Targoman{
/**
 * @brief Base namespace of TargomanSMT library surrounding all other namespaces
 */
namespace SMT {

using namespace Private;
using namespace NLPLibs;
using namespace Private::OOV;
using namespace SearchGraph;

bool clsTranslatorPrivate::Initialized = false;

clsTranslator::clsTranslator(const QString &_inputStr) :
    pPrivate(new Private::clsTranslatorPrivate(_inputStr))
{
    TargomanDebug(5,_inputStr);
}

clsTranslator::~clsTranslator()
{
    ///@note Just to suppress compiler error using QScoppedPointer
}

void clsTranslator::init(const QString _configFilePath)
{
    if (clsTranslatorPrivate::Initialized){
        TargomanWarn(5, "Reinitialization of translator has no effect");
        return;
    }

    OOVHandler::instance().initialize();
    InputDecomposer::clsInputDecomposer::init(_configFilePath);
    gConfigs.EmptyLMScorer.reset(gConfigs.LM.getInstance<Proxies::intfLMSentenceScorer>());
    gConfigs.EmptyLMScorer->init(false);

    SearchGraph::clsSearchGraphBuilder::init(_configFilePath);

    clsTranslatorPrivate::Initialized = true;
}

stuTranslationOutput clsTranslator::translate(bool _justTranslationString)
{
    if (clsTranslatorPrivate::Initialized == false)
        throw exTargomanCore("Translator is not initialized");

    //Input was decomposed in constructor
    this->pPrivate->SearchGraphBuilder->collectPhraseCandidates();
    this->pPrivate->SearchGraphBuilder->decode();

    if (_justTranslationString){
        stuTranslationOutput Output;
        Output.Translation = this->pPrivate->Output->translationString();
        return Output;
    }else
        return this->pPrivate->Output->translationOutput();
}

void clsTranslator::saveBinaryRuleTable(const QString &_filePath)
{
    clsSearchGraphBuilder::saveBinaryRuleTable(_filePath);
}

}
}
