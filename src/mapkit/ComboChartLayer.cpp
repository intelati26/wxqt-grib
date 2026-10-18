// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ComboChartLayer.h"
#include <algorithm>
#include <map>
#include <set>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/ProductPicker.h"
#include "util/Utility.h"

namespace {
    // each model that can be in the stack: its grid spacing in km (the order of the stack), the days it covers (yyyyMMdd; "" no end: to the present) and whether it is in unless left out
    struct Entry {
        const char * id;
        double km;
        const char * from;
        const char * to;
        bool on;
    };
    const std::vector<Entry>& table() {
        static const std::vector<Entry> list{
            {"GFS1-ANL", 111.0, "20040304", "20200515", true}, {"GFS-ANL", 55.0, "20070101", "20140630", true}, {"NARR", 32.0, "19790101", "20140930", true},
            {"ERA5", 28.0, "19400101", "20230331", true}, {"IFS", 28.0, "20230125", "", true}, {"GFS", 28.0, "20210222", "", true}, {"GEOS-FP", 28.0, "20200101", "", false},
            {"AIGFS", 28.0, "20250101", "", false}, {"AIFS", 28.0, "20250101", "", false}, {"GEFS", 55.0, "20170101", "", false},
            {"RAP-ANL", 13.0, "20050105", "", true}, {"NAM-ANL", 12.0, "20040304", "20200415", true},
            {"HRRR", 3.0, "20140730", "", true}, {"RRFS", 3.0, "20240101", "", true}, {"ICON-EU", 6.5, "20240101", "", true}, {"HRRR-AK", 3.0, "20180101", "", true}, {"RRFS-NA", 13.0, "20240101", "", true}, {"RRFS-AK", 3.0, "20240101", "", true},
            {"RRFS-HI", 2.5, "20240101", "", true}, {"RRFS-PR", 2.5, "20240101", "", true}, {"NBM", 2.5, "20200518", "", true}, {"HRDPS", 2.5, "20240101", "", true}, {"ICON-D2", 2.2, "20240101", "", true},
            {"RTMA", 2.5, "20130319", "", true}, {"URMA", 2.5, "20140129", "", true}};
        return list;
    }

    // whether a model has the day (UTC)
    bool covers(const Entry& e, const QDate& day) {
        return day >= QDate::fromString(e.from, "yyyyMMdd") && (e.to[0] == '\0' || day <= QDate::fromString(e.to, "yyyyMMdd"));
    }

    vector<string> splitList(const string& text) {
        vector<string> out;
        for (size_t at = 0; at < text.size();) {
            auto end = text.find(',', at);
            end = end == string::npos ? text.size() : end;
            if (end > at) {
                out.push_back(text.substr(at, end - at));
            }
            at = end + 1;
        }
        return out;
    }
}

ComboChartLayer::ComboChartLayer() {
    param = Utility::readPref("MM_COMBO_PARAM", "500_wnd_ht");
    excluded = splitList(Utility::readPref("MM_COMBO_EXCLUDED", ""));
    included_ = splitList(Utility::readPref("MM_COMBO_INCLUDED", ""));
}

bool ComboChartLayer::isOn(const string& model) const {
    for (const auto& e : table()) {
        if (model == e.id) {
            return e.on ? std::find(excluded.begin(), excluded.end(), model) == excluded.end() : std::find(included_.begin(), included_.end(), model) != included_.end();
        }
    }
    return false;
}

double ComboChartLayer::kmOf(const string& model) {
    for (const auto& e : table()) {
        if (model == e.id) {
            return e.km;
        }
    }
    return 0.0;
}

vector<std::pair<string, double>> ComboChartLayer::modelsWith(const string& param, const QDate& day) {
    vector<std::pair<string, double>> out;
    for (const auto& e : table()) {
        if (covers(e, day) && GfsModels::find(e.id) != nullptr && GfsRender::drawsModel(e.id) && GfsChart::product(param, e.id) != nullptr) {
            out.emplace_back(e.id, e.km);
        }
    }
    std::stable_sort(out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
    return out;
}

// the day the picture is for: the time asked for, or today
QDate ComboChartLayer::dayNow() const {
    return asOf.isValid() ? asOf.date() : QDateTime::currentDateTimeUtc().date();
}

// the stack for the chart: a layer of the model chart for each model that has it and has not been left out
void ComboChartLayer::build(MapHost& host) {
    parts.clear();
    names.clear();
    kilometers.clear();
    for (const auto& [model, km] : modelsWith(param, dayNow())) {
        if (!isOn(model)) {
            continue;
        }
        auto part = std::make_unique<ModelChartLayer>();
        part->configure(model, param, 100);
        names.push_back(model);
        kilometers.push_back(km);
        parts.push_back(std::move(part));
    }
    for (auto& part : parts) {
        if (asOf.isValid()) {
            part->setAsOf(asOf, host);
        }
        part->start(host);
        if (shown != 0) {
            part->showTime(shown, host);
        }
    }
    host.timesChanged();
    host.redraw();
}

void ComboChartLayer::refresh(MapHost& host) {
    if (parts.empty()) {
        build(host);
        return;
    }
    for (auto& part : parts) {
        part->refresh(host);
    }
}

void ComboChartLayer::optionChanged(MapHost& host) {
    shown = 0;
    build(host);
}

void ComboChartLayer::paint(QPainter& painter, MapHost& host) {
    for (auto& part : parts) {   // the coarse first
        painter.save();
        part->paint(painter, host);
        painter.restore();
    }
}

MapHit ComboChartLayer::pick(const QPointF& pixels, MapHost& host) const {
    for (size_t i = parts.size(); i > 0; i--) {   // the finest that has a value there
        auto hit = parts[i - 1]->pick(pixels, host);
        if (hit.valid()) {
            hit.text = QString::fromStdString(names[i - 1] + " (" + (kilometers[i - 1] < 10 ? std::to_string(kilometers[i - 1]).substr(0, 3) : std::to_string(static_cast<int>(kilometers[i - 1]))) + " km)\n") + hit.text;
            return hit;
        }
    }
    return {};
}

vector<MapLegendRow> ComboChartLayer::legend() const {
    for (size_t i = parts.size(); i > 0; i--) {
        auto rows = parts[i - 1]->legend();
        if (!rows.empty()) {
            return rows;
        }
    }
    return {};
}

string ComboChartLayer::summary() const {
    string text;
    for (size_t i = 0; i < parts.size(); i++) {
        text += (text.empty() ? "" : ", ") + names[i];
    }
    return text.empty() ? string{"looking for the models"} : "Combo: " + text + " (finest on top)";
}

string ComboChartLayer::notice() const {
    string text;
    for (size_t i = 0; i < parts.size(); i++) {
        const auto note = parts[i]->notice();
        if (!note.empty()) {
            text += (text.empty() ? "" : "\n") + names[i] + ": " + note;
        }
        const auto line = parts[i]->summary();
        if (!line.empty() && line.find("no ") != string::npos) {   // "no HRRR run found": a model that has nothing for the time is left out of the picture
            text += (text.empty() ? "" : "\n") + names[i] + ": " + line;
        }
    }
    return text;
}

vector<long> ComboChartLayer::times() const {
    std::set<long> all;
    for (const auto& part : parts) {
        for (const long t : part->times()) {
            all.insert(t);
        }
    }
    return {all.begin(), all.end()};
}

void ComboChartLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    for (auto& part : parts) {
        part->showTime(seconds, host);
    }
}

string ComboChartLayer::timeText() const {
    for (size_t i = parts.size(); i > 0; i--) {
        const auto text = parts[i - 1]->timeText();
        if (!text.empty()) {
            return "Combo  " + text;
        }
    }
    return {};
}

void ComboChartLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    // the models that have the time are not the ones that have today: the stack is made again for the day
    asOf = utc.toUTC();
    shown = static_cast<long>(asOf.toSecsSinceEpoch());
    build(host);
}

void ComboChartLayer::prepareTimes(MapHost& host) {
    for (auto& part : parts) {
        part->prepareTimes(host);
    }
}

QWidget * ComboChartLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * chartButton = new QPushButton{widget};
    layout->addWidget(chartButton);
    auto * models = new QWidget{widget};
    auto * modelColumn = new QVBoxLayout{models};
    modelColumn->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(models);
    // the boxes of the models that have the chart
    const auto refill = [this, chartButton, models, modelColumn, changed] {
        const auto * product = [&] () -> const GfsChart::Product * {
            for (const auto& [model, km] : modelsWith(param, dayNow())) {
                return GfsChart::product(param, model);
            }
            return nullptr;
        }();
        chartButton->setText(QString::fromStdString("Chart: " + (product != nullptr ? product->label : param) + "  \\xE2\\x96\\xBE"));
        while (auto * item = modelColumn->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        for (const auto& [model, km] : modelsWith(param, dayNow())) {
            auto * box = new QCheckBox{QString::fromStdString(model + "  (" + (km < 10 ? std::to_string(km).substr(0, 3) : std::to_string(static_cast<int>(km))) + " km)"), models};
            box->setChecked(isOn(model));
            QObject::connect(box, &QCheckBox::toggled, [this, model = model, changed] (bool on) {
                bool dflt = true;
                for (const auto& e : table()) {
                    if (model == e.id) {
                        dflt = e.on;
                    }
                }
                auto& list = dflt ? excluded : included_;   // the list of the ones that differ from the default
                list.erase(std::remove(list.begin(), list.end(), model), list.end());
                if (on != dflt) {
                    list.push_back(model);
                }
                const auto join = [] (const std::vector<string>& l) { string out; for (const auto& i : l) { out += (out.empty() ? "" : ",") + i; } return out; };
                Utility::writePref("MM_COMBO_EXCLUDED", join(excluded));
                Utility::writePref("MM_COMBO_INCLUDED", join(included_));
                changed();
            });
            modelColumn->addWidget(box);
        }
    };
    refill();
    QObject::connect(chartButton, &QPushButton::clicked, [this, widget, refill, changed] {
        // the charts that more than one model has
        std::map<string, std::set<string>> have;
        std::map<string, const GfsChart::Product *> first;
        for (const auto& e : table()) {
            const string model = e.id;
            if (!covers(e, dayNow()) || GfsModels::find(model) == nullptr || !GfsRender::drawsModel(model)) {
                continue;
            }
            for (const auto& product : GfsChart::products()) {
                if (product.source == model && !product.id.empty() && product.id.compare(0, 4, "gen~") != 0) {
                    have[product.id].insert(model);
                    first.emplace(product.id, &product);
                }
            }
        }
        std::vector<ProductPicker::Entry> entries;
        for (const auto& [id, set] : have) {
            if (set.size() >= 2) {
                entries.push_back({id, first[id]->label, GfsChart::category(*first[id])});
            }
        }
        auto * picker = new ProductPicker{widget->window(), "combo", entries, param, {}, {}, {}};
        picker->setAttribute(Qt::WA_DeleteOnClose);
        picker->onPick = [this, refill, changed] (const string& id) {
            param = id;
            Utility::writePref("MM_COMBO_PARAM", id);
            refill();
            changed();
        };
        picker->show();
    });
    return widget;
}
