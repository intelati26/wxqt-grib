// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AscatUhrViewer.h"
#include <algorithm>
#include <map>
#include <set>
#include <memory>
#include <QDateTime>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QWidget>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/URL.h"

namespace {
    const string site{"https://www.ospo.noaa.gov/Visualization01/cData/Atmosphere/ASCAT/"};

    string typeLabel(const string& type) {
        if (type == "ave3m") return "Wind map (AVE3, latest iteration)";
        if (type == "ave2m") return "Wind map (AVE2)";
        if (type == "ave") return "Backscatter map (sigma-0)";
        if (type == "ave3") return "The beams of AVE3 (strip of panels)";
        if (type == "ave2") return "The beams of AVE2 (strip of panels)";
        return type;
    }

    QString when(const string& yyyymmddhhmm) {
        const auto t = QDateTime::fromString(QString::fromStdString(yyyymmddhhmm), "yyyyMMddHHmm");
        return t.isValid() ? t.toString("MMM d HH:mm") : QString::fromStdString(yyyymmddhhmm);
    }

    // the letters an ATCF basin has in the storm numbers of the list ("29W", "09L")
    string letters(const string& atcfId) {
        const auto basin = atcfId.substr(0, 2);
        return basin == "al" ? "L" : basin == "ep" ? "E" : basin == "cp" ? "C" : basin == "wp" ? "W" : basin == "io" ? "AB" : basin == "sh" ? "SP" : "";
    }
}

AscatUhrViewer::AscatUhrViewer(Window * parent, const string& atcfId)
    : Window{parent}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("ASCAT UHR winds - NOAA OSPO");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    stormBox = new QComboBox{top};
    typeBox = new QComboBox{top};
    for (const string type : {"ave3m", "ave2m", "ave", "ave3", "ave2"}) {
        typeBox->addItem(QString::fromStdString(typeLabel(type)), QString::fromStdString(type));
    }
    topLayout->addWidget(stormBox, 1);
    topLayout->addWidget(typeBox, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    note = new QLabel{"Reading OSPO's list of passes...", this};
    note->setWordWrap(true);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    list = new QListWidget{this};
    list->setFixedWidth(250);
    auto * row = new QWidget{this};
    auto * rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(list);
    rowLayout->addWidget(&image, 1);
    box.addWidgetReal(row, 1, Qt::Alignment{});
    auto * credit = new QLabel{"ASCAT ultra high resolution winds: BYU's AVE (the first iteration of the SIR) retrieval from MetOp-B and MetOp-C ASCAT SZF data, KNMI wind retrieval at each 4.45 km cell. "
                               "Winds in m/s. NOAA / NESDIS OSPO (www.ospo.noaa.gov/products/atmosphere/ascat/uhr/winds/).", this};
    credit->setWordWrap(true);
    credit->setStyleSheet("color: gray;");
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1050, 780);
    QObject::connect(stormBox, &QComboBox::currentIndexChanged, this, [this] (int) { fillPasses(); });
    QObject::connect(typeBox, &QComboBox::currentIndexChanged, this, [this] (int) { fillPasses(); });
    QObject::connect(list, &QListWidget::currentRowChanged, this, [this] (int row) { showPass(row); });
    fill(atcfId);
}

void AscatUhrViewer::fill(const string& atcfId) {
    auto files = std::make_shared<string>();
    auto names = std::make_shared<string>();
    new FutureVoid{this,
        [files, names] {
            *files = URL::getText(site + "assets/uhr_filelist.js");
            *names = URL::getText(site + "assets/uhr_names.js");
        },
        [this, files, names, atcfId] {
            if (closed) {
                return;
            }
            // the two arrays of file names: MetOp-B, then MetOp-C
            const auto text = QString::fromStdString(*files);
            const int cStart = text.indexOf("uhr_filesC");
            static const QRegularExpression name{"\"(SEAP-UHR-([a-z0-9]+)-([0-9]{2}[A-Z])[^\"]*_s([0-9]{12})[0-9]*_e([0-9]{12})[0-9]*_c[0-9]+)\""};
            for (auto m = name.globalMatch(text); m.hasNext();) {
                const auto match = m.next();
                Pass p;
                p.file = match.captured(1).toStdString();
                p.type = match.captured(2).toStdString();
                p.storm = match.captured(3).toStdString();
                p.start = match.captured(4).toStdString();
                p.end = match.captured(5).toStdString();
                p.satellite = cStart >= 0 && match.capturedStart() > cStart ? "MetOp-C" : "MetOp-B";
                passes.push_back(std::move(p));
            }
            if (passes.empty()) {
                note->setText("OSPO's list of ASCAT UHR passes could not be read.");
                return;
            }
            // the storms, named as the page names them (the last name of a number is the newest), with the newest first
            std::map<string, string> nameOf;
            static const QRegularExpression pair{"\"value\":\"([0-9A-Z]+)\",\\s*\"name\":\"([^\"]+)\""};
            for (auto m = pair.globalMatch(QString::fromStdString(*names)); m.hasNext();) {
                const auto match = m.next();
                nameOf[match.captured(1).toStdString()] = match.captured(2).toStdString();
            }
            std::map<string, string> newest;   // storm -> start of its newest pass
            for (const auto& p : passes) {
                if (p.start > newest[p.storm]) {
                    newest[p.storm] = p.start;
                }
            }
            vector<std::pair<string, string>> byTime(newest.begin(), newest.end());
            std::sort(byTime.begin(), byTime.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
            int select = 0, index = 0;
            const string number = atcfId.size() >= 4 ? atcfId.substr(2, 2) : string{};
            stormBox->blockSignals(true);
            for (const auto& [value, start] : byTime) {
                storms.emplace_back(value, nameOf.count(value) ? nameOf[value] : string{});
                stormBox->addItem(QString::fromStdString(value + "  " + storms.back().second + "   (latest pass " + when(start).toStdString() + "Z)"));
                if (!atcfId.empty() && select == 0 && value.substr(0, 2) == number && letters(atcfId).find(value[2]) != string::npos) {
                    select = index;
                }
                index++;
            }
            stormBox->setCurrentIndex(select);
            stormBox->blockSignals(false);
            note->setText(atcfId.empty() ? "" : (select == 0 && !(byTime.empty() == false && byTime[0].first.substr(0, 2) == number && letters(atcfId).find(byTime[0].first[2]) != string::npos)) ?
                              QString::fromStdString("ASCAT UHR has no pass of " + atcfId + " in its list; the newest storm is shown.") : QString{});
            fillPasses();
        }};
}

void AscatUhrViewer::fillPasses() {
    shown.clear();
    list->blockSignals(true);
    list->clear();
    if (stormBox->currentIndex() < 0 || static_cast<size_t>(stormBox->currentIndex()) >= storms.size()) {
        list->blockSignals(false);
        return;
    }
    const string storm = storms[static_cast<size_t>(stormBox->currentIndex())].first;
    const string type = typeBox->currentData().toString().toStdString();
    for (const auto& p : passes) {
        if (p.storm == storm && p.type == type) {
            shown.push_back(p);
        }
    }
    // OSPO keeps every reprocessing of a pass (the c... number differs): the newest of each pass, by satellite and time, with the orbit (the number after the storm) kept apart
    std::sort(shown.begin(), shown.end(), [] (const Pass& a, const Pass& b) { return a.file > b.file; });
    {
        std::set<string> seen;
        vector<Pass> unique;
        for (const auto& p : shown) {
            const auto key = p.satellite + p.start + p.file.substr(0, p.file.rfind("_c"));
            if (seen.insert(key).second) {
                unique.push_back(p);
            }
        }
        shown = unique;
    }
    std::sort(shown.begin(), shown.end(), [] (const Pass& a, const Pass& b) { return a.start != b.start ? a.start > b.start : a.satellite > b.satellite; });
    for (const auto& p : shown) {
        list->addItem(when(p.start) + " - " + when(p.end).right(5) + "Z   " + QString::fromStdString(p.satellite));
    }
    list->blockSignals(false);
    if (shown.empty()) {
        note->setText("No pass of this storm in this kind of picture.");
        return;
    }
    list->setCurrentRow(0);
}

void AscatUhrViewer::showPass(int row) {
    if (row < 0 || static_cast<size_t>(row) >= shown.size()) {
        return;
    }
    const auto& p = shown[static_cast<size_t>(row)];
    note->setText(QString::fromStdString(p.satellite + ", " + p.file));
    new FutureBytes{this, site + p.satellite + "/SEAP_ASCAT_UHR_WINDS_GIF/" + p.file + ".gif", [this] (const auto& bytes) {
        if (!closed && !bytes.isEmpty()) {
            image.setBytes(bytes);
        }
    }};
}
