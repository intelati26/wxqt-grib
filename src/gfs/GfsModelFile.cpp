// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModelFile.h"
#include "gfs/Servers.h"
#include "gfs/GeosOpendap.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <sstream>
#include <QDateTime>
#include <cctype>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTimeZone>

namespace {
    using Run = GfsData::Run;
    using Want = GfsData::Want;

    std::string trim(const std::string& text) {
        const auto begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return {};
        }
        return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    }

    std::vector<std::string> split(const std::string& text, char separator) {
        std::vector<std::string> out;
        std::string part;
        std::istringstream in{text};
        while (std::getline(in, part, separator)) {
            out.push_back(trim(part));
        }
        return out;
    }

    std::string padded(int value, int width) {
        auto s = std::to_string(value);
        return s.size() < static_cast<size_t>(width) ? std::string(static_cast<size_t>(width) - s.size(), '0') + s : s;
    }

    // one condition of a file rule: a field of the want must be one of a list, or match a regular expression
    struct Condition {
        enum class Field { Variable, Level, Stat, File } field{Field::Variable};
        std::vector<std::string> values;
        std::shared_ptr<QRegularExpression> pattern;
        bool matchesText(const std::string& text) const {
            if (pattern) {
                return pattern->match(QString::fromStdString(text)).hasMatch();
            }
            return std::find(values.begin(), values.end(), text) != values.end();
        }
        bool matches(const Want& w) const {
            return matchesText(field == Field::Variable ? w.variable : field == Field::Level ? w.level : w.stat);
        }
    };

    struct Rule {
        std::vector<Condition> conditions;
        std::string pattern;   // the template
    };

    // what a definition says, kept by the functions it makes
    struct Definition {
        std::string legacyBefore, legacyUrl;     // `legacy_url = yyyyMMddHH => pattern`: the runs before that time (yyyyMMddHH) are in the file of this pattern (a model that changed its files)
        std::vector<Rule> urls;                  // `url` lines: with `when file...` the first that matches is the place; the plain ones are the places when none does
        std::string storm;                       // {storm} in a url (a model that follows a storm)
        std::string subsetUrl;                   // the server's box request, with {variables} and {levels}
        std::string subsetVariable{"&var_{v}=on"};
        std::string subsetLevel{"&lev_{l}=on"};
        std::vector<Rule> rules;
        std::string fileDefault;
        std::vector<std::pair<std::string, double>> scales;           // variable -> factor
        std::vector<std::pair<std::string, double>> offsets;          // variable -> amount added after the factor (Kelvin to degrees C)
        std::vector<Rule> variables;                                  // `nc_variable` rules: a NetCDF file's name for a want
        std::vector<double> ncLevels;                                 // `nc_levels` the pressure levels of a NetCDF file's `lev` dimension, in its order: a variable named with a trailing `@lev` is then the band of the want's level
        std::map<std::string, int> validOffsetByFile;                 // `valid_offset_file` FILE:minutes: a collection stamped differently (a 3 hour average is stamped in the middle of its period)
        std::string tarMember;                                        // `tar_member` pattern: the file of an hour in the tar that is the url (kind = tar)
        std::string partial;                                          // `partial` NAME: a reader of part of a file tried before the whole file (geos-opendap)
        int validOffsetMinutes{0};                                    // {valid} is the run's time plus the hour plus this (a time average is stamped half an hour after)
        std::vector<std::pair<std::string, std::pair<int, int>>> members;   // "p01".."p20" -> bands, kept as expanded names
        std::map<std::string, int> memberBand;
    };

    // {variable} {level} {stat} {mb} {mb4} {stat|default}
    std::string expandWant(const std::string& pattern, const Want& w) {
        std::string out;
        for (size_t i = 0; i < pattern.size();) {
            if (pattern[i] != '{') {
                out += pattern[i++];
                continue;
            }
            const auto close = pattern.find('}', i);
            if (close == std::string::npos) {
                out += pattern.substr(i);
                break;
            }
            const auto token = pattern.substr(i + 1, close - i - 1);
            const auto bar = token.find('|');
            const auto name = token.substr(0, bar);
            std::string value;
            if (name == "variable") {
                value = w.variable;
            } else if (name == "level") {
                value = w.level;
            } else if (name == "stat") {
                value = w.stat;
            } else if (name == "mb" || name == "mb4") {
                const int number = std::atoi(w.level.c_str());
                value = name == "mb" ? std::to_string(number) : padded(number, 4);
            }
            if (value.empty() && bar != std::string::npos) {
                value = token.substr(bar + 1);
            }
            out += value;
            i = close + 1;
        }
        return out;
    }

    void replaceAll(std::string& text, const std::string& token, const std::string& value) {
        for (auto at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size())) {
            text.replace(at, token.size(), value);
        }
    }

    // {date} {yy} {cycle} {prev_cycle} {hour} {hour2} {hour3} {file} {/file} {file3} {dir} {name} {tag} {tail} {storm} {year} {month} {day}
    std::string expandUrl(std::string url, const Definition& d, const Run& run, int hour, const std::string& alt, const std::string& previous) {
        if (!Servers::nameIn(url).empty()) {   // a server by name (`{@rrfs-ops}/...`): its first place; urlsOf gives the others
            const auto first = Servers::places(url);
            if (!first.empty()) {
                url = first.front();
            }
        }
        const auto slash = alt.find('/');
        const auto directory = slash == std::string::npos ? std::string{} : alt.substr(0, slash);
        const auto name = slash == std::string::npos ? alt : alt.substr(slash + 1);
        const auto colon = alt.find(':');
        replaceAll(url, "{date}", run.date);
        replaceAll(url, "{year}", run.date.substr(0, 4));
        replaceAll(url, "{yy}", run.date.substr(2, 2));
        replaceAll(url, "{month}", run.date.substr(4, 2));
        replaceAll(url, "{day}", run.date.substr(6, 2));
        replaceAll(url, "{cycle}", run.cycle);
        replaceAll(url, "{prev_cycle}", previous);
        if (url.find("{valid") != std::string::npos) {   // the time the file is for: the run's time, the hour and the offset
            auto t = QDateTime::fromString(QString::fromStdString(run.date + run.cycle), "yyyyMMddHH");
            t.setTimeZone(QTimeZone::utc());
            const auto own = d.validOffsetByFile.find(alt);
            const int offset = own != d.validOffsetByFile.end() ? own->second : d.validOffsetMinutes;
            t = t.addSecs(static_cast<qint64>(hour) * 3600 + static_cast<qint64>(offset) * 60);
            replaceAll(url, "{valid_date}", t.toString("yyyyMMdd").toStdString());
            replaceAll(url, "{valid_hour2}", t.toString("HH").toStdString());
            replaceAll(url, "{valid_minute2}", t.toString("mm").toStdString());
            replaceAll(url, "{valid_iso}", t.toString("yyyy-MM-ddTHHmm").toStdString());   // Open-Meteo's file names
            replaceAll(url, "{valid}", t.toString("yyyyMMdd_HHmm").toStdString());
        }
        replaceAll(url, "{hour3}", padded(hour, 3));
        replaceAll(url, "{hour2}", padded(hour, 2));
        replaceAll(url, "{hour}", std::to_string(hour));
        replaceAll(url, "{/file}", alt.empty() ? std::string{} : "/" + alt);
        replaceAll(url, "{file3}", alt.substr(0, 3));
        replaceAll(url, "{file}", alt);
        replaceAll(url, "{dir}", directory);
        replaceAll(url, "{name}", name);
        replaceAll(url, "{tag}", colon == std::string::npos ? alt : alt.substr(0, colon));
        replaceAll(url, "{tail}", colon == std::string::npos ? alt : alt.substr(colon + 1));
        {   // (the storm's id comes from a server's list: only its letters and digits go into a url)
            std::string storm;
            for (const char c : d.storm) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    storm += c;
                }
            }
            replaceAll(url, "{storm}", storm);
        }
        return url;
    }

    // the places a field may be: each alternative of the file (`;` between them) in each url that applies
    std::vector<std::string> urlsOf(const Definition& d, const Run& run, int hour, const std::string& file) {
        const int cycle = std::atoi(run.cycle.c_str());
        const auto previous = padded((cycle + 18) % 24, 2);
        std::vector<std::string> alternatives = split(file, ';');
        if (alternatives.empty()) {
            alternatives.push_back({});
        }
        std::vector<std::string> out;
        const auto pushPlaces = [&] (const std::string& pattern, const std::string& alt) {   // a pattern that begins with a server's name is one place for each of its bases (the first first)
            const auto bases = Servers::nameIn(pattern).empty() ? std::vector<std::string>{pattern} : Servers::places(pattern);
            for (const auto& each : bases.empty() ? std::vector<std::string>{pattern} : bases) {
                out.push_back(expandUrl(each, d, run, hour, alt, previous));
            }
        };
        if (!d.legacyBefore.empty() && run.date + run.cycle < d.legacyBefore) {   // a run from before the files changed: its one file, whatever the record
            out.push_back(expandUrl(d.legacyUrl, d, run, hour, {}, previous));
            return out;
        }
        for (auto alt : alternatives) {
            replaceAll(alt, "{ee}", run.cycle);
            replaceAll(alt, "{pe}", previous);
            if (alt.empty()) {
                alt = d.fileDefault;
            }
            const auto slash = alt.find('/');
            const auto directory = slash == std::string::npos ? std::string{} : alt.substr(0, slash);
            // a conditional url that matches is the one place; with none, the plain urls
            const Rule * chosen = nullptr;
            for (const auto& rule : d.urls) {
                if (!rule.conditions.empty() && std::all_of(rule.conditions.begin(), rule.conditions.end(), [&alt] (const Condition& c) { return c.matchesText(alt); })) {
                    chosen = &rule;
                    break;
                }
            }
            if (chosen != nullptr) {
                pushPlaces(chosen->pattern, alt);
                continue;
            }
            for (const auto& rule : d.urls) {
                if (!rule.conditions.empty()) {
                    continue;
                }
                if (directory.empty() && rule.pattern.find("{dir}") != std::string::npos) {
                    continue;   // a file that is not "directory/name" has no place where the url wants one
                }
                pushPlaces(rule.pattern, alt);
            }
        }
        return out;
    }

    // What a definition file may name. A file can come from someone else, so a place is a web address and nothing more (not a file of this computer, not one of GDAL's "/vsi..." paths), and no
    // pattern has the characters that would end the quoted name it is handed to GDAL in or step out of a folder. "" when it is fine, or what is wrong with it.
    std::string textProblem(const std::string& text) {
        for (const unsigned char c : text) {
            if (c < 0x20 || c == 0x7f || c == '"' || c == '\'' || c == '\\' || c == '`' || c == '<' || c == '>') {
                return c < 0x20 || c == 0x7f ? "a control character is not allowed" : std::string{"the character "} + static_cast<char>(c) + " is not allowed";
            }
        }
        if (text.find("..") != std::string::npos && (text.find("../") != std::string::npos || text.find("/..") != std::string::npos)) {
            return "\"..\" as a folder is not allowed";
        }
        if (text.find("/vsi") != std::string::npos) {
            return "a GDAL \"/vsi\" path is not allowed";
        }
        return {};
    }

    std::string urlProblem(const std::string& url) {
        if (url.rfind("{@", 0) == 0) {   // a server by name: its bases are checked where the table is read (Servers::parse); what follows is the path
            const auto name = Servers::nameIn(url);
            if (name.empty()) {
                return "a server by name is {@name} with a plain name (a to z, 0 to 9 and -)";
            }
            const auto rest = url.substr(name.size() + 3);
            if (!rest.empty() && rest[0] != '/') {
                return "a path follows {@name} with a /";
            }
            return textProblem(rest);
        }
        const bool https = url.rfind("https://", 0) == 0;
        if (!https && url.rfind("http://", 0) != 0) {
            return "a url begins with https:// or http://";
        }
        const size_t from = https ? 8 : 7;
        const auto host = url.substr(from, url.find_first_of("/?#", from) - from);
        if (host.empty() || host.find_first_of("{}@ ") != std::string::npos) {   // (the server is written out: no token decides it, and no "user@" in front of it)
            return "the server of a url is written out (no token, space or @ in it)";
        }
        if (url.find(' ') != std::string::npos) {
            return "a url has no spaces (write %20)";
        }
        return textProblem(url);
    }

    std::string nameProblem(const std::string& name, bool slashes) {
        if (!slashes && name.find('/') != std::string::npos) {
            return "a / is not allowed here";
        }
        if (!name.empty() && name.front() == '-') {
            return "it can not begin with -";
        }
        return textProblem(name);
    }

    struct Reader {
        std::string error;
        int line{0};
        bool fail(const std::string& why) {
            if (error.empty()) {
                error = "line " + std::to_string(line) + ": " + why;
            }
            return false;
        }
    };

    bool toInt(const std::string& text, int& out) {
        char * end = nullptr;
        const long v = std::strtol(text.c_str(), &end, 10);
        if (text.empty() || end == nullptr || *end != '\0') {
            return false;
        }
        out = static_cast<int>(v);
        return true;
    }

    bool toDouble(const std::string& text, double& out) {
        char * end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (text.empty() || end == nullptr || *end != '\0') {
            return false;
        }
        out = v;
        return true;
    }

    bool toBool(const std::string& text) {
        return text == "true" || text == "yes" || text == "1" || text == "on";
    }

    // `when variable=A,B level="2 m above ground" stat~^p[0-9]+$ => template`, or just a template
    bool parseRule(const std::string& value, Rule& rule, Reader& reader) {
        const auto arrow = value.find("=>");
        if (arrow == std::string::npos) {
            rule.pattern = trim(value);
            return !rule.pattern.empty() || reader.fail("a file rule needs a template");
        }
        rule.pattern = trim(value.substr(arrow + 2));
        auto head = trim(value.substr(0, arrow));
        if (head.compare(0, 4, "when") != 0) {
            return reader.fail("a file rule with a condition starts with `when`");
        }
        head = trim(head.substr(4));
        size_t i = 0;
        while (i < head.size()) {
            while (i < head.size() && head[i] == ' ') {
                i++;
            }
            if (i >= head.size()) {
                break;
            }
            const auto stop = head.find_first_of("=~", i);
            if (stop == std::string::npos) {
                return reader.fail("a condition is field=value or field~pattern");
            }
            const auto field = trim(head.substr(i, stop - i));
            Condition c;
            if (field == "variable") {
                c.field = Condition::Field::Variable;
            } else if (field == "level") {
                c.field = Condition::Field::Level;
            } else if (field == "stat") {
                c.field = Condition::Field::Stat;
            } else if (field == "file") {
                c.field = Condition::Field::File;
            } else {
                return reader.fail("unknown condition field " + field + " (variable, level, stat, file)");
            }
            const bool regex = head[stop] == '~';
            i = stop + 1;
            // the value runs to the next space that is not inside quotes
            std::string raw;
            bool quoted = false;
            while (i < head.size() && (quoted || head[i] != ' ')) {
                if (head[i] == '"') {
                    quoted = !quoted;
                } else {
                    raw += head[i];
                }
                i++;
            }
            if (regex) {
                c.pattern = std::make_shared<QRegularExpression>(QString::fromStdString(raw));
                if (!c.pattern->isValid()) {
                    return reader.fail("not a regular expression: " + raw);
                }
            } else {
                c.values = split(raw, ',');
                if (raw.empty()) {
                    c.values = {""};   // `stat=` : no stat
                }
            }
            rule.conditions.push_back(c);
        }
        return true;
    }

    // "p01-p20:2-21", "c00:1", "b1-b9:1-9"
    bool parseMembers(const std::string& value, Definition& d, Reader& reader) {
        for (const auto& item : split(value, ',')) {
            if (item.empty()) {
                continue;
            }
            const auto colon = item.find(':');
            if (colon == std::string::npos) {
                return reader.fail("a member is name:band, or first-last:band-band");
            }
            const auto names = item.substr(0, colon), bands = item.substr(colon + 1);
            const auto dash = names.find('-');
            const auto bandDash = bands.find('-');
            if (dash == std::string::npos) {
                int band = 0;
                if (!toInt(bands, band)) {
                    return reader.fail("not a band number: " + bands);
                }
                d.memberBand[names] = band;
                continue;
            }
            const auto first = names.substr(0, dash), last = names.substr(dash + 1);
            size_t prefix = 0;
            while (prefix < first.size() && !std::isdigit(static_cast<unsigned char>(first[prefix]))) {
                prefix++;
            }
            int a = 0, b = 0, band0 = 0;
            if (prefix == first.size() || bandDash == std::string::npos || !toInt(first.substr(prefix), a) || !toInt(last.substr(prefix), b) || !toInt(bands.substr(0, bandDash), band0)) {
                return reader.fail("a range of members is p01-p20:2-21");
            }
            const int width = static_cast<int>(first.size() - prefix);
            for (int n = a; n <= b; n++) {
                d.memberBand[first.substr(0, prefix) + padded(n, width)] = band0 + (n - a);
            }
        }
        return true;
    }

    // every, or a comma list: ids, variables, levels ...
    void addList(std::vector<std::string>& to, const std::string& value) {
        for (const auto& item : split(value, ',')) {
            if (!item.empty()) {
                to.push_back(item);
            }
        }
    }
}

namespace {
    // `include = NAME` in a built-in definition: the lines of the shared piece "_NAME" of the built-in list (what several models have alike: SPC's post-processed NetCDF on the 0.03 degree grid)
    std::string withIncludes(const std::string& text) {
        std::string out;
        std::istringstream in{text};
        std::string raw;
        while (std::getline(in, raw)) {
            const auto eq = raw.find('=');
            if (eq != std::string::npos && raw.compare(0, 7, "include") == 0 && trim(raw.substr(0, eq)) == "include") {
                const auto wanted = "_" + trim(raw.substr(eq + 1));
                for (const auto& [name, piece] : GfsModelFile::builtIn()) {
                    if (name == wanted) {
                        out += piece + "\n";
                    }
                }
                continue;
            }
            out += raw + "\n";
        }
        return out;
    }
}

GfsModelFile::Parsed GfsModelFile::parse(const std::string& original, const std::string& origin) {
    const auto text = origin == "built in" ? withIncludes(original) : original;
    Parsed result;
    result.origin = origin;
    Reader reader;
    auto definition = std::make_shared<Definition>();
    GfsModels::Def def;
    std::string kind = "grib";
    GfsData::Source source;   // the parts that are plain values
    std::string indexFormat = "noaa";
    std::string sourceLabel;
    bool stormModel = false;
    std::string wrfFoldersPref;
    std::istringstream in{text};
    std::string raw;
    std::vector<std::string> hourSegments;
    std::string runLength;
    while (std::getline(in, raw)) {
        reader.line++;
        const auto line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            reader.fail("expected key = value");
            break;
        }
        auto key = trim(line.substr(0, eq));
        std::transform(key.begin(), key.end(), key.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto value = trim(line.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {   // quotes keep the spaces at the ends: clone_prefix = "Mean "
            value = value.substr(1, value.size() - 2);
        }
        int n = 0;
        double x = 0.0;
        if (key == "id") {
            def.id = value;
        } else if (key == "label") {
            def.label = value;
        } else if (key == "run_note") {   // run_note = 0.1 degree: said beside each run in a list of runs
            def.runNote = value;
        } else if (key == "legacy_run_note") {
            def.legacyRunNote = value;
        } else if (key == "legacy_label") {   // legacy_label = text: the credit of the runs before legacy_url's time
            def.legacyLabel = value;
        } else if (key == "group") {
            def.group = value;
        } else if (key == "alternate") {
            const auto bar = value.find('|');
            def.alternate = value.substr(0, bar);
            def.alternate.erase(def.alternate.find_last_not_of(' ') + 1);
            if (bar != std::string::npos) {
                def.alternateNote = value.substr(bar + 1);
                def.alternateNote.erase(0, def.alternateNote.find_first_not_of(' '));
            }
        } else if (key == "source_label") {
            sourceLabel = value;
        } else if (key == "why") {
            def.why = value;
        } else if (key == "link") {   // kind | label | url
            const auto parts = split(value, '|');
            if (parts.size() != 3 || parts[2].empty()) {
                reader.fail("link is kind | label | url");
                break;
            }
            def.links.push_back({parts[0], parts[1], parts[2]});
        } else if (key == "kind") {
            if (value != "grib" && value != "whole" && value != "netcdf" && value != "om" && value != "wrf" && value != "custom" && value != "tar") {
                reader.fail("kind is grib, whole, netcdf, om, wrf, custom or tar");
                break;
            }
            kind = value;
        } else if (key == "legacy_url") {
            const auto arrow = value.find("=>");
            if (arrow == std::string::npos || arrow < 10) {
                reader.fail("legacy_url is yyyyMMddHH => url");
                break;
            }
            definition->legacyBefore = value.substr(0, arrow);
            definition->legacyBefore.erase(definition->legacyBefore.find_last_not_of(' ') + 1);
            def.legacyBefore = definition->legacyBefore;
            definition->legacyUrl = value.substr(arrow + 2);
            definition->legacyUrl.erase(0, definition->legacyUrl.find_first_not_of(' '));
            if (const auto problem = urlProblem(definition->legacyUrl); !problem.empty()) {
                reader.fail("legacy_url: " + problem);
                break;
            }
        } else if (key == "url") {
            Rule rule;
            if (!parseRule(value, rule, reader)) {
                break;
            }
            for (const auto& c : rule.conditions) {
                if (c.field != Condition::Field::File) {
                    reader.fail("a url is chosen by the file only: when file...");
                }
            }
            if (const auto problem = urlProblem(rule.pattern); !problem.empty()) {
                reader.fail("url: " + problem);
                break;
            }
            definition->urls.push_back(rule);
        } else if (key == "nc_variable") {
            Rule rule;
            if (!parseRule(value, rule, reader)) {
                break;
            }
            if (const auto problem = nameProblem(rule.pattern, true); !problem.empty()) {
                reader.fail("nc_variable: " + problem);
                break;
            }
            definition->variables.push_back(rule);
        } else if (key == "value_offset") {   // VARIABLE:amount, ...
            for (const auto& item : split(value, ',')) {
                const auto colon = item.find(':');
                double amount = 0.0;
                if (colon == std::string::npos || !toDouble(item.substr(colon + 1), amount)) {
                    reader.fail("value_offset is VARIABLE:amount");
                    break;
                }
                definition->offsets.emplace_back(item.substr(0, colon), amount);
            }
        } else if (key == "tar_member") {
            if (const auto problem = nameProblem(value, false); !problem.empty()) {
                reader.fail("tar_member: " + problem);
                break;
            }
            definition->tarMember = value;
        } else if (key == "partial") {
            if (value != "geos-opendap") {
                reader.fail("partial is geos-opendap");
                break;
            }
            definition->partial = value;
        } else if (key == "valid_offset") {
            if (!toInt(value, n)) {
                reader.fail("valid_offset is minutes");
                break;
            }
            definition->validOffsetMinutes = n;
        } else if (key == "nc_levels") {
            definition->ncLevels.clear();
            for (size_t at = 0; at <= value.size();) {
                auto end = value.find(',', at);
                end = end == std::string::npos ? value.size() : end;
                const auto text = value.substr(at, end - at);
                char * stop = nullptr;
                const double level = std::strtod(text.c_str(), &stop);
                if (stop == text.c_str()) {
                    reader.fail("nc_levels is a list of pressure levels");
                    break;
                }
                definition->ncLevels.push_back(level);
                at = end + 1;
            }
        } else if (key == "valid_offset_file") {
            const auto colon = value.find(':');
            if (colon == std::string::npos || !toInt(value.substr(colon + 1), n)) {
                reader.fail("valid_offset_file is FILE:minutes");
                break;
            }
            definition->validOffsetByFile[value.substr(0, colon)] = n;
        } else if (key == "subset_url") {
            if (const auto problem = urlProblem(value); !problem.empty()) {
                reader.fail("subset_url: " + problem);
                break;
            }
            definition->subsetUrl = value;
        } else if (key == "subset_variable") {
            if (const auto problem = textProblem(value); !problem.empty()) {
                reader.fail("subset_variable: " + problem);
                break;
            }
            definition->subsetVariable = value;
        } else if (key == "subset_level") {
            if (const auto problem = textProblem(value); !problem.empty()) {
                reader.fail("subset_level: " + problem);
                break;
            }
            definition->subsetLevel = value;
        } else if (key == "storm") {
            stormModel = toBool(value);
        } else if (key == "file") {
            Rule rule;
            if (!parseRule(value, rule, reader)) {
                break;
            }
            if (const auto problem = nameProblem(rule.pattern, true); !problem.empty()) {
                reader.fail("file: " + problem);
                break;
            }
            definition->rules.push_back(rule);
        } else if (key == "file_default") {
            if (const auto problem = nameProblem(value, true); !problem.empty()) {
                reader.fail("file_default: " + problem);
                break;
            }
            definition->fileDefault = value;
        } else if (key == "index_format") {
            if (value != "noaa" && value != "ecmwf") {
                reader.fail("index_format is noaa or ecmwf");
                break;
            }
            indexFormat = value;
        } else if (key == "index_suffix") {
            if (const auto problem = textProblem(value); !problem.empty()) {
                reader.fail("index_suffix: " + problem);
                break;
            }
            source.indexSuffix = value;
        } else if (key == "index_replaces") {
            if (const auto problem = textProblem(value); !problem.empty()) {
                reader.fail("index_replaces: " + problem);
                break;
            }
            source.indexReplaces = value;
        } else if (key == "default_detail") {
            source.defaultDetail = value;
        } else if (key == "forecast_text") {   // "{hour} hour fcst": every hour is a record of the one file
            source.forecastText = value;
        } else if (key == "check_run") {
            source.checkRun = value;
        } else if (key == "probe_file") {
            if (const auto problem = textProblem(value); !problem.empty()) {
                reader.fail("probe_file: " + problem);
                break;
            }
            source.probeFile = value;
        } else if (key == "probe_hour" || key == "cycle_hours" || key == "lag_hours" || key == "cycles_to_try" || key == "max_parallel") {
            if (!toInt(value, n)) {
                reader.fail(key + " is a whole number");
                break;
            }
            (key == "probe_hour" ? source.probeHour : key == "cycle_hours" ? source.cycleHours : key == "lag_hours" ? source.lagHours : key == "cycles_to_try" ? source.cyclesToTry : source.maxParallel) = n;
        } else if (key == "extra_missing") {
            if (!toDouble(value, x)) {
                reader.fail("extra_missing is a number");
                break;
            }
            source.extraMissing = static_cast<float>(x);
        } else if (key == "value_scale") {   // VARIABLE:factor, ...
            for (const auto& item : split(value, ',')) {
                const auto colon = item.find(':');
                double factor = 1.0;
                if (colon == std::string::npos || !toDouble(item.substr(colon + 1), factor)) {
                    reader.fail("value_scale is VARIABLE:factor");
                    break;
                }
                definition->scales.emplace_back(item.substr(0, colon), factor);
            }
        } else if (key == "run_folders_pref") {   // for kind = wrf: the name of the setting that, when it has a value (folders separated by `;`), is used instead of run_folders
            wrfFoldersPref = value;
        } else if (key == "run_folders") {   // for kind = wrf: the folders of the runs, globs, separated by `;` (`~` is the home folder)
            for (const auto& folder : split(value, ';')) {
                if (!folder.empty()) {
                    source.wrf.folders.push_back(folder);
                    source.wrf.defaultFolders.push_back(folder);
                }
            }
            if (source.wrf.folders.empty()) {
                reader.fail("run_folders is a folder (or several, separated by ;)");
                break;
            }
        } else if (key == "om_grid") {   // `regular` (the file's own latitude / longitude grid) or `gaussian 320 0.25` (the order of ECMWF's reduced Gaussian grid and the degrees it is interpolated to)
            const auto parts = split(value, ' ');
            if (parts.size() == 1 && parts[0] == "regular") {
                source.om.gaussian = 0;
            } else if (parts.size() == 3 && parts[0] == "gaussian" && toInt(parts[1], n) && n > 0 && toDouble(parts[2], x) && x > 0.0) {
                source.om.gaussian = n;
                source.om.step = x;
            } else {
                reader.fail("om_grid is `regular` or `gaussian 320 0.25`");
                break;
            }
        } else if (key == "warp") {   // step,west,south,east,north
            const auto parts = split(value, ',');
            double v[5] = {0, 0, 0, 0, 0};
            bool ok = parts.size() == 5;
            for (size_t i = 0; ok && i < 5; i++) {
                ok = toDouble(parts[i], v[i]);
            }
            if (!ok) {
                reader.fail("warp is step,west,south,east,north");
                break;
            }
            source.warp = {true, v[0], v[1], v[2], v[3], v[4]};
        } else if (key == "nest_center") {   // url of the file with the area's centre, half width km, half height km: the run's own box (the `warp` box is the fallback and must hold every place)
            const auto parts = split(value, ',');
            double halfWidth = 0, halfHeight = 0;
            if (parts.size() != 3 || !toDouble(parts[1], halfWidth) || !toDouble(parts[2], halfHeight)) {
                reader.fail("nest_center is url,half width km,half height km");
                break;
            }
            const std::string pattern = parts[0];
            source.warpOf = [pattern, halfWidth, halfHeight] (const GfsData::Run& run, GfsData::Source::Warp& warp) {
                static std::mutex mutex;
                static std::map<std::string, std::pair<double, double>> known;   // by url: read once a session (a run's centre does not change)
                std::string url = pattern;
                replaceAll(url, "{date}", run.date);
                replaceAll(url, "{cycle}", run.cycle);
                double lat = 0, lon = 0;
                {
                    std::lock_guard<std::mutex> lock{mutex};
                    const auto found = known.find(url);
                    if (found != known.end()) {
                        lat = found->second.first;
                        lon = found->second.second;
                    } else {
                        std::istringstream in{UtilityIO::downloadAsByteArray(url).toStdString()};
                        if (!(in >> lat >> lon) || std::abs(lat) > 90.0 || std::abs(lon) > 360.0) {
                            return false;
                        }
                        known[url] = {lat, lon};
                    }
                }
                const double dLat = halfHeight / 111.2, dLon = halfWidth / (111.2 * std::cos(lat * 3.14159265358979 / 180.0));
                warp.west = lon - dLon;
                warp.east = lon + dLon;
                warp.south = lat - dLat;
                warp.north = lat + dLat;
                return true;
            };
        } else if (key == "member") {
            if (!parseMembers(value, *definition, reader)) {
                break;
            }
        } else if (key == "hours") {   // 0-192/3, 198-384/6
            for (const auto& seg : split(value, ',')) {
                hourSegments.push_back(seg);
            }
        } else if (key == "run_every") {
            if (!toInt(value, n) || n < 1 || n > 24) {
                reader.fail("run_every is the hours between runs");
                break;
            }
            def.cycleStep = n;
        } else if (key == "hourly_runs") {
            def.hourlyRuns = toBool(value);
        } else if (key == "sectors") {
            if (value != "world" && value != "conus" && value != "storm") {
                reader.fail("sectors is world, conus or storm");
                break;
            }
            def.sectors = value == "conus" ? GfsModels::Sectors::Conus : value == "storm" ? GfsModels::Sectors::Storm : GfsModels::Sectors::World;
        } else if (key == "overlays") {
            def.overlays = toBool(value);
        } else if (key == "list") {
            def.recipeList = toBool(value);
        } else if (key == "on_missing") {
            def.onMissing = value == "main_run" ? GfsModels::Missing::MainRun : value == "previous_cycle" ? GfsModels::Missing::PreviousCycle
                : value == "previous_run" ? GfsModels::Missing::PreviousRun : GfsModels::Missing::Nothing;
        } else if (key == "preload_hours") {
            if (!toInt(value, n)) {
                reader.fail("preload_hours is a whole number");
                break;
            }
            def.preloadHours = n;
        } else if (key == "run_length") {   // every 6 : 84 , else : 18
            runLength = value;
        } else if (key == "clone") {
            def.clone.enabled = toBool(value);
        } else if (key == "clone_ids") {
            addList(def.clone.ids, value);
        } else if (key == "clone_variables") {
            std::vector<std::string> list;
            addList(list, value);
            def.clone.variables.insert(list.begin(), list.end());
        } else if (key == "clone_levels") {
            std::vector<std::string> list;
            addList(list, value);
            def.clone.levels.insert(list.begin(), list.end());
        } else if (key == "clone_fields") {
            std::vector<std::string> list;
            addList(list, value);
            def.clone.fields.insert(list.begin(), list.end());
        } else if (key == "clone_skip") {
            addList(def.clone.skip, value);
        } else if (key == "clone_plain") {
            addList(def.clone.plain, value);
        } else if (key == "clone_rename") {
            for (const auto& item : split(value, ',')) {
                const auto colon = item.find(':');
                if (colon == std::string::npos) {
                    reader.fail("clone_rename is FROM:TO");
                    break;
                }
                def.clone.rename[item.substr(0, colon)] = item.substr(colon + 1);
            }
        } else if (key == "clone_prefix") {
            def.clone.labelPrefix = value;
        } else if (key == "clone_pieces") {
            def.clone.pieces = toBool(value);
        } else if (key == "clone_specific_humidity") {
            def.clone.specificHumidity = toBool(value);
        } else {
            reader.fail("unknown key " + key);
            break;
        }
        if (!reader.error.empty()) {
            break;
        }
    }
    if (reader.error.empty()) {
        if (def.id.empty()) {
            reader.line = 0;
            reader.fail("no id");
        } else if (def.label.empty()) {
            reader.line = 0;
            reader.fail("no label");
        } else if (definition->urls.empty() && kind != "wrf") {
            reader.line = 0;
            reader.fail("no url");
        }
    }
    for (const auto& seg : hourSegments) {   // "0-192/3"
        const auto slash = seg.find('/');
        const auto dash = seg.find('-');
        int from = 0, to = 0, step = 1;
        if (dash == std::string::npos || !toInt(seg.substr(0, dash), from) || !toInt(seg.substr(dash + 1, slash == std::string::npos ? std::string::npos : slash - dash - 1), to) ||
            (slash != std::string::npos && !toInt(seg.substr(slash + 1), step)) || step < 1 || to < from) {
            reader.line = 0;
            reader.fail("hours is from-to/step, like 0-192/3");
            break;
        }
        def.hours.push_back({from, to, step});
    }
    if (reader.error.empty() && def.hours.empty()) {
        reader.line = 0;
        reader.fail("no hours");
    }
    if (reader.error.empty() && !runLength.empty()) {   // "every 6 : 84 , else : 18"
        QRegularExpression re{"^every\\s+(\\d+)\\s*:\\s*(\\d+)\\s*,\\s*else\\s*:\\s*(\\d+)$"};
        const auto m = re.match(QString::fromStdString(runLength));
        if (m.hasMatch()) {
            const int every = m.captured(1).toInt(), whole = m.captured(2).toInt(), other = m.captured(3).toInt();
            def.runLength = [every, whole, other] (int cycle) { return cycle % every == 0 ? whole : other; };
        } else {   // "0:240, 12:120, else:30": the last hour of the run of each cycle
            std::map<int, int> byCycle;
            int other = -1;
            bool ok = true;
            for (const auto& item : split(runLength, ',')) {
                const auto colon = item.find(':');
                int amount = 0;
                if (colon == std::string::npos || !toInt(trim(item.substr(colon + 1)), amount)) {
                    ok = false;
                    break;
                }
                const auto left = trim(item.substr(0, colon));
                int cycle = 0;
                if (left == "else") {
                    other = amount;
                } else if (toInt(left, cycle)) {
                    byCycle[cycle] = amount;
                } else {
                    ok = false;
                    break;
                }
            }
            if (!ok || other < 0) {
                reader.line = 0;
                reader.fail("run_length is `every 6 : 84 , else : 18` or `0:240, 12:120, else:30`");
            } else {
                def.runLength = [byCycle, other] (int cycle) {
                    const auto found = byCycle.find(cycle);
                    return found == byCycle.end() ? other : found->second;
                };
            }
        }
    }
    if (!reader.error.empty()) {
        result.error = reader.error;
        return result;
    }
    // the source: made for each call (a model that follows a storm has the storm in its urls and its name), with what the definition says in the functions
    source.label = sourceLabel.empty() ? def.label : sourceLabel;
    source.wholeFile = kind != "grib";
    source.netcdf = kind == "netcdf";
    source.om.enabled = kind == "om";
    source.wrf.enabled = kind == "wrf";
    source.custom = kind == "custom";
    const bool candidatesWanted = std::count_if(definition->urls.begin(), definition->urls.end(), [] (const Rule& r) { return r.conditions.empty(); }) > 1 || std::any_of(definition->urls.begin(), definition->urls.end(), [] (const Rule& r) { return !Servers::nameIn(r.pattern).empty(); });
    const bool ecmwf = indexFormat == "ecmwf";
    const std::string id = def.id;
    def.source = [definition, source, candidatesWanted, ecmwf, stormModel, id, wrfFoldersPref] (const std::string& storm) {
        auto d = std::make_shared<Definition>(*definition);
        d->storm = storm;
        GfsData::Source made = source;
        made.id = stormModel ? id + "-" + storm : id;
        if (!wrfFoldersPref.empty()) {   // the folders of the runs, as the user's settings say
            const auto chosen = Utility::readPref(wrfFoldersPref, "");
            if (!chosen.empty()) {
                made.wrf.folders.clear();
                std::stringstream parts{chosen};
                std::string folder;
                while (std::getline(parts, folder, ';')) {
                    if (!trim(folder).empty()) {
                        made.wrf.folders.push_back(trim(folder));
                    }
                }
            }
        }
        if (!d->rules.empty()) {
            made.fileOf = [d] (const Want& w) {
                for (const auto& rule : d->rules) {
                    const bool all = std::all_of(rule.conditions.begin(), rule.conditions.end(), [&w] (const Condition& c) { return c.matches(w); });
                    if (all) {
                        return expandWant(rule.pattern, w);
                    }
                }
                return std::string{};
            };
        }
        if (!d->legacyBefore.empty()) {
            made.legacyBefore = d->legacyBefore;
            made.legacyFileUrl = [d] (const Run& run, int hour, const std::string&) {
                const int cycle = std::atoi(run.cycle.c_str());
                return expandUrl(d->legacyUrl, *d, run, hour, {}, padded((cycle + 18) % 24, 2));
            };
        }
        made.fileUrl = [d] (const Run& run, int hour, const std::string& file) {
            const auto urls = urlsOf(*d, run, hour, file);
            return urls.empty() ? std::string{} : urls.front();
        };
        if (candidatesWanted) {
            made.candidates = [d] (const Run& run, int hour, const std::string& file) { return urlsOf(*d, run, hour, file); };
        }
        if (!d->tarMember.empty()) {
            made.tarMember = [d] (const Run& run, int hour) {
                const int cycle = std::atoi(run.cycle.c_str());
                return expandUrl(d->tarMember, *d, run, hour, {}, padded((cycle + 18) % 24, 2));
            };
        }
        if (d->partial == "geos-opendap") {
            made.partial = [] (const Run& run, int hour, const std::string& file, const std::string& variable, GfsGrid::Grid& out, std::string& error) {
                return GeosOpendap::read(run, hour, file, variable, out, error);
            };
        }
        if (ecmwf) {
            made.parseIndex = [] (const std::string& text, const Want& want) {
                // an ensemble member by its number ("e07" is member 7); no stat is the plain forecast
                std::string member;
                if (want.stat.size() > 1 && want.stat[0] == 'e') {
                    member = std::to_string(std::atoi(want.stat.c_str() + 1));
                }
                return GfsGrid::parseEcmwfIndex(text, member);
            };
        }
        if (!d->variables.empty()) {
            made.variableOf = [d] (const Want& w) {
                for (const auto& rule : d->variables) {
                    const bool all = std::all_of(rule.conditions.begin(), rule.conditions.end(), [&w] (const Condition& c) { return c.matches(w); });
                    if (all) {
                        auto name = expandWant(rule.pattern, w);
                        if (name.size() > 4 && name.compare(name.size() - 4, 4, "@lev") == 0) {   // (the level is a band of the variable: ncBand)
                            name.erase(name.size() - 4);
                        }
                        return name;
                    }
                }
                return w.variable;
            };
            if (!d->ncLevels.empty()) {   // a variable of a collection that holds every level as the bands of one variable (NASA's 3 hourly pressure levels: `H@lev`): the band of the level
                made.ncBand = [d] (const Run&, int, const Want& w) {
                    for (const auto& rule : d->variables) {
                        const bool all = std::all_of(rule.conditions.begin(), rule.conditions.end(), [&w] (const Condition& c) { return c.matches(w); });
                        if (!all) {
                            continue;
                        }
                        const auto name = expandWant(rule.pattern, w);
                        if (name.size() <= 4 || name.compare(name.size() - 4, 4, "@lev") != 0) {
                            return 0;
                        }
                        const double level = std::atof(w.level.c_str());
                        for (size_t i = 0; i < d->ncLevels.size(); i++) {
                            if (std::abs(d->ncLevels[i] - level) < 1e-6) {
                                return static_cast<int>(i) + 1;
                            }
                        }
                        return 0;
                    }
                    return 0;
                };
            }
        }
        if (!d->offsets.empty()) {
            made.valueOffset = [d] (const Want& w) {
                for (const auto& [variable, amount] : d->offsets) {
                    if (variable == w.variable) {
                        return amount;
                    }
                }
                return 0.0;
            };
        }
        if (!d->scales.empty()) {
            made.valueScale = [d] (const Want& w) {
                for (const auto& [variable, factor] : d->scales) {
                    if (variable == w.variable) {
                        return factor;
                    }
                }
                return 1.0;
            };
        }
        if (!d->memberBand.empty()) {
            made.memberBand = [d] (const std::string& stat) {
                const auto found = d->memberBand.find(stat);
                return found == d->memberBand.end() ? 0 : found->second;
            };
        }
        if (!d->subsetUrl.empty()) {   // a box of the fields cut out by the server (NOMADS's grib filter)
            made.subsetUrl = [d] (const Run& run, int hour, const std::vector<std::string>& variables, const std::vector<std::string>& levels, const GfsData::Box& box) {
                std::string url = d->subsetUrl;
                std::string vars, levs;
                for (const auto& v : variables) {
                    auto one = d->subsetVariable;
                    replaceAll(one, "{v}", v);
                    vars += one;
                }
                for (auto l : levels) {
                    std::replace(l.begin(), l.end(), ' ', '_');
                    auto one = d->subsetLevel;
                    replaceAll(one, "{l}", l);
                    levs += one;
                }
                replaceAll(url, "{variables}", vars);
                replaceAll(url, "{levels}", levs);
                replaceAll(url, "{west}", std::to_string(static_cast<int>(box.west)));
                replaceAll(url, "{east}", std::to_string(static_cast<int>(box.east)));
                replaceAll(url, "{north}", std::to_string(static_cast<int>(box.north)));
                replaceAll(url, "{south}", std::to_string(static_cast<int>(box.south)));
                replaceAll(url, "{date}", run.date);
                replaceAll(url, "{cycle}", run.cycle);
                replaceAll(url, "{hour3}", padded(hour, 3));
                return url;
            };
        }
        return made;
    };
    def.storm = stormModel;
    result.def = def;
    result.ok = true;
    return result;
}

void GfsModelFile::applyAbout(GfsModels::Def& def, const std::string& text) {
    std::istringstream in{text};
    std::string raw;
    while (std::getline(in, raw)) {
        const auto line = trim(raw);
        if (line.compare(0, 6, "why = ") == 0) {
            def.why = trim(line.substr(6));
        } else if (line.compare(0, 7, "link = ") == 0) {
            const auto parts = split(line.substr(7), '|');
            if (parts.size() == 3) {
                def.links.push_back({parts[0], parts[1], parts[2]});
            }
        }
    }
}

std::vector<GfsModelFile::Parsed> GfsModelFile::loadFolder(const std::string& folder) {
    std::vector<Parsed> out;
    if (folder.empty()) {
        return out;
    }
    const QDir dir{QString::fromStdString(folder)};
    for (const auto& info : dir.entryInfoList({"*.model"}, QDir::Files, QDir::Name)) {
        QFile file{info.absoluteFilePath()};
        if (!file.open(QIODevice::ReadOnly)) {
            Parsed bad;
            bad.origin = info.fileName().toStdString();
            bad.error = "could not read the file";
            out.push_back(bad);
            continue;
        }
        const auto text = file.readAll().toStdString();
        if (const auto target = overrideTarget(text); !target.empty()) {
            Parsed over;
            over.origin = info.fileName().toStdString();
            over.overrideId = target;
            over.text = text;
            out.push_back(over);
            continue;
        }
        out.push_back(parse(text, info.fileName().toStdString()));
    }
    return out;
}

namespace {
    struct TextLine {
        std::string raw, key, value;
        bool data{false};   // a `key = value` line (not a comment or a blank)
    };
    std::vector<TextLine> lines(const std::string& text) {
        std::vector<TextLine> out;
        std::istringstream in{text};
        std::string raw;
        while (std::getline(in, raw)) {
            TextLine l;
            l.raw = raw;
            const auto t = trim(raw);
            const auto eq = t.find('=');
            if (!t.empty() && t[0] != '#' && eq != std::string::npos) {
                l.data = true;
                l.key = trim(t.substr(0, eq));
                std::transform(l.key.begin(), l.key.end(), l.key.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
                l.value = trim(t.substr(eq + 1));
            }
            out.push_back(std::move(l));
        }
        return out;
    }
    // "when <condition> => pattern": the condition ("" when the value has none)
    std::string conditionOf(const std::string& value) {
        if (value.compare(0, 5, "when ") != 0) {
            return {};
        }
        const auto arrow = value.find("=>");
        return arrow == std::string::npos ? std::string{} : trim(value.substr(5, arrow - 5));
    }
}

std::string GfsModelFile::overrideTarget(const std::string& text) {
    for (const auto& l : lines(text)) {
        if (l.data && l.key == "override") {
            return l.value;
        }
    }
    return {};
}

std::string GfsModelFile::builtInText(const std::string& id) {
    for (const auto& [name, text] : builtIn()) {
        if (name == id) {
            return withIncludes(text);
        }
    }
    return {};
}

bool GfsModelFile::mergeOverride(const std::string& base, const std::string& over, std::string& merged, std::string& why) {
    static const std::set<std::string> forbidden{"id", "kind", "label", "include", "override"};
    auto baseLines = lines(base);
    std::map<std::string, std::vector<std::string>> replace;   // key -> the override's values, in order
    std::vector<std::string> order;
    std::vector<std::string> prepend, append;
    int number = 0;
    for (const auto& l : lines(over)) {
        number++;
        if (!l.data || l.key == "override") {
            continue;
        }
        if (forbidden.count(l.key) > 0 || l.key.compare(0, 5, "clone") == 0) {
            why = "line " + std::to_string(number) + ": `" + l.key + "` cannot be overridden (the id, kind, label and the clone rules belong to the built-in model)";
            return false;
        }
        if (l.key == "url_prepend") {
            prepend.push_back(l.value);
        } else if (l.key == "url_append") {
            append.push_back(l.value);
        } else {
            if (replace.find(l.key) == replace.end()) {
                order.push_back(l.key);
            }
            replace[l.key].push_back(l.value);
        }
    }
    std::vector<std::string> out;
    const auto put = [&out] (const std::string& key, const std::string& value) { out.push_back(key + " = " + value); };
    std::set<std::string> done;
    // keys whose every override line is a `when` rule with a condition the built-in text has: those lines are replaced one by one
    std::map<std::string, std::map<std::string, std::string>> perRule;   // key -> condition -> new value
    for (const auto& key : order) {
        bool all = true;
        for (const auto& v : replace[key]) {
            const auto c = conditionOf(v);
            bool known = false;
            for (const auto& b : baseLines) {
                known = known || (b.data && b.key == key && !c.empty() && conditionOf(b.value) == c);
            }
            all = all && known;
        }
        if (all) {
            for (const auto& v : replace[key]) {
                perRule[key][conditionOf(v)] = v;
            }
        }
    }
    bool urlSeen = false;
    size_t lastUrl = 0;
    for (size_t i = 0; i < baseLines.size(); i++) {
        if (baseLines[i].data && baseLines[i].key == "url") {
            lastUrl = i;
        }
    }
    for (size_t i = 0; i < baseLines.size(); i++) {
        const auto& b = baseLines[i];
        if (!b.data) {
            out.push_back(b.raw);
            continue;
        }
        if (b.key == "url" && !urlSeen) {   // the places to try first go before the first url
            urlSeen = true;
            for (const auto& v : prepend) {
                put("url", v);
            }
        }
        const auto pr = perRule.find(b.key);
        if (pr != perRule.end()) {
            const auto found = pr->second.find(conditionOf(b.value));
            out.push_back(found != pr->second.end() && !conditionOf(b.value).empty() ? b.key + " = " + found->second : b.raw);
        } else if (replace.count(b.key) > 0) {   // the key is replaced as a set: its lines go in the place of the first one
            if (done.insert(b.key).second) {
                for (const auto& v : replace[b.key]) {
                    put(b.key, v);
                }
            }
        } else {
            out.push_back(b.raw);
        }
        if (i == lastUrl) {
            for (const auto& v : append) {
                put("url", v);
            }
        }
    }
    for (const auto& key : order) {   // a key the built-in text does not have is added at the end
        if (perRule.count(key) == 0 && done.insert(key).second) {
            for (const auto& v : replace[key]) {
                put(key, v);
            }
        }
    }
    if (!baseLines.empty() && !urlSeen && (!prepend.empty() || !append.empty())) {   // no url in the model: the places are its first
        for (const auto& v : prepend) {
            put("url", v);
        }
        for (const auto& v : append) {
            put("url", v);
        }
    }
    merged.clear();
    for (const auto& l : out) {
        merged += l + "\n";
    }
    return true;
}
