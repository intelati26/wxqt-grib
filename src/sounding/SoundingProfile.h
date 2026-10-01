// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGPROFILE_H
#define SOUNDINGPROFILE_H

#include <string>
#include <vector>

// A vertical sounding (balloon or model column): levels ordered from the lowest
// (highest pressure) upward. Missing values are -9999, exactly as in SPC's files.
// Units: pressure mb, height m MSL, temperature and dewpoint C, wind direction
// degrees, wind speed knots.
class SoundingProfile {
public:
    std::vector<double> pres;
    std::vector<double> hght;
    std::vector<double> tmpc;
    std::vector<double> dwpc;
    std::vector<double> wdir;
    std::vector<double> wspd;
    std::string station;
    double latitude{35.0};   // degrees north of the station / point (for the inferred temperature advection); 35 when unknown
    std::string validTime;   // as printed in the file, e.g. "260930/1200"

    // Reads SPC's sounding text ("%TITLE% ... %RAW% ... %END%"). Returns false if
    // no usable levels were found; `error` then says why.
    static bool parseSpcText(const std::string& text, SoundingProfile& out, std::string& error);

    // Builds the derived arrays; call once after filling the raw arrays.
    void finalize();

    // the lowest level with a valid temperature and dewpoint (the "surface")
    int sfc{0};
    double sfcPres() const { return pres.empty() ? -9999.0 : pres[static_cast<size_t>(sfc)]; }
    double sfcHght() const { return hght.empty() ? -9999.0 : hght[static_cast<size_t>(sfc)]; }
    size_t size() const { return pres.size(); }

    // u / v wind components in knots (meteorological convention: from-direction)
    std::vector<double> u;
    std::vector<double> v;
    std::vector<double> vtmp;   // virtual temperature, C (where T and Td are valid)
    std::vector<double> thetae; // equivalent potential temperature, C (where T and Td are valid)
    std::vector<double> wetbulb;// wet-bulb temperature, C (where T and Td are valid)

    // ---- interpolation (-9999 when p is outside the valid data) ----
    double interpHght(double pMb) const;
    double interpTemp(double pMb) const;
    double interpDwpt(double pMb) const;
    double interpVtmp(double pMb) const;
    double interpThetae(double pMb) const;
    bool interpComponents(double pMb, double& uOut, double& vOut) const;
    double interpPresAtHght(double hMsl) const;   // pressure at a height MSL
    double toAgl(double hMsl) const { return hMsl - sfcHght(); }
    double toMsl(double hAgl) const { return hAgl + sfcHght(); }

private:
    struct Series {
        std::vector<double> logp;
        std::vector<double> value;
    };
    Series tempSeries;
    Series dwptSeries;
    Series vtmpSeries;
    Series thetaeSeries;
    Series hghtSeries;
    Series uSeries;
    Series vSeries;
    std::vector<double> hghtAsc;    // height-ordered copy for height -> pressure
    std::vector<double> logpAtHght;
    static double interpLogP(const Series& series, double pMb);
};

#endif  // SOUNDINGPROFILE_H
