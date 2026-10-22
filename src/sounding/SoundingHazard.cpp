// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingHazard.h"
#include <cmath>
#include <functional>
#include <QString>
#include "sounding/SoundingPrecip.h"

namespace {
    using namespace SoundingHazard;
    constexpr double missing = -9999.0;
    bool have(double v) { return v > -9998.0 && std::isfinite(v); }

    std::string number(double v, int digits) {
        return have(v) ? QString::number(v, 'f', digits).toStdString() : std::string{"--"};
    }

    // one test on a value: Unknown when the sounding has no value
    Condition test(const std::string& text, double value, int digits, const std::function<bool(double)>& rule) {
        Condition c;
        c.text = text;
        c.value = number(value, digits);
        c.state = !have(value) ? State::Unknown : rule(value) ? State::Pass : State::Fail;
        return c;
    }

    // "a or b": a line holding two or three tests, true when any holds (unknown when none holds and one is unknown)
    Condition anyOf(const std::string& text, const std::vector<Condition>& parts) {
        Condition c;
        c.text = text;
        bool unknown = false;
        std::string values;
        for (const auto& p : parts) {
            values += (values.empty() ? "" : " / ") + p.value;
            if (p.state == State::Pass) {
                c.state = State::Pass;
            } else if (p.state == State::Unknown) {
                unknown = true;
            }
        }
        if (c.state != State::Pass) {
            c.state = unknown ? State::Unknown : State::Fail;
        }
        c.value = values;
        return c;
    }
    Condition allOf(const std::string& text, const std::vector<Condition>& parts) {
        Condition c;
        c.text = text;
        bool unknown = false, failed = false;
        std::string values;
        for (const auto& p : parts) {
            values += (values.empty() ? "" : " / ") + p.value;
            failed = failed || p.state == State::Fail;
            unknown = unknown || p.state == State::Unknown;
        }
        c.state = failed ? State::Fail : unknown ? State::Unknown : State::Pass;
        c.value = values;
        return c;
    }

    bool holds(const Node& node) {
        for (const auto& line : node.lines) {
            if (line.state != State::Pass) {
                return false;
            }
        }
        return true;
    }

    // the NWS heat index (Rothfusz regression with its two adjustments) from the temperature in F and the relative humidity in per cent
    double heatIndex(double tF, double rh) {
        if (!have(tF) || !have(rh)) {
            return missing;
        }
        double simple = 0.5 * (tF + 61.0 + (tF - 68.0) * 1.2 + rh * 0.094);
        if ((simple + tF) / 2.0 < 80.0) {
            return simple;
        }
        double hi = -42.379 + 2.04901523 * tF + 10.14333127 * rh - 0.22475541 * tF * rh - 0.00683783 * tF * tF - 0.05481717 * rh * rh + 0.00122874 * tF * tF * rh + 0.00085282 * tF * rh * rh -
                    0.00000199 * tF * tF * rh * rh;
        if (rh < 13.0 && tF >= 80.0 && tF <= 112.0) {
            hi -= (13.0 - rh) / 4.0 * std::sqrt((17.0 - std::fabs(tF - 95.0)) / 17.0);
        } else if (rh > 85.0 && tF >= 80.0 && tF <= 87.0) {
            hi += (rh - 85.0) / 10.0 * (87.0 - tF) / 5.0;
        }
        return hi;
    }
}

SoundingHazard::Result SoundingHazard::classify(const SoundingProfile& p, const SoundingAnalysis& a, const Input& input) {
    Result out;
    // the parameters, in the units of the chart
    const double stpE = a.stpEffective, stpF = a.stpFixed, srh01 = a.srh01, esrh = a.effectiveSrh;
    const double srw46 = a.kin46.stormRelative.speed(), bwd08 = a.kin8.shear.speed();
    const double sbLcl = a.sb.valid ? a.sb.lclHght : missing, mlLcl = a.ml.valid ? a.ml.lclHght : missing;
    const double lr01 = SoundingIndices::lapseRateAgl(p, 0.0, 1000.0);
    const double mlCin = a.ml.valid ? a.ml.cin : missing, muCin = a.mu.valid ? a.mu.cin : missing;
    const double inflowBase = a.effective.valid ? a.effective.botAgl : missing;
    const double rhMean = have(a.lowRh) && have(a.midRh) ? (a.lowRh + a.midRh) / 2.0 : missing;
    const double scp = a.supercell, ship = a.hail, wndg = a.wndg, dcape = a.dcape, sigSvr = a.sigSevere, mmp = a.mmp;
    const double upshear = a.corfidi.valid() ? a.corfidi.upshear.speed() : missing;
    const size_t sfc = static_cast<size_t>(p.sfc);
    const double sfcWindMph = sfc < p.wspd.size() && have(p.wspd[sfc]) ? p.wspd[sfc] * 1.15078 : missing;
    const double sfcTempC = sfc < p.tmpc.size() ? p.tmpc[sfc] : missing;
    const double heat = heatIndex(have(sfcTempC) ? sfcTempC * 1.8 + 32.0 : missing, a.surfaceRh);
    const auto precip = SoundingPrecip::bestGuess(p);
    const double pw = a.precipitableWaterIn;
    const bool pwKnown = have(input.pwMeanIn) && have(input.pwSigmaIn);
    out.pwKnown = pwKnown;

    const auto ge = [] (double limit) { return [limit] (double v) { return v >= limit; }; };
    const auto gt = [] (double limit) { return [limit] (double v) { return v > limit; }; };
    const auto lt = [] (double limit) { return [limit] (double v) { return v < limit; }; };
    const auto le = [] (double limit) { return [limit] (double v) { return v <= limit; }; };
    const auto base = [&] {
        if (!a.effective.valid) {   // no effective inflow layer (not enough CAPE, too much CIN): the test cannot hold
            Condition c;
            c.text = "Effective inflow base = 0 m AGL";
            c.value = "no layer";
            c.state = State::Fail;
            return c;
        }
        return test("Effective inflow base = 0 m AGL", inflowBase, 0, [] (double v) { return v <= 0.5; });
    };

    // ---- the tornado column
    auto make = [] (const char * id, const char * outcome, std::vector<Condition> lines) {
        Node n;
        n.id = id;
        n.outcome = outcome;
        n.lines = std::move(lines);
        return n;
    };
    std::vector<Node> left, right;
    left.push_back(make("L1", "PDS TOR", {test("STPE >= 3", stpE, 1, ge(3)), test("STPF >= 3", stpF, 1, ge(3)), test("SRH 0-1 km >= 200 m2/s2", srh01, 0, ge(200)), test("ESRH >= 200 m2/s2", esrh, 0, ge(200)),
                                         test("SRW 4-6 km >= 15 kt", srw46, 0, ge(15)), test("BWD 0-8 km > 45 kt", bwd08, 0, gt(45)), test("SBLCL < 1000 m", sbLcl, 0, lt(1000)),
                                         test("MLLCL < 1200 m", mlLcl, 0, lt(1200)), test("LR 0-1 km >= 5 C/km", lr01, 1, ge(5)), test("MLCINH > -50 J/kg", mlCin, 0, gt(-50)), base()}));
    left.push_back(make("L2", "TOR", {anyOf("STPE >= 3 or STPF >= 4", {test("STPE >= 3", stpE, 1, ge(3)), test("STPF >= 4", stpF, 1, ge(4))}), test("MLCINH > -125 J/kg", mlCin, 0, gt(-125)), base()}));
    left.push_back(make("L3", "TOR", {anyOf("STPE >= 1 or STPF >= 1", {test("STPE >= 1", stpE, 1, ge(1)), test("STPF >= 1", stpF, 1, ge(1))}),
                                      anyOf("SRW 4-6 km >= 15 kt or BWD 0-8 km >= 40 kt", {test("SRW", srw46, 0, ge(15)), test("BWD", bwd08, 0, ge(40))}), test("MLCINH > -50 J/kg", mlCin, 0, gt(-50)), base()}));
    left.push_back(make("L4", "TOR", {anyOf("STPE >= 1 or STPF >= 1", {test("STPE >= 1", stpE, 1, ge(1)), test("STPF >= 1", stpF, 1, ge(1))}), test("(LowRH + MidRH) / 2 >= 60 %", rhMean, 0, ge(60)),
                                      test("LR 0-1 km >= 5 C/km", lr01, 1, ge(5)), test("MLCINH > -50 J/kg", mlCin, 0, gt(-50)), base()}));
    left.push_back(make("L5", "MRGL TOR", {anyOf("STPE >= 1 or STPF >= 1", {test("STPE >= 1", stpE, 1, ge(1)), test("STPF >= 1", stpF, 1, ge(1))}), test("MLCINH > -150 J/kg", mlCin, 0, gt(-150)), base()}));
    left.push_back(make("L6", "MRGL TOR", {anyOf("(STPE >= 1 and ESRH >= 150) or (STPF >= 0.5 and SRH 0-1 km >= 150)",
                                                {allOf("STPE >= 1 and ESRH >= 150", {test("STPE", stpE, 1, ge(1)), test("ESRH", esrh, 0, ge(150))}),
                                                 allOf("STPF >= 0.5 and SRH01 >= 150", {test("STPF", stpF, 1, ge(0.5)), test("SRH01", srh01, 0, ge(150))})}),
                                         test("MLCINH > -50 J/kg", mlCin, 0, gt(-50)), base()}));
    // ---- the other column, from the bottom up
    right.push_back(make("R1", "SVR", {anyOf("STPF >= 1 or SCP >= 4 or STPE >= 1", {test("STPF", stpF, 1, ge(1)), test("SCP", scp, 1, ge(4)), test("STPE", stpE, 1, ge(1))}), test("MUCINH >= -50 J/kg", muCin, 0, ge(-50))}));
    right.push_back(make("R2", "SVR", {anyOf("(SCP >= 2 and SHIP >= 1) or DCAPE >= 750 J/kg", {allOf("SCP >= 2 and SHIP >= 1", {test("SCP", scp, 1, ge(2)), test("SHIP", ship, 1, ge(1))}), test("DCAPE", dcape, 0, ge(750))}),
                                       test("MUCINH >= -50 J/kg", muCin, 0, ge(-50))}));
    right.push_back(make("R3", "SVR", {test("SigSvr >= 30000 m3/s3", sigSvr, 0, ge(30000)), test("MMP >= 0.6", mmp, 2, ge(0.6)), test("MUCINH >= -50 J/kg", muCin, 0, ge(-50))}));
    right.push_back(make("R4", "MRGL SVR", {anyOf("WNDG >= 0.5 or SHIP >= 0.5 or SCP >= 0.5", {test("WNDG", wndg, 1, ge(0.5)), test("SHIP", ship, 1, ge(0.5)), test("SCP", scp, 1, ge(0.5))}),
                                           test("MUCINH >= -75 J/kg", muCin, 0, ge(-75))}));
    {
        Condition pwTest;
        pwTest.text = "PW > climatological mean + 2 sigma";
        if (pwKnown && have(pw)) {
            pwTest.value = number(pw, 2) + " in (limit " + number(input.pwMeanIn + 2.0 * input.pwSigmaIn, 2) + ")";
            pwTest.state = pw > input.pwMeanIn + 2.0 * input.pwSigmaIn ? State::Pass : State::Fail;
        } else {
            pwTest.value = have(pw) ? number(pw, 2) + " in (no climatology here)" : "--";
            pwTest.state = State::Unknown;
        }
        right.push_back(make("R5", "FLASH FLOOD", {pwTest, test("Corfidi upshear speed < 25 kt", upshear, 0, lt(25))}));
    }
    right.push_back(make("R6", "BLIZZARD", {test("Surface wind > 35 mph", sfcWindMph, 0, gt(35)), test("Surface temperature <= 0 C", sfcTempC, 1, le(0)),
                                           [&] {
                                               Condition c;
                                               c.text = "Bourgouin algorithm indicates snow";
                                               c.value = precip.type;
                                               c.state = precip.type == "Snow" || precip.type == "Sleet and Snow" ? State::Pass : (precip.type.empty() || precip.type == "Unknown" ? State::Unknown : State::Fail);
                                               return c;
                                           }()}));
    right.push_back(make("R7", "EXCESSIVE HEAT", {test("Surface heat index > 105 F", heat, 0, gt(105))}));

    // ---- the walk: the tornado column from the top, then the other column from the bottom; the first node that holds gives its outcome
    for (auto * column : {&left, &right}) {
        for (auto& node : *column) {
            node.reached = out.type == "NONE";
            if (!node.reached) {
                out.nodes.push_back(node);
                continue;
            }
            node.passed = holds(node);
            if (node.passed) {
                out.type = node.outcome;
            }
            out.nodes.push_back(node);
        }
    }
    return out;
}
