// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModelFile.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

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

    // {date} {cycle} {prev_cycle} {hour} {hour2} {hour3} {file} {/file} {file3} {dir} {name} {tag} {tail} {storm} {year} {month} {day}
    std::string expandUrl(std::string url, const Definition& d, const Run& run, int hour, const std::string& alt, const std::string& previous) {
        const auto slash = alt.find('/');
        const auto directory = slash == std::string::npos ? std::string{} : alt.substr(0, slash);
        const auto name = slash == std::string::npos ? alt : alt.substr(slash + 1);
        const auto colon = alt.find(':');
        replaceAll(url, "{date}", run.date);
        replaceAll(url, "{year}", run.date.substr(0, 4));
        replaceAll(url, "{month}", run.date.substr(4, 2));
        replaceAll(url, "{day}", run.date.substr(6, 2));
        replaceAll(url, "{cycle}", run.cycle);
        replaceAll(url, "{prev_cycle}", previous);
        if (url.find("{valid") != std::string::npos) {   // the time the file is for: the run's time, the hour and the offset
            auto t = QDateTime::fromString(QString::fromStdString(run.date + run.cycle), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(static_cast<qint64>(hour) * 3600 + static_cast<qint64>(d.validOffsetMinutes) * 60);
            replaceAll(url, "{valid_date}", t.toString("yyyyMMdd").toStdString());
            replaceAll(url, "{valid_hour2}", t.toString("HH").toStdString());
            replaceAll(url, "{valid_minute2}", t.toString("mm").toStdString());
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
        replaceAll(url, "{storm}", d.storm);
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
                out.push_back(expandUrl(chosen->pattern, d, run, hour, alt, previous));
                continue;
            }
            for (const auto& rule : d.urls) {
                if (!rule.conditions.empty()) {
                    continue;
                }
                if (directory.empty() && rule.pattern.find("{dir}") != std::string::npos) {
                    continue;   // a file that is not "directory/name" has no place where the url wants one
                }
                out.push_back(expandUrl(rule.pattern, d, run, hour, alt, previous));
            }
        }
        return out;
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

GfsModelFile::Parsed GfsModelFile::parse(const std::string& text, const std::string& origin) {
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
        } else if (key == "group") {
            def.group = value;
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
            if (value != "grib" && value != "whole" && value != "netcdf" && value != "custom") {
                reader.fail("kind is grib, whole, netcdf or custom");
                break;
            }
            kind = value;
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
            definition->urls.push_back(rule);
        } else if (key == "nc_variable") {
            Rule rule;
            if (!parseRule(value, rule, reader)) {
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
        } else if (key == "valid_offset") {
            if (!toInt(value, n)) {
                reader.fail("valid_offset is minutes");
                break;
            }
            definition->validOffsetMinutes = n;
        } else if (key == "subset_url") {
            definition->subsetUrl = value;
        } else if (key == "subset_variable") {
            definition->subsetVariable = value;
        } else if (key == "subset_level") {
            definition->subsetLevel = value;
        } else if (key == "storm") {
            stormModel = toBool(value);
        } else if (key == "file") {
            Rule rule;
            if (!parseRule(value, rule, reader)) {
                break;
            }
            definition->rules.push_back(rule);
        } else if (key == "file_default") {
            definition->fileDefault = value;
        } else if (key == "index_format") {
            if (value != "noaa" && value != "ecmwf") {
                reader.fail("index_format is noaa or ecmwf");
                break;
            }
            indexFormat = value;
        } else if (key == "index_suffix") {
            source.indexSuffix = value;
        } else if (key == "index_replaces") {
            source.indexReplaces = value;
        } else if (key == "default_detail") {
            source.defaultDetail = value;
        } else if (key == "probe_file") {
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
        } else if (definition->urls.empty()) {
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
    source.custom = kind == "custom";
    const bool candidatesWanted = std::count_if(definition->urls.begin(), definition->urls.end(), [] (const Rule& r) { return r.conditions.empty(); }) > 1;
    const bool ecmwf = indexFormat == "ecmwf";
    const std::string id = def.id;
    def.source = [definition, source, candidatesWanted, ecmwf, stormModel, id] (const std::string& storm) {
        auto d = std::make_shared<Definition>(*definition);
        d->storm = storm;
        GfsData::Source made = source;
        made.id = stormModel ? id + "-" + storm : id;
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
        made.fileUrl = [d] (const Run& run, int hour, const std::string& file) {
            const auto urls = urlsOf(*d, run, hour, file);
            return urls.empty() ? std::string{} : urls.front();
        };
        if (candidatesWanted) {
            made.candidates = [d] (const Run& run, int hour, const std::string& file) { return urlsOf(*d, run, hour, file); };
        }
        if (ecmwf) {
            made.parseIndex = [] (const std::string& text, const Want&) { return GfsGrid::parseEcmwfIndex(text); };
        }
        if (!d->variables.empty()) {
            made.variableOf = [d] (const Want& w) {
                for (const auto& rule : d->variables) {
                    const bool all = std::all_of(rule.conditions.begin(), rule.conditions.end(), [&w] (const Condition& c) { return c.matches(w); });
                    if (all) {
                        return expandWant(rule.pattern, w);
                    }
                }
                return w.variable;
            };
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
        out.push_back(parse(file.readAll().toStdString(), info.fileName().toStdString()));
    }
    return out;
}
