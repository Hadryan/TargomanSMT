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

#include <QStringList>
#include "libTargomanTextProcessor/TextProcessor.h"
#include "clsInput.h"
#include "Private/OOVHandler/OOVHandler.h"

using Targoman::NLPLibs::TargomanTextProcessor;

namespace Targoman {
namespace SMT {
namespace Private {
namespace InputDecomposer {

using namespace Common;
using namespace OOV;

QSet<QString>    clsInputDecomposer::SpecialTags;

Configuration::tmplConfigurable<QString>  clsInputDecomposer::UserDefinedTags(
        clsInputDecomposer::moduleName() + "/UserDefinedTags",
        "User Defined valid XML tags. ",
        "Valid user defined XML tags that must be stored with their attributes."
        "These must not overlap with predefined XML Tags"
        /*TODO add lambda to check overlap*/);
Configuration::tmplConfigurable<bool>    clsInputDecomposer::IsIXML(
        clsInputDecomposer::moduleName() + "/IsIXML",
        "Input is in Plain text(default) or IXML format",
        false);
Configuration::tmplConfigurable<bool>    clsInputDecomposer::DoNormalize(
        clsInputDecomposer::moduleName() + "/DoNormalize",
        "Normalize Input(default) or let it unchanged",
        true);

Configuration::tmplConfigurable<bool>    clsInputDecomposer::TagNameEntities(
        clsInputDecomposer::moduleName() + "/TagNameEntities",
        "Use NER to tag name entities",
        false);

/**
 * @brief clsInput::clsInput Instructor of this class gets input string and based on input arguments parses that.
 * @param _inputStr input string.
 */
clsInputDecomposer::clsInputDecomposer(const QString &_inputStr)
{
    if (this->IsIXML.value()) {
        if (this->DoNormalize.value())
            this->parseRichIXML(_inputStr,true);
        else
            this->parseRichIXML(_inputStr);
    }else{
        this->parsePlain(_inputStr);
    }
}
/**
 * @brief clsInput::init This function inserts userdefined and default tags to #SpecialTags.
 */
void clsInputDecomposer::init(const QString& _configFilePath)
{
    if (clsInputDecomposer::IsIXML.value() == false || clsInputDecomposer::DoNormalize.value())
        TargomanTextProcessor::instance().init(_configFilePath);

    if (UserDefinedTags.value().size())
        foreach(const QString& Tag, UserDefinedTags.value().split(gConfigs.Separator.value()))
            clsInputDecomposer::SpecialTags.insert(Tag);
    for (int i=0; i<Targoman::NLPLibs::enuTextTags::getCount(); i++)
        clsInputDecomposer::SpecialTags.insert(Targoman::NLPLibs::enuTextTags::toStr((Targoman::NLPLibs::enuTextTags::Type)i));

    foreach(const QString& Tag, clsInputDecomposer::SpecialTags){
        WordIndex_t WordIndex = gConfigs.SourceVocab.size() + 1;
        gConfigs.SourceVocab.insert(Tag, WordIndex);
    }
}

/**
 * @brief clsInput::parsePlain This function fisrt converts plain text to iXML format and then parses that.
 * @param _inputStr Input string
 */

void clsInputDecomposer::parsePlain(const QString &_inputStr)
{
    this->NormalizedString =
            TargomanTextProcessor::instance().normalizeText(_inputStr, false, gConfigs.SourceLanguage.value());
    this->parseRichIXML(
                TargomanTextProcessor::instance().text2RichIXML(_inputStr, gConfigs.SourceLanguage.value()), false);
}


void clsInputDecomposer::parseRichIXML(const QString &_inputIXML, bool _normalize)
{
    if (_normalize){
        this->parseRichIXML(
                    TargomanTextProcessor::instance().normalizeText(_inputIXML, false, gConfigs.SourceLanguage.value()));
    }else
        this->parseRichIXML(_inputIXML);
}

/**
 * @brief clsInput::parseRichIXML parses iXML input string and adds detected tokens and their additional informations to #Tokens list.
 * @param _inputIXML Input string.
 */

void clsInputDecomposer::parseRichIXML(const QString &_inputIXML)
{
    if (_inputIXML.contains('<') == false) {
      foreach(const QString& Token, _inputIXML.split(" ", QString::SkipEmptyParts))
          this->newToken(Token);
      return;
    }

    enum enuParsingState{
        Look4Open,
        XMLOpened,
        CollectAttrName,
        Looking4AttrValue,
        CollectAttrValue,
        CollectXMLText,
        Look4Closed,
        XMLClosed,
    }ParsingState = Look4Open;


    QString Token;
    QString TagStr;
    QString TempStr;
    QString AttrName;
    QString AttrValue;
    QVariantMap Attributes;
    bool NextCharEscaped = false;
    int Index = 0;

    foreach(const QChar& Ch, _inputIXML){
        Index++;
        switch(ParsingState){
        case Look4Open:
            if (Ch == '<'){
                if (NextCharEscaped)
                    Token.append(Ch);
                else
                    ParsingState = XMLOpened;
                NextCharEscaped = false;
                continue;
            }
            NextCharEscaped = false;
            if (this->isSpace(Ch)){
                this->newToken(Token);
                Token.clear();
            }else if (Ch == '\\'){
                NextCharEscaped = true;
                Token.append(Ch);
            }else
                Token.append(Ch);
            break;

        case XMLOpened:
            if (this->isSpace(Ch)){
                if (this->SpecialTags.contains(TagStr) == false)
                    throw exInput("Unrecognized Tag Name: <" + TagStr+">");
                ParsingState = CollectAttrName;
            }else if (Ch == '>'){
                if (this->SpecialTags.contains(TagStr) == false)
                    throw exInput("Unrecognized Tag Name: <" + TagStr+">");
                ParsingState = CollectXMLText;
            }else if(Ch.isLetter())
                TagStr.append(Ch);
            else
                throw exInput("Inavlid character '"+QString(Ch)+"' at index: "+ QString::number(Index));
            break;
        case CollectAttrName:
             if (this->isSpace(Ch))
                 continue; //skip spaces untill attrname
             else if (Ch == '=')
                 ParsingState = Looking4AttrValue;
             else if (Ch == '>')
                 ParsingState = CollectXMLText; //No new attribute so collext XML text
             else if (Ch.isLetter())
                 AttrName.append(Ch);
             else
                 throw exInput("Inavlid character '"+QString(Ch)+"' at index: "+ QString::number(Index));
             break;
        case Looking4AttrValue:
            if (this->isSpace(Ch))
                continue; //skip spaces unitl attr value started
            else if (Ch == '"')
                ParsingState = CollectAttrValue;
            else //Short XML tags <b/> are invalid as XML text is obligatory
                throw exInput("Inavlid character '"+QString(Ch)+"' at index: "+ QString::number(Index));
            break;
        case CollectAttrValue:
            if (Ch == '"'){
                if (NextCharEscaped)
                    AttrValue.append(Ch);
                else{
                    if (Attributes.contains(AttrName))
                        throw exInput("Attribute: <"+AttrName+"> Was defined later.");
                    Attributes.insert(AttrName, AttrValue);
                    AttrName.clear();
                    AttrValue.clear();
                    ParsingState = CollectAttrName;
                }
                NextCharEscaped = false;
                continue;
            }
            NextCharEscaped = false;
            if (Ch == '\\')             //zhnDebug: why do we append '\'
                NextCharEscaped = true;
            AttrValue.append(Ch);
            break;
        case CollectXMLText:
            if (Ch == '<'){
                if (NextCharEscaped)
                    Token.append(Ch);
                else
                    ParsingState = Look4Closed;
                NextCharEscaped = false;
                continue;
            }
            NextCharEscaped = false;
            if (Ch == '\\')
                NextCharEscaped = true;
            Token.append(Ch);
            break;

        case Look4Closed:
            if (Ch == '/')
                ParsingState = XMLClosed;
            else
                throw exInput("Inavlid character '"+QString(Ch)+"' at index: "+ QString::number(Index)+" it must be '/'");
            break;
        case XMLClosed:
            if (this->isSpace(Ch))
                continue; //skip Until end of tag
            else if (Ch == '>'){
                if (TempStr != TagStr)
                    throw exInput("Invalid closing tag: <"+TempStr+"> while looking for <"+TagStr+">");
                this->newToken(Token, TagStr, Attributes);

                Token.clear();
                TempStr.clear();
                TagStr.clear();
                Attributes.clear();
                AttrName.clear();
                AttrValue.clear();
                ParsingState = Look4Open;
            }else if (Ch.isLetter())
                TempStr.append(Ch);
            else
                throw exInput("Inavlid character '"+QString(Ch)+"' at index: "+ QString::number(Index));
        }
    }

    switch(ParsingState){
    case Look4Open:
        return;
    case XMLOpened:
    case CollectAttrName:
    case CollectXMLText:
    case Look4Closed:
    case XMLClosed:
        throw exInput("XML Tag: <"+TagStr+"> has not been closed");
    case Looking4AttrValue:
        throw exInput("XML Tag: <"+TagStr+"> Attribute: <"+AttrName+"> has no value");
    case CollectAttrValue:
        throw exInput("XML Tag: <"+TagStr+"> Attribute: <"+AttrName+"> value not closed");
    }
}

/**
 * @brief This function finds wordIndex of input (token or tag string) then inserts token, tag string, attributes and word index to #Tokens.
 *
 * If token is wrapped with a tag, word index of that tag will be find and the token string is not important in finding the word index.
 * If word index of token can not be found using SourceVocab, OOVHandler helps us to allocate a word index for that OOV word.
 * If OOVHandler inform us that this unknown token should be ignored in decoding process we don't add #Tokens.
 *
 * @note: SourceVocab has already been filled in loading phrase table phase.
 * @param _token Input token
 * @param _tagStr If token is wrapped with a tag, this argument inserts string of tag to the function.
 * @param _attrs If token is wrapped with a tag and tag has some attributes, this argument inserts keys and value of those attributes.
 */

void clsInputDecomposer::newToken(const QString &_token, const QString &_tagStr, const QVariantMap &_attrs)
{
    if (_token.isEmpty())
        return;

    WordIndex_t WordIndex;
    QVariantMap Attributes = _attrs;

    if (_tagStr.size())
        WordIndex =  gConfigs.SourceVocab.value(_tagStr);
    else{
        WordIndex = gConfigs.SourceVocab.value(_token, gConfigs.EmptyLMScorer->unknownWordIndex());
        if (WordIndex == gConfigs.EmptyLMScorer->unknownWordIndex()){
            WordIndex = OOVHandler::instance().getWordIndex(_token, Attributes);
            if (Attributes.value(enuDefaultAttrs::toStr(enuDefaultAttrs::NoDecode)).isValid()) //zhnDebug: I think it should be "Attributes.value(..."
                return; // OOVHandler says that I must ignore this word when decoding
        }
    }

    this->Tokens.append(clsToken(_token, WordIndex, _tagStr, Attributes));
}

/**
  TAGGING PROBLEM:
  @tag --> @tag --> Ok

  @tag word --> @tag word | word @tag --> Ok

  @tag --> word --> Ok

  word --> @tag --> ??

  @tag1 --> @tag2  --> ??

  @tag @tag --> @tag [@tag]+ --> ??

  @tag word --> @tag @tag  --> ??

  @tag

  **/
}
}
}
}
