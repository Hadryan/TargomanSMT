/******************************************************************************
 * Targoman: A robust Statistical Machine Translation framework               *
 *                                                                            *
 * Copyright 2014-2015 by ITRC <http://itrc.ac.ir>                            *
 *                                                                            *
 * This file is part of Targoman.                                             *
 *                                                                            *
 * Targoman is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Targoman is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Lesser General Public License for more details.                        *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with Targoman. If not, see <http://www.gnu.org/licenses/>.           *
 *                                                                            *
 ******************************************************************************/
/**
 * @author S. Mohammad M. Ziabary <ziabary@targoman.com>
 */

#include "Configs.h"

namespace Targoman {
namespace Apps {

using namespace Common;
using namespace Common::Configuration;

tmplConfigurable<QString>     gConfigs::InputFile(
        gConfigs::appConfig("InputFile"),
        "Input file path to convert",
        "",
        Validators::tmplPathAccessValidator<
            (enuPathAccess::Type)(enuPathAccess::File | enuPathAccess::Readable),
            false>,
        "f",
        "FILE_PATH",
        "input-file");

tmplConfigurable<QString>     gConfigs::InputText(
        gConfigs::appConfig("InputText"),
        "Input Text to translate",
        "",
        ReturnTrueCrossValidator,
        "i",
        "TEXT",
        "input-text");

tmplConfigurable<QString>     gConfigs::OutputFile(
        gConfigs::appConfig("Output"),
        "output path to write translation",
        "",
        Validators::tmplPathAccessValidator<
            (enuPathAccess::Type)(enuPathAccess::File | enuPathAccess::Writeatble),
            false>,
        "o",
        "FILE_PATH",
        "output-file");

tmplConfigurable<quint8>     gConfigs::MaxThreads(
        gConfigs::appConfig("MaxThreads"),
        "Maximum Concurrent Translations",
        5,
        Validators::tmplNumericValidator<quint8, 0, 255>,
        "t",
        "MAX_THREADS",
        "max-threads");

}
}
