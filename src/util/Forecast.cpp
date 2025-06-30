// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Forecast.h"

Forecast::Forecast(
    const string& name,
    const string& temperature,
    const string& icon,
    const string& shortForecast,
    const string& detailedForecast
)
    : name{name}
    , temperature{temperature}
    , icon{icon}
    , shortForecast{shortForecast}
    , detailedForecast{detailedForecast}
{}
