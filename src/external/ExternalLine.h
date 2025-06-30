 /*
#  Copyright 2013-present Roman Kushnarenko
#  Licensed under the Apache License, Version 2.0 (the "License")
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  http:#www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  https:#github.com/sromku/polygon-contains-point
#  */

#ifndef EXTERNALLINE_H
#define EXTERNALLINE_H

#include "external/ExternalPoint.h"

class ExternalLine {
public:
    ExternalLine(ExternalPoint, ExternalPoint);
    bool isInside(ExternalPoint) const;
    bool isVertical() const;
    double getA() const;
    double getB() const;
    ExternalPoint getStart() const;
    ExternalPoint start;
    ExternalPoint end;

private:
    bool vertical{false};
    double a{999999999.0};
    double b{999999999.0};
};

#endif  // EXTERNALLINE_H
