// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The National Blend of Models text bulletins (misc/NbmText): the parse of real blocks of KOKC (NBS, NBE, NBX: tests/gfs/nbm/), the merge of the three, and the one-station finder over range reads on a
// stand-in file with several sorted runs. With NBM_FILE_<KIND>=<path of a whole bulletin file> (the real ones: blend_nbxtx.t06z ...) the finder is also tried on EVERY station of that file, which is what
// the plan asked for (the stand-in passed while 41 stations of the real file failed). Usage: nbmtext_test <folder of the fixtures>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include "misc/NbmText.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    std::string slurp(const std::string& path) {
        std::ifstream in{path, std::ios::binary};
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }
    // a stand-in file: blocks of one real station's text under many station ids, in several sorted runs (the real files are the runs of the model's regions one after another)
    std::string standIn(const std::string& block, const std::string& kind, const std::vector<std::vector<std::string>>& runs) {
        std::string file;
        for (const auto& run : runs) {
            for (const auto& id : run) {
                auto b = block;
                b.replace(b.find("KOKC"), 4, id.size() == 4 ? id : id.substr(0, 4));
                file += " " + b.substr(1);   // (blocks begin with a space: the header is at the start of a line)
                file += "                                                                          \n";
            }
        }
        (void) kind;
        return file;
    }
}

int main(int argc, char ** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    const std::string folder = argc > 1 ? argv[1] : "tests/gfs/nbm";
    const auto nbsText = slurp(folder + "/KOKC_nbs.txt"), nbeText = slurp(folder + "/KOKC_nbe.txt"), nbxText = slurp(folder + "/KOKC_nbx.txt");
    CHECK(!nbsText.empty() && !nbeText.empty() && !nbxText.empty());

    // ---- NBS: 3 hourly columns, 6 to 72 hours; TXN and XND only under the 00 and 12 UTC columns that have them
    const auto nbs = NbmText::parse(nbsText);
    CHECK(nbs.station == "KOKC" && nbs.kind == "NBS" && nbs.cycle > 0 && nbs.columns.size() == 23);
    CHECK(nbs.columns.front().time == nbs.cycle + 6 * 3600 && nbs.columns.back().time == nbs.cycle + 72 * 3600 && nbs.columns.front().source == "NBS");
    for (size_t i = 1; i < nbs.columns.size(); i++) {
        CHECK(nbs.columns[i].time - nbs.columns[i - 1].time == 3 * 3600);
    }
    CHECK(nbs.columns[0].get("TMP") == 42 && nbs.columns[1].get("TMP") == 46 && nbs.columns[22].get("TMP") == 54 && nbs.columns[0].get("DPT") == 42 && nbs.columns[0].get("SKY") == 66);
    CHECK(nbs.columns[4].get("TXN") == 56 && nbs.columns[4].get("XND") == 2 && nbs.columns[8].get("TXN") == 36 && nbs.columns[12].get("TXN") == 59 && nbs.columns[20].get("TXN") == 63);
    CHECK(!nbs.columns[0].has("TXN") && !nbs.columns[3].has("TXN") && !nbs.columns[5].has("TXN"));   // (the highs and lows are under the columns of 00 and 12 UTC only: the others have none)

    // ---- NBE: 12 hourly, the last number of a row may be the climate value (CLIMO) which is not a column
    const auto nbe = NbmText::parse(nbeText);
    CHECK(nbe.station == "KOKC" && nbe.kind == "NBE" && nbe.columns.size() == 15);
    CHECK(nbe.columns.front().time == nbe.cycle + 18 * 3600 && nbe.columns.back().time == nbe.cycle + 186 * 3600);
    for (size_t i = 1; i < nbe.columns.size(); i++) {
        CHECK(nbe.columns[i].time - nbe.columns[i - 1].time == 12 * 3600);
    }
    CHECK(nbe.columns[0].get("TXN") == 56 && nbe.columns[1].get("TXN") == 36 && nbe.columns[0].get("TMP") == 50 && nbe.columns[1].get("TMP") == 39 && nbe.columns[14].has("TMP"));
    CHECK(!nbe.columns[14].has("CLI") && nbe.columns[14].get("TXN") == 58);   // (the CLIMO pair of the TXN row: 59 and 37 of the header's last column is not read as another column)

    // ---- NBX: 12 hourly, to 264 hours
    const auto nbx = NbmText::parse(nbxText);
    CHECK(nbx.station == "KOKC" && nbx.kind == "NBX" && !nbx.columns.empty() && nbx.columns.front().time > nbe.columns.back().time - 24 * 3600);
    for (size_t i = 1; i < nbx.columns.size(); i++) {
        CHECK(nbx.columns[i].time - nbx.columns[i - 1].time == 12 * 3600);
    }
    // a text that is not a bulletin gives nothing
    CHECK(NbmText::parse("nothing here").columns.empty() && NbmText::parse("").columns.empty());

    // ---- the merge: the gap from the grid, then NBS, NBE after the last NBS, NBX after the last NBE, each tagged, the times strictly increasing
    std::vector<NbmText::Column> gap;
    for (qint64 t = nbs.cycle - 6 * 3600; t < nbs.columns.front().time; t += 3 * 3600) {
        NbmText::Column c;
        c.time = t;
        c.source = "API";
        c.value["TMP"] = 40;
        gap.push_back(c);
    }
    const auto merged = NbmText::merge(gap, nbs.columns, nbe.columns, nbx.columns, nbs.cycle - 6 * 3600);
    CHECK(!merged.empty() && merged.front().source == "API" && merged.back().time == std::max(nbx.columns.empty() ? 0 : nbx.columns.back().time, nbe.columns.back().time));
    std::set<std::string> sources;
    for (size_t i = 0; i < merged.size(); i++) {
        sources.insert(merged[i].source);
        CHECK(i == 0 || merged[i].time > merged[i - 1].time);
    }
    CHECK(sources.count("API") == 1 && sources.count("NBS") == 1 && sources.count("NBE") == 1);
    qint64 lastNbs = 0;
    for (const auto& c : merged) {
        if (c.source == "NBS") {
            lastNbs = c.time;
        }
        CHECK(c.source != "NBE" || c.time > lastNbs);   // (NBE columns begin after the last NBS column)
    }

    // ---- the finder on a stand-in file: 4 sorted runs of 200 stations each; every station found, a missing one not
    std::vector<std::vector<std::string>> runs(4);
    std::vector<std::string> all;
    for (size_t r = 0; r < runs.size(); r++) {
        for (int i = 0; i < 200; i++) {
            char id[8];
            std::snprintf(id, sizeof id, "%c%03d", 'A' + static_cast<char>(r * 5 + i / 50), i);   // ascending within a run; the next run starts low again
            runs[r].push_back(id);
            all.push_back(id);
        }
    }
    const auto file = standIn(nbeText, "NBE", runs);
    NbmText::File f;
    f.url = "stand-in";
    f.kind = "NBE";
    f.size = static_cast<long long>(file.size());
    const NbmText::Range range = [&file] (const std::string&, long long a, long long b) { return QByteArray::fromStdString(file.substr(static_cast<size_t>(a), static_cast<size_t>(b - a + 1))); };
    int missed = 0;
    std::set<std::string> distinct{all.begin(), all.end()};
    for (const auto& id : distinct) {
        const auto block = NbmText::findBlock(f, id, range);
        if (block.empty() || NbmText::parse(block).station != id || NbmText::parse(block).columns.size() != 15) {
            missed++;
            std::printf("  stand-in: %s not found (%s)\n", id.c_str(), block.empty() ? "no block" : "wrong block");
        }
    }
    std::printf("stand-in: %d of %d stations not found\n", missed, static_cast<int>(distinct.size()));
    CHECK(missed == 0);
    CHECK(NbmText::findBlock(f, "ZZZZ", range).empty() && NbmText::findBlock(f, "A", range).empty());

    // ---- the real files, every station (only with the paths given)
    for (const char * kind : {"NBS", "NBE", "NBX"}) {
        const auto * path = std::getenv((std::string{"NBM_FILE_"} + kind).c_str());
        if (path == nullptr) {
            continue;
        }
        const auto real = slurp(path);
        std::set<std::string> stations;
        for (size_t at = 0; at < real.size();) {   // the header lines: " KOKC    NBM  NBE GUIDANCE ..." (a scan by lines: a regex over 30 MB takes minutes)
            auto end = real.find('\n', at);
            end = end == std::string::npos ? real.size() : end;
            if (end > at + 2 && real[at] == ' ' && real[at + 1] != ' ' && real.find(" NBM ", at) < end && real.find("GUIDANCE", at) < end) {
                const auto token = real.find(' ', at + 1);
                stations.insert(real.substr(at + 1, token - at - 1));
            }
            at = end + 1;
        }
        NbmText::File rf;
        rf.url = path;
        rf.kind = kind;
        rf.size = static_cast<long long>(real.size());
        const NbmText::Range rr = [&real] (const std::string&, long long a, long long b) { return QByteArray::fromStdString(real.substr(static_cast<size_t>(a), static_cast<size_t>(b - a + 1))); };
        int bad = 0;
        for (const auto& id : stations) {
            const auto block = NbmText::findBlock(rf, id, rr);
            if (block.empty() || NbmText::parse(block).station != id) {
                bad++;
                if (bad <= 5) {
                    std::printf("  not found: %s\n", id.c_str());
                }
            }
        }
        std::printf("%s: %d stations in the file, %d not found by the finder\n", kind, static_cast<int>(stations.size()), bad);
        CHECK(!stations.empty() && bad == 0);
    }
    std::printf(failures == 0 ? "all NBM text tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
