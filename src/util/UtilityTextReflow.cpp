// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/UtilityTextReflow.h"
#include <QStringList>
#include "util/Utility.h"

namespace {
    // a line that begins something of its own: a list item, a heading, a table row, an end mark
    bool startsOwnLine(const QString& line) {
        if (line.isEmpty()) {
            return true;
        }
        const QChar first = line[0];
        if (first == ' ' || first == '\t' || first == '*' || first == '-' || first == '|' || first == '>' || first == '.' || first == '&' || first == '$' || first == '#' || first == '=') {
            return true;
        }
        if (first.unicode() == 0x2022) {   // a bullet
            return true;
        }
        // "1. ", "2) ", "12: "
        int digits = 0;
        while (digits < line.size() && line[digits].isDigit()) {
            digits++;
        }
        return digits > 0 && digits <= 2 && digits < line.size() && (line[digits] == '.' || line[digits] == ')') && digits + 1 < line.size() && line[digits + 1] == ' ';
    }

    // a line of a table or a column layout: runs of spaces inside it
    bool columns(const QString& line) {
        return line.trimmed().contains("   ");
    }
}

bool UtilityTextReflow::enabled() {
    return Utility::readPref("TEXT_REFLOW", "true").compare(0, 1, "t") == 0;
}

QString UtilityTextReflow::reflow(const QString& text) {
    if (!text.contains('\n')) {
        return text;
    }
    QStringList lines = QString{text}.remove(QChar{13}).split(QChar{10});
    QString out;
    bool inItem = false;
    bool joined = false;
    for (int i = 0; i < lines.size(); i++) {
        const QString line = lines[i];
        out += joined ? line.trimmed() : line;   // an indented continuation joins with one space
        if (i + 1 == lines.size()) {
            break;
        }
        const QString trimmed = line.trimmed();
        const QString next = lines[i + 1];
        // the lines of a list item ("- text" with its continuation indented two spaces) join too
        const bool item = line.startsWith("- ") || line.startsWith("* ") || (line.startsWith("  ") && !line.startsWith("   ") && inItem);
        const bool continuation = item && next.startsWith("  ") && !next.startsWith("   ") && next.size() > 2 && !next.at(2).isSpace() && next.at(2) != '-' && next.at(2) != '*';
        inItem = item && (continuation || trimmed.size() >= 52);
        // lines wrapped by the writer are full (about 66 to 69 characters): a shorter one ended on purpose
        const bool wrapped = trimmed.size() >= 52 && !columns(line) && !columns(next) && !trimmed.isEmpty() && !trimmed.startsWith('.') && (!startsOwnLine(next) || continuation);
        joined = wrapped;
        if (wrapped) {
            out += ' ';
        } else {
            out += '\n';
        }
    }
    return out;
}
