// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spc/UtilityWatchProbabilities.h"
#include <QRegularExpression>
#include <QStringList>
#include "util/UtilityIO.h"

std::vector<UtilityWatchProbabilities::Hazard> UtilityWatchProbabilities::parse(const QString& page) {
    QString text = page;
    text.remove(QRegularExpression{"<script.*?</script>|<style.*?</style>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption});
    text.replace(QRegularExpression{"<[^>]*>"}, "\n");
    text.replace("&nbsp;", " ").replace("&amp;", "&").replace("&gt;", ">").replace("&lt;", "<");
    QStringList lines;
    for (const auto& line : text.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            lines << line.trimmed();
        }
    }
    static const QStringList groups{"Tornadoes", "Wind", "Hail", "Combined Severe Hail/Wind"};
    std::vector<Hazard> out;
    QString group;
    const QRegularExpression level{R"(^(Low|Mod|High)\s*\(\s*([<>]?\s*[0-9]+)\s*%\s*\))"};
    for (int i = 0; i + 1 < lines.size(); i++) {
        if (groups.contains(lines[i])) {
            group = lines[i];
            continue;
        }
        if (lines[i].startsWith("Probability of ")) {
            const auto m = level.match(lines[i + 1]);
            if (!m.hasMatch()) {
                continue;
            }
            Hazard h;
            h.group = group;
            h.event = lines[i].mid(15);
            h.level = m.captured(1);
            h.percent = m.captured(2).remove(' ');
            h.value = QString{h.percent}.remove('<').remove('>').toInt();
            out.push_back(h);
        }
    }
    return out;
}

std::vector<UtilityWatchProbabilities::Hazard> UtilityWatchProbabilities::fetch(int number) {
    const auto page = QString::fromUtf8(UtilityIO::downloadAsByteArray("https://www.spc.noaa.gov/products/watch/ww" + QString{"%1"}.arg(number, 4, 10, QChar{'0'}).toStdString() + "_prob.html"));
    return parse(page);
}
