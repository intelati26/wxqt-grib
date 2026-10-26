// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spc/AbpgExtended.h"
#include <memory>
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    const string site{"https://analog.missouri.edu/"};
    const string pref{"ABPG_EXT_PRODUCT"};
    const vector<string> periods{"Days 6-8", "Days 9-11", "Days 12-14"};
    const vector<int> hours{168, 240, 312};   // the pictures' forecast hours: the middle of each period

    struct Product {
        string code;
        string name;
    };
    const vector<Product>& products() {
        static const vector<Product> list{
            {"PRALLC01", "Severe: 1+ reports"},
            {"PRALLC05", "Severe: 5+ reports"},
            {"PTMPA000", "Temperature above / below normal"},
            {"DIFFTMPF", "Temperature anomaly"},
            {"APC72I", "72 h precipitation above / below normal"},
            {"AVGPC72", "72 h precipitation, mean"},
            {"PPC72010", "72 h precipitation over 1.0 in"},
            {"PPC72020", "72 h precipitation over 2.0 in"},
            {"PPC72030", "72 h precipitation over 3.0 in"},
            {"MEDSN72", "Snowfall, median"},
            {"XMAXT080", "High temperature over 80 F"},
            {"XMAXT070", "High temperature over 70 F"},
            {"XMAXT060", "High temperature over 60 F"},
            {"NMINT032", "Low temperature under 32 F"},
            {"NMINT000", "Low temperature under 0 F"},
        };
        return list;
    }

    vector<string> productNames() {
        vector<string> names;
        for (const auto& product : products()) {
            names.push_back(product.name);
        }
        return names;
    }
}

AbpgExtended::AbpgExtended(Window * parent)
    : Window{parent}
    , comboPeriod{this, periods}
    , comboProduct{this, productNames()}
    , textValid{this, ""}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Extended analog guidance (University of Missouri)");
    comboProduct.setIndexByValue(Utility::readPref(pref, products().front().name));
    comboPeriod.connect([this] { reload(); });
    comboProduct.connect([this] { Utility::writePref(pref, comboProduct.getValue()); reload(); });
    textValid.setGray();
    row.addWidget(comboPeriod);
    row.addWidget(comboProduct);
    box.addLayout(row);
    box.addWidget(textValid);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 800);
    reload();
}

void AbpgExtended::reload() {
    const auto generationNow = ++generation;
    const auto period = static_cast<size_t>(std::clamp(comboPeriod.getIndex(), 0, static_cast<int>(hours.size()) - 1));
    const auto& product = products()[static_cast<size_t>(std::clamp(comboProduct.getIndex(), 0, static_cast<int>(products().size()) - 1))];
    const auto hour = hours[period];
    textValid.setText(string{"reading the site..."});
    struct Found {
        string url;
        string valid;
    };
    auto found = std::make_shared<Found>();
    const auto code = product.code;
    new FutureVoid{this, [found, code, hour] {
        // the page names the newest run; the picture's address follows from it (the same file for every region)
        const auto html = QString::fromStdString(UtilityIO::getHtml(site + "ANALOG/EXT_threats.php?reg=SP"));
        const auto hourText = QString{"%1"}.arg(hour, 3, 10, QChar{'0'});
        const auto match = QRegularExpression{QString{"WEB/EXT/(\\d{10})/F%1/%2_(\\w+)F%1\\.png"}.arg(hourText, QString::fromStdString(code))}.match(html);
        if (!match.hasMatch()) {
            return;
        }
        found->url = site + match.captured(0).toStdString();
        const auto run = QDateTime::fromString(match.captured(1), "yyyyMMddHH");
        const QDateTime runUtc{run.date(), run.time(), QTimeZone::utc()};
        // the pictures say "valid from ... to ...": the forecast hour is the middle of a 72 h period
        const auto from = runUtc.addSecs(3600LL * (hour - 36));
        const auto to = runUtc.addSecs(3600LL * (hour + 36));
        found->valid = "run " + runUtc.toString("ddd yyyy-MM-dd HH").toStdString() + "Z, valid " + from.toString("ddd MM-dd HH").toStdString() + "Z - " + to.toString("ddd MM-dd HH").toStdString() + "Z";
        if (to < QDateTime::currentDateTimeUtc()) {
            found->valid += "  - EXPIRED";
        }
    }, [this, found, generationNow] {
        if (closed || generationNow != generation) {
            return;
        }
        textValid.setText(found->url.empty() ? string{"nothing published for this period and product"} : found->valid);
        if (found->url.empty()) {
            return;
        }
        new FutureBytes{this, found->url, [this, generationNow] (const QByteArray& bytes) {
            if (!closed && generationNow == generation && !bytes.isEmpty()) {
                image.setBytes(bytes);
            }
        }};
    }};
}
