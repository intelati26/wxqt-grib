// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/MrmsVerifyViewer.h"
#include <algorithm>
#include <cmath>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "gfs/MrmsVerify.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"

using std::string;
using std::vector;

namespace {
    const vector<double> thresholds{20.0, 35.0, 50.0};
    const vector<double> radii{10.0, 25.0, 50.0};
}

struct MrmsVerifyViewer::Row {
    string run;   // yyyyMMddHH
    int lead{0};
    string error;
    MrmsVerify::Result result;
    GfsGrid::Grid model, observed;
    MrmsVerify::Box box;
};

MrmsVerifyViewer::MrmsVerifyViewer(Window * parent, const string& modelName, const string& paramName, const string& sectorName, const QDateTime& validUtc)
    : Window{parent}, model{modelName}, param{paramName}, sector{sectorName} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Verify against MRMS - " + model);
    auto * page = new QWidget{this};
    auto * column = new QVBoxLayout{page};
    auto * top = new QHBoxLayout;
    top->addWidget(new QLabel{QString::fromStdString(model + "  " + param + "  - valid (UTC):"), page});
    timePick = new QDateTimeEdit{validUtc.isValid() ? validUtc.toUTC() : QDateTime::currentDateTimeUtc().addSecs(-3 * 3600), page};
    timePick->setTimeSpec(Qt::UTC);
    timePick->setDisplayFormat("yyyy-MM-dd HH':00'");
    timePick->setCalendarPopup(true);
    timePick->setToolTip("The time to compare at, on the hour (UTC). Every run of the model that has a forecast for it is scored against MRMS.");
    top->addWidget(timePick);
    hourMax = new QCheckBox{"Maximum of the hour (MRMS)", page};
    hourMax->setToolTip("Compare with the highest MRMS value of the hour that ends at this time, for the model fields that are the maximum of an hour (hourly maximum reflectivity). Off: the MRMS scan nearest the time.");
    top->addWidget(hourMax);
    top->addWidget(new QLabel{"Threshold:", page});
    thresholdPick = new QComboBox{page};
    for (const double t : thresholds) {
        thresholdPick->addItem(QString::number(t, 'f', 0) + " dBZ", t);
    }
    thresholdPick->setCurrentIndex(1);
    thresholdPick->setToolTip("The reflectivity that counts as an echo for the scores and the agreement picture.");
    top->addWidget(thresholdPick);
    go = UiStandards::button(page, "Compare");
    top->addWidget(go);
    top->addStretch(1);
    column->addLayout(top);
    status = new QLabel{"Pick a time and press Compare.", page};
    status->setWordWrap(true);
    column->addWidget(status);
    table = new QTableWidget{0, 9, page};
    table->setHorizontalHeaderLabels({"Run", "Lead", "POD", "FAR", "CSI", "Bias", "FSS 10 km", "FSS 25 km", "FSS 50 km"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMaximumHeight(230);
    table->setToolTip("POD: the observed echoes the model had. FAR: the model's echoes that were not observed. CSI: hits over hits, misses and false alarms. Bias: the model's echo area over the observed one. FSS: Fractions Skill Score over a window of that size (1 is perfect; a storm a little off still counts).");
    column->addWidget(table);
    picture = new QLabel{page};
    picture->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    column->addWidget(picture, 1);
    setCentralWidget(page);
    setSize(1300, 800);
    QObject::connect(go, &QPushButton::clicked, this, [this] { compare(); });
    QObject::connect(table, &QTableWidget::itemSelectionChanged, this, [this] { showRow(); });
    QObject::connect(thresholdPick, &QComboBox::currentIndexChanged, this, [this] { fillTable(); showRow(); });
    QWidget::show();
    if (qEnvironmentVariableIsSet("WXQT_VERIFY_GO")) {   // (for testing: compare at once)
        compare();
        if (const auto file = qEnvironmentVariable("WXQT_VERIFY_PNG"); !file.isEmpty()) {
            QTimer::singleShot(qEnvironmentVariableIntValue("WXQT_VERIFY_MS") > 0 ? qEnvironmentVariableIntValue("WXQT_VERIFY_MS") : 60000, this, [this, file] { grab().save(file); });
        }
    }
}

void MrmsVerifyViewer::compare() {
    const int mine = ++generation;
    const auto valid = timePick->dateTime().toUTC();
    const QDateTime hour{valid.date(), QTime{valid.time().hour(), 0}, Qt::UTC};
    const bool wholeHour = hourMax->isChecked();
    const auto modelName = model, paramName = param, sectorName = sector;
    const auto * def = GfsModels::find(modelName);
    if (def == nullptr || def->hours.empty()) {
        status->setText("No such model.");
        return;
    }
    // the runs with a forecast for the time: the latest at or before it, then back one cycle at a time while the lead is inside the model's hours
    const int cycle = std::max(1, def->source("").cycleHours);
    int longest = 0;
    for (const auto& h : def->hours) {
        longest = std::max(longest, h.to);
    }
    vector<std::pair<string, int>> wanted;
    auto run = QDateTime{hour.date(), QTime{hour.time().hour() / cycle * cycle, 0}, Qt::UTC};
    const auto published = QDateTime::currentDateTimeUtc().addSecs(-3600LL * def->source("").lagHours);   // a run is there from its lag after its time: the newer ones would be another run's
    for (; wanted.size() < 8 && run.secsTo(hour) / 3600 <= longest; run = run.addSecs(-3600LL * cycle)) {
        if (run > published) {
            continue;
        }
        const int lead = static_cast<int>(run.secsTo(hour) / 3600);
        bool offered = false;
        for (const auto& h : def->hours) {
            offered = offered || (lead >= h.from && lead <= h.to && (lead - h.from) % std::max(1, h.step) == 0);
        }
        if (offered) {
            wanted.emplace_back(run.toString("yyyyMMddHH").toStdString(), lead);
        }
    }
    if (wanted.empty()) {
        status->setText("The model has no run with a forecast for that time.");
        return;
    }
    go->setEnabled(false);
    status->setText("Reading the MRMS scan and " + QString::number(wanted.size()) + " model runs...");
    table->setRowCount(0);
    picture->clear();
    rows.clear();
    auto result = std::make_shared<vector<std::shared_ptr<Row>>>();
    auto note = std::make_shared<string>();
    new FutureVoid{this,
        [result, note, wanted, hour, wholeHour, modelName, paramName, sectorName] {
            // MRMS: the scan nearest the time, or the scans of the hour that ends at it (every third: the maximum does not need them all)
            const auto products = UtilityMrms::products();
            const auto product = std::find_if(products.begin(), products.end(), [] (const auto& p) { return p.id == "MergedReflectivityQCComposite"; });
            vector<UtilityMrms::Scan> scans;
            string error;
            if (product == products.end() || !UtilityMrms::scansAwsAt(*product, "00.50", hour, wholeHour ? 60 : 3, wholeHour ? 0 : 3, scans, error) || scans.empty()) {
                *note = "MRMS has no scan for that time" + (error.empty() ? string{} : ": " + error);
                return;
            }
            vector<UtilityMrms::Scan> chosen;
            if (wholeHour) {
                for (size_t i = 0; i < scans.size(); i += 3) {
                    chosen.push_back(scans[i]);
                }
            } else {
                chosen.push_back(*std::min_element(scans.begin(), scans.end(), [&hour] (const auto& a, const auto& b) { return std::abs(a.utc.secsTo(hour)) < std::abs(b.utc.secsTo(hour)); }));
            }
            vector<UtilityMrms::Frame> frames;
            for (const auto& scan : chosen) {
                UtilityMrms::Frame frame;
                if (UtilityMrms::frame(*product, scan, frame, error)) {
                    frames.push_back(std::move(frame));
                }
            }
            if (frames.empty()) {
                *note = "The MRMS scan could not be read: " + error;
                return;
            }
            *note = wholeHour ? "MRMS: the maximum of " + std::to_string(frames.size()) + " scans of the hour ending " + hour.toString("HH:mm").toStdString() + "Z"
                              : "MRMS scan " + frames.front().utc.toString("yyyy-MM-dd HH:mm:ss").toStdString() + "Z";
            GfsRender::Session session;
            for (const auto& [runId, lead] : wanted) {
                auto row = std::make_shared<Row>();
                row->run = runId;
                row->lead = lead;
                if (GfsRender::availableHours(session, modelName, paramName, runId, {lead}).empty()) {   // (the render would take an earlier run's field for the same time and score it twice)
                    row->error = "this run has no file for that hour on the server";
                    result->push_back(row);
                    continue;
                }
                GfsRender::Variant variant;
                variant.fillOut = &row->model;
                string renderError;
                GfsRender::png(session, modelName, paramName, sectorName, runId, lead, {}, renderError, nullptr, variant);
                if (row->model.empty()) {
                    row->error = renderError.empty() ? "no field" : renderError;
                } else {
                    row->box.north = std::min(55.0, row->model.lat0);
                    row->box.south = std::max(20.0, row->model.lat0 - (row->model.rows - 1) * row->model.step);
                    row->box.west = std::max(-130.0, row->model.lon0 > 180.0 ? row->model.lon0 - 360.0 : row->model.lon0);
                    row->box.east = std::min(-60.0, row->box.west + (row->model.columns - 1) * row->model.step);
                    row->observed = MrmsVerify::observed(frames, row->model);
                    row->result = MrmsVerify::score(row->model, row->observed, row->box, thresholds, radii);
                }
                result->push_back(row);
            }
        },
        [this, result, note, mine] {
            if (closed || mine != generation) {
                return;
            }
            go->setEnabled(true);
            observedNote = QString::fromStdString(*note);
            rows = *result;
            if (rows.empty()) {
                status->setText(observedNote);
                return;
            }
            status->setText(observedNote + "  -  " + QString::number(rows.size()) + " runs");
            fillTable();
            table->selectRow(static_cast<int>(rows.size()) - 1);   // the shortest lead first
        }};
}

void MrmsVerifyViewer::fillTable() {
    const double threshold = thresholdPick->currentData().toDouble();
    const int selected = table->currentRow();
    table->blockSignals(true);
    table->setRowCount(static_cast<int>(rows.size()));
    const auto number = [] (double v, int digits) { return std::isfinite(v) ? QString::number(v, 'f', digits) : QString{"-"}; };
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        const auto& row = *rows[static_cast<size_t>(i)];
        QStringList cells{QString::fromStdString(row.run.substr(0, 4) + "-" + row.run.substr(4, 2) + "-" + row.run.substr(6, 2) + " " + row.run.substr(8, 2) + "Z"), "f" + QString::number(row.lead)};
        const auto found = std::find_if(row.result.scores.begin(), row.result.scores.end(), [threshold] (const auto& s) { return s.dbz == threshold; });
        if (found == row.result.scores.end()) {
            table->setItem(i, 0, new QTableWidgetItem{cells[0]});
            table->setItem(i, 1, new QTableWidgetItem{cells[1]});
            table->setItem(i, 2, new QTableWidgetItem{QString::fromStdString(row.error.empty() ? "no data" : row.error)});
            table->setSpan(i, 2, 1, 7);
            continue;
        } else {
            cells << number(found->table.pod(), 2) << number(found->table.far(), 2) << number(found->table.csi(), 2) << number(found->table.bias(), 2);
            for (const double f : found->fss) {
                cells << number(f, 2);
            }
        }
        for (int c = 0; c < 9; c++) {
            table->setItem(i, c, new QTableWidgetItem{c < cells.size() ? cells[c] : QString{}});
        }
    }
    table->blockSignals(false);
    table->resizeColumnToContents(0);
    if (selected >= 0 && selected < table->rowCount()) {
        table->selectRow(selected);
    }
}

void MrmsVerifyViewer::showRow() {
    const int index = table->currentRow();
    if (index < 0 || index >= static_cast<int>(rows.size())) {
        return;
    }
    const auto& row = *rows[static_cast<size_t>(index)];
    if (row.model.empty()) {
        picture->setText(QString::fromStdString(row.error));
        return;
    }
    const auto when = QString::fromStdString(row.run.substr(6, 2) + "/" + row.run.substr(8, 2) + "Z f" + std::to_string(row.lead));
    const auto image = MrmsVerify::picture(row.model, row.observed, row.box, thresholdPick->currentData().toDouble(), QString::fromStdString(model) + " " + when, observedNote.section("  ", 0, 0).left(60), std::max(300, (width() - 40) / 3));
    picture->setPixmap(QPixmap::fromImage(image));
}
