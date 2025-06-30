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

#ifndef EXTERNALPOINT_H
#define EXTERNALPOINT_H

class ExternalPoint {
public:
    ExternalPoint();
    ExternalPoint(double, double);
    double x{};
    double y{};
};

#endif  // EXTERNALPOINT_H
