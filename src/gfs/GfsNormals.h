// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSNORMALS_H
#define GFSNORMALS_H

#include <array>
#include <string>

// NOAA's U.S. daily gridded climate normals (1991-2020, NCEI accession 0259962, 1/24 degree, about 5 km, the contiguous states) for the daily high and low. The source is a file for each
// day of a 366 day year (0.87 MB each); the normal is a smooth seasonal curve, so the first time it is wanted 15 of them (every 26th day, 13 MB) are fetched and a mean and six sine / cosine pairs
// are fitted to each land cell. Only those 13 numbers per cell are kept for good (about 2.6 MB, the "climate" folder), and the 15 days are dropped: every day of the year is then within 0.1 F rms
// (0.7 F at the worst) of the NCEI value. See docs/normals-storage.md. Outside the contiguous states there is no value (NaN): the caller keeps the coarser reanalysis normal there.
class GfsNormals {
public:
    enum class Kind { High, Low };
    static constexpr int terms = 13;   // 1 + 2 * 6 harmonics

    // the shared copy (the coefficients are 25 MB in memory, read once)
    static const GfsNormals& shared();

    // Makes sure the coefficients are on hand: read from the data folder, or built from the web (blocking: 15 small downloads, a few seconds to a minute) the first time. false with the reason
    // when they can not be had (no connection, say): the callers then use the reanalysis normal.
    bool prepare(Kind kind, std::string& error) const;
    // true when the coefficients are already on disk or in memory: nothing is fetched to answer
    bool ready(Kind kind) const;

    // The seasonal basis of a date (month 1-12, day 1-31; 29 Feb is itself: the source calendar always has it), so a grid of cells costs 13 multiplications each
    struct Day {
        std::array<double, terms> basis{};
    };
    static Day day(int month, int dayOfMonth);

    // The normal in degrees F at the place, NaN outside the contiguous states or when prepare() has not succeeded
    double at(Kind kind, const Day& day, double lon, double lat) const;

    // the helpers the build and the tests use
    static int dayOfLeapYear(int month, int dayOfMonth);   // 1..366
    struct Fit {
        // coefficients of one cell from its 15 sample values (degrees F)
        static std::array<double, terms> of(const std::array<double, 15>& samples);
        static std::array<int, 15> sampleDays();
    };

private:
    GfsNormals() = default;
};

#endif  // GFSNORMALS_H
