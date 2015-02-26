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

#ifndef TARGOMAN_CORE_PRIVATE_FEATUREFUNCTIONS_LANGUAGEMODEL_H
#define TARGOMAN_CORE_PRIVATE_FEATUREFUNCTIONS_LANGUAGEMODEL_H

#include "Private/FeatureFunctions/intfFeatureFunction.hpp"

namespace Targoman {
namespace Core {
namespace Private {
namespace FeatureFunction{

TARGOMAN_ADD_EXCEPTION_HANDLER(exLanguageModel, exFeatureFunction);


class LanguageModel : public intfFeatureFunction
{
public:
    ~LanguageModel(){}

    void initialize(const QString &){}

    Common::Cost_t scoreSearchGraphNode(SearchGraphBuilder::clsSearchGraphNode& _newHypothesisNode) const;

    Common::Cost_t getRestCostForPosition(const Coverage_t& _coverage, size_t _beginPos, size_t endPos) const {
        Q_UNUSED(_coverage);
        Q_UNUSED(_beginPos);
        Q_UNUSED(endPos);
        return 0;
    }

    inline Common::Cost_t getApproximateCost(unsigned _sourceStart,
                                      unsigned _sourceEnd,
                                      const RuleTable::clsTargetRule& _targetRule) const;

    bool nodesHaveSameState(const SearchGraphBuilder::clsSearchGraphNode &_first,
                            const SearchGraphBuilder::clsSearchGraphNode &_second) const;

    inline QStringList columnNames() const{return QStringList();}

private:
    LanguageModel():
        intfFeatureFunction(this->moduleName(), false)
    {}
    TARGOMAN_DEFINE_SINGLETONMODULE("FeatureFunctions/LanguageModel", LanguageModel)

private:
    static Common::Configuration::tmplConfigurable<double> ScalingFactor;
};


}
}
}
}


#endif // TARGOMAN_CORE_PRIVATE_FEATUREFUNCTIONS_LANGUAGEMODEL_H