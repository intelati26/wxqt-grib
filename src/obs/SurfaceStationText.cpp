// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/SurfaceStationText.h"
#include <cmath>
#include <ctime>
#include <QLocale>
#include "hurricane/UtilityHdob.h"

namespace {
    QString number(double v, int digits = 0) {
        return QLocale{QLocale::English}.toString(v, 'f', digits);
    }

    QString ageText(long seconds) {
        const long age = static_cast<long>(std::time(nullptr)) - seconds;
        if (age < 90) {
            return "just now";
        }
        if (age < 5400) {
            return QString::number(age / 60) + " min ago";
        }
        return number(static_cast<double>(age) / 3600.0, 1) + " h ago";
    }

    QString windText(const SurfaceStation& s) {
        if (!SurfaceStation::has(s.windSpeed)) {
            return "no wind reported";
        }
        if (s.windSpeed < 0.5) {
            return "calm";
        }
        QString text = SurfaceStation::has(s.windDirection) ? QString::number(static_cast<int>(std::lround(s.windDirection)), 10).rightJustified(3, '0') + " deg at " : QString{"variable at "};
        text += number(s.windSpeed) + " kt";
        if (SurfaceStation::has(s.windGust) && s.windGust > s.windSpeed + 0.5) {
            text += ", gusts " + number(s.windGust) + " kt";
        }
        return text;
    }
}

namespace SurfaceStationText {

QString summary(const SurfaceStation& s, bool fahrenheit) {
    const auto temperature = [fahrenheit] (double c) { return number(fahrenheit ? c * 1.8 + 32.0 : c) + (fahrenheit ? " F" : " C"); };
    QString text = QString::fromStdString(s.id + (s.name.empty() ? "" : "  " + s.name + (s.state.empty() ? "" : ", " + s.state)) + "   [" + s.network + "]");
    text += "\n" + QString::fromStdString(UtilityHdob::timeText(s.seconds)) + "  (" + ageText(s.seconds) + ")";
    text += "\nWind " + windText(s);
    if (SurfaceStation::has(s.temperature)) {
        text += "\nTemperature " + temperature(s.temperature);
        if (SurfaceStation::has(s.dewPoint)) {
            text += ", dew point " + temperature(s.dewPoint);
        }
    }
    if (SurfaceStation::has(s.altimeter)) {
        text += "\nAltimeter " + number(s.altimeter, 2) + " inHg";
    }
    if (!s.flight.empty()) {
        text += "   " + QString::fromStdString(s.flight);
    }
    return text;
}

QString details(const SurfaceStation& s) {
    const auto both = [] (double c) { return number(c * 1.8 + 32.0, 1) + " F  (" + number(c, 1) + " C)"; };
    QString text = QString::fromStdString(s.id + (s.name.empty() ? "" : "  -  " + s.name)) + "\n\n";
    text += "Network:        " + QString::fromStdString(s.network) + (s.airport ? "  (airport METAR)" : "") + "\n";
    if (!s.state.empty()) {
        text += "State:          " + QString::fromStdString(s.state) + "\n";
    }
    text += "Position:       " + number(s.lat, 4) + ", " + number(s.lon, 4);
    if (SurfaceStation::has(s.elevation)) {
        text += "     elevation " + number(s.elevation) + " m  (" + number(s.elevation * 3.28084) + " ft)";
    }
    text += "\nObserved:       " + QString::fromStdString(UtilityHdob::timeText(s.seconds)) + "  (" + ageText(s.seconds) + ")\n\n";
    text += "Wind:           " + windText(s);
    if (SurfaceStation::has(s.windSpeed)) {
        text += "   (" + number(s.windSpeed * 1.15078) + " mph)";
    }
    text += "\n";
    if (SurfaceStation::has(s.temperature)) {
        text += "Temperature:    " + both(s.temperature) + "\n";
    }
    if (SurfaceStation::has(s.dewPoint)) {
        text += "Dew point:      " + both(s.dewPoint) + "\n";
    }
    if (SurfaceStation::has(s.humidity)) {
        text += "Humidity:       " + number(s.humidity) + " %\n";
    }
    if (SurfaceStation::has(s.altimeter)) {
        text += "Altimeter:      " + number(s.altimeter, 2) + " inHg  (" + number(s.altimeter * 33.8639, 1) + " mb)\n";
    }
    if (SurfaceStation::has(s.seaLevel)) {
        text += "Sea level:      " + number(s.seaLevel, 1) + " mb\n";
    }
    if (SurfaceStation::has(s.visibility)) {
        text += "Visibility:     " + number(s.visibility, 1) + " mi\n";
    }
    if (!s.weather.empty()) {
        text += "Weather:        " + QString::fromStdString(s.weather) + "\n";
    }
    if (!s.sky.empty()) {
        text += "Sky:            " + QString::fromStdString(s.sky) + "\n";
    }
    if (!s.flight.empty()) {
        text += "Flight category: " + QString::fromStdString(s.flight) + "\n";
    }
    if (s.quality != ' ' && s.quality != '\0') {
        text += QString{"Quality check:  "} + s.quality + "  (MADIS: V verified, S screened, C coarse pass, Q questioned)\n";
    }
    if (!s.raw.empty()) {
        text += "\n" + QString::fromStdString(s.raw) + "\n";
    }
    text += s.airport ? "\nAs reported." : "\nAs reported; the mesonet and citizen stations are not all of the same quality.";
    return text;
}

}  // namespace SurfaceStationText
