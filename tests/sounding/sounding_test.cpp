// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// Regression test for src/sounding/: every fixture is an SPC observed-sounding text file, which carries SPC's
// own computed values after the raw levels. The engine's results must stay within the tolerances below of those
// values. Tolerances are the engine's measured accuracy over 270 soundings (see docs/sharppy-notice.md), not
// aspirations; where SPC differs from SHARPpy, the engine follows SPC and the fixtures guard that.
//
// usage: sounding_test <fixture.txt>...   exit status 1 if any comparison is out of tolerance
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "sounding/SoundingAnalysis.h"
#include "sounding/SoundingProfile.h"

namespace {
    constexpr double knotsToMs = 0.514444;
    constexpr double missing = -9999.0;

    // first number after `key` in the text (-9999 if the key is absent)
    double after(const std::string& text, const std::string& key, size_t from = 0) {
        const auto at = text.find(key, from);
        return at == std::string::npos ? missing : std::atof(text.c_str() + at + key.size());
    }

    struct Block {
        double cape{0}, cin{0}, cape3{0}, lclP{missing}, lclH{missing}, lfcP{missing}, elP{missing}, li{missing};
    };

    // SPC's block for one parcel kind ("SB", "ML", "MU"); starts at its "***" heading
    Block spcBlock(const std::string& text, const std::string& heading, const std::string& prefix) {
        Block b;
        const auto start = text.find(heading);
        if (start == std::string::npos) return b;
        const auto end = text.find("----- Misc", start);
        const auto part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        b.cape = after(part, prefix + "CAPE:");
        b.cin = after(part, prefix + "CINH:");
        b.cape3 = after(part, "3km " + prefix + "CAPE:");
        b.li = after(part, prefix + "LI:");
        auto level = [&](const char* name, double& p, double* h) {
            const auto at = part.find(prefix + name + ":");
            if (at == std::string::npos) return;
            p = std::atof(part.c_str() + at + prefix.size() + std::string{name}.size() + 1);
            if (h) {
                const auto m = part.find("mb", at);
                if (m != std::string::npos) *h = std::atof(part.c_str() + m + 2);
            }
        };
        level("LCL", b.lclP, &b.lclH);
        level("LFC", b.lfcP, nullptr);
        level("EL", b.elP, nullptr);
        return b;
    }

    int failures = 0;
    int checks = 0;
    std::string current;

    void check(const char* what, double mine, double spc, double tol) {
        if (spc <= -9998.0 && mine <= -9998.0) return;   // both say "not defined"
        checks += 1;
        const bool presenceDiffers = (spc <= -9998.0) != (mine <= -9998.0);
        if (presenceDiffers || std::fabs(mine - spc) > tol) {
            failures += 1;
            std::printf("FAIL %-18s %-22s mine %10.2f  spc %10.2f  (tol %.2f)\n", current.c_str(), what, mine, spc, tol);
        }
    }

    void parcel(const char* name, const SoundingParcel::Parcel& p, const Block& s) {
        const std::string n = name;
        check((n + " CAPE").c_str(), p.cape, s.cape, std::max(3.0, 0.01 * s.cape));
        check((n + " CIN").c_str(), p.cin, s.cin, std::max(3.0, 0.02 * std::fabs(s.cin)));
        check((n + " 3km CAPE").c_str(), p.cape3km, s.cape3, std::max(8.0, 0.1 * s.cape3));
        check((n + " LCL mb").c_str(), p.lclPres, s.lclP, 1.0);
        check((n + " LFC mb").c_str(), p.lfcPres, s.lfcP, 6.0);
        check((n + " EL mb").c_str(), p.elPres, s.elP, 6.0);
        check((n + " LI500").c_str(), p.liftedIndex500, s.li, 1.0);
    }
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i += 1) {
        std::ifstream file{argv[i]};
        std::stringstream buffer;
        buffer << file.rdbuf();
        const auto text = buffer.str();
        current = argv[i];
        current = current.substr(current.rfind('/') == std::string::npos ? 0 : current.rfind('/') + 1);
        SoundingProfile profile;
        std::string error;
        if (!SoundingProfile::parseSpcText(text, profile, error)) {
            std::printf("FAIL %s: cannot parse (%s)\n", current.c_str(), error.c_str());
            failures += 1;
            continue;
        }
        const auto a = SoundingAnalysis::compute(profile);
        parcel("SB", a.sb, spcBlock(text, "*** SFC PARCEL ***", "SB"));
        parcel("ML", a.ml, spcBlock(text, "*** 100mb MIXED LAYER PARCEL ***", "ML"));
        parcel("MU", a.mu, spcBlock(text, "*** MU PARCEL IN LOWEST 400mb ***", "MU"));

        // kinematics (SPC prints integers: tolerance covers rounding)
        check("0-1 km shear kt", a.shear01.speed(), after(text, "0-1 km BWD"), 1.0);
        check("0-6 km shear kt", a.shear06.speed(), after(text, "0-6 km BWD"), 1.0);
        check("0-6 mean wind kt", a.meanWind06.speed(), after(text, "0-6 km mean wind"), 1.0);
        {
            const auto at = text.find("Bunkers storm motion");
            if (at != std::string::npos) {
                const char* s = text.c_str() + at + 20;
                char* end = nullptr;
                const double dir = std::strtod(s, &end);
                const double spd = std::strtod(end + 1, nullptr);
                double dd = std::fabs(a.rightMover.direction() - dir);
                if (dd > 180.0) dd = 360.0 - dd;
                checks += 1;
                if (spd >= 8.0 && dd > 5.0) {   // direction is meaningless for a near-calm storm motion
                    failures += 1;
                    std::printf("FAIL %-18s Bunkers direction      mine %10.2f  spc %10.2f\n", current.c_str(), a.rightMover.direction(), dir);
                }
                check("Bunkers speed (kt)", a.rightMover.speed(), spd, 2.0);
            }
        }
        check("0-1 km SRH", a.srh01, after(text, "0-1 km SRH"), 3.0);
        check("0-3 km SRH", a.srh03, after(text, "0-3 km SRH"), 3.0);

        // thermodynamic odds and ends
        check("PW in", a.precipitableWaterIn, after(text, "Precip Water:"), 0.02);
        check("low-100mb mean W", a.meanMixingLow100, after(text, "0-1 km mean W:"), 0.06);
        check("0-3 km mean W", a.meanMixing03, after(text, "0-3 km mean W:"), 0.06);
        check("sfc RH %", a.surfaceRh, after(text, "SFC RH:"), 1.0);
        check("DCAPE", a.dcape, after(text, "DCAPE:"), 3.0);
        check("Conv temp C", a.convectiveTemp, after(text, "Conv Temp:"), 1.6);
        {   // lapse-rate lines hold "<dT> C  <rate> C/km"
            auto rate = [&](const char* key) {
                const auto at = text.find(key);
                if (at == std::string::npos) return missing;
                char* end = nullptr;
                std::strtod(text.c_str() + at + std::string{key}.size(), &end);
                return std::strtod(end + 2, nullptr);
            };
            check("lapse 0-3 km", a.lapse03, rate("0-3 km      "), 0.1);
            check("lapse 3-6 km MSL", a.lapse36, rate("3-6 km      "), 0.2);
            check("lapse 700-500", a.lapse700500, rate("700-500mb   "), 0.1);
        }
        // composites: SCP / STP follow SPC to a small absolute error (SHIP is known to differ, see the notice)
        check("eff SCP", a.supercell, after(text, "Effective-layer SCP"), 0.15);
        check("eff STP", a.stpEffective, after(text, "Effective-layer STP"), 0.1);
        check("fixed STP", a.stpFixed, after(text, "Fixed-layer STP"), 0.15);
        check("eff SRH", a.effectiveSrh, after(text, "Effective SRH"), 3.0);
    }
    std::printf("%d comparisons over %d soundings, %d out of tolerance\n", checks, argc - 1, failures);
    return failures == 0 ? 0 : 1;
}
