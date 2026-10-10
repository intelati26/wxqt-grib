// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spc/SvrComparison.h"
#include <algorithm>
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "util/UtilityIO.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/Utility.h"

namespace {
    const string csuBase{"https://schumacher.atmos.colostate.edu/hilla/csu_mlp/latest/"};
    const string abpgSite{"https://analog.missouri.edu/"};
    // ABPG region codes, in the order of the region combo
    const vector<string> regionCodes{"SP", "GP", "MV", "SE", "EC", "RM", "SW", "WC"};
    const vector<string> regionNames{"Southern Plains", "Great Plains", "Mississippi Valley", "Southeast", "East Coast",
                                     "Rocky Mountains", "Southwest", "West Coast"};
    const string regionPref{"ABPG_REGION"};
    const vector<string> abpgProducts{"PRALLC01", "PRALLC05"};   // percent of analogs with 1+ / 5+ severe reports

    string stamp(const QDateTime& utc) {
        return utc.toString("ddd yyyy-MM-dd HH").toStdString() + "Z";
    }

    // "valid 12Z Thu 10/01 - 12Z Fri 10/02", with a warning once the period is over (a stale picture must not look current)
    string validText(const QDateTime& from, const QDateTime& to, bool derived) {
        string text = "valid " + stamp(from) + " - " + stamp(to);
        if (derived) {
            text += " (from file time)";
        }
        if (to < QDateTime::currentDateTimeUtc()) {
            text += "  - EXPIRED";
        }
        return text;
    }

    QDateTime twelveZ(const QDate& date) {
        return QDateTime{date, QTime{12, 0}, QTimeZone::utc()};
    }

    // SPC's "Valid 011300Z - 021200Z": day-of-month, hour, minute; the month is the one that puts it nearest to now
    QDateTime fromSpcStamp(int day, int hour, int minute) {
        const auto now = QDateTime::currentDateTimeUtc();
        QDateTime best;
        for (int offset : {-1, 0, 1}) {
            const auto month = now.date().addMonths(offset);
            const QDate date{month.year(), month.month(), 1};
            if (day > date.daysInMonth()) {
                continue;
            }
            const QDateTime candidate{QDate{month.year(), month.month(), day}, QTime{hour, minute}, QTimeZone::utc()};
            if (!best.isValid() || std::abs(candidate.secsTo(now)) < std::abs(best.secsTo(now))) {
                best = candidate;
            }
        }
        return best;
    }

    vector<string> dayLabels() {
        vector<string> days;
        for (int day = 1; day <= 8; day += 1) {
            days.push_back("Day " + To::string(day));
        }
        return days;
    }
}

SvrComparison::SvrComparison(Window * parent)
    : Window{parent}
    , comboDay{this, dayLabels()}
    , comboHazard{this, {"ABPG: 1+ severe reports", "ABPG: 5+ severe reports"}}
    , comboRegion{this, regionNames}
    , textNote{this, ""}
{
    setTitle("Severe weather outlook comparison");
    comboDay.connect([this] { reload(); });
    comboHazard.connect([this] { reload(); });
    comboHazard.getView()->setToolTip("University of Missouri ABPG: the percentage of its top 15 analog days that had this many severe reports within 110 km of a point. SPC and CSU-MLP pictures are not affected.");
    comboRegion.setIndexByValue(Utility::readPref(regionPref, regionNames.front()));
    comboRegion.connect([this] { Utility::writePref(regionPref, comboRegion.getValue()); reload(); });
    rowTop.addWidget(comboDay);
    rowTop.addWidget(comboHazard);
    rowTop.addWidget(comboRegion);
    rowTop.addWidget(textNote);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addLayout(rowTitles);
    box.addLayout(rowValid);
    box.addLayout(rowImages);
    box.addStretch();
    box.getAndShow(this);
    reload();
}

vector<SvrComparison::Panel> SvrComparison::panels(int day, int hazard) const {
    vector<Panel> out;
    // SPC: days 1-3 the categorical outlook, days 4-8 the probability graphic for that day
    out.push_back({"SPC convective outlook", ""});
    out.push_back({"CSU-MLP (Colorado State)", day <= 2 ? csuBase + "day" + To::string(day) + "_3panel_latest.png"
                                                         : csuBase + "day" + To::string(day) + "_nospc_latest.png"});
    // ABPG: the picture's address comes from its page (it names the newest run), so only the title is known here
    const auto region = static_cast<size_t>(std::clamp(comboRegion.getIndex(), 0, static_cast<int>(regionNames.size()) - 1));
    string cipsTitle = "ABPG analogs (Univ. of Missouri) - " + regionNames[region] + (hazard == 1 ? ", 5+ reports" : ", 1+ reports");
    const string cips = day <= 6 ? "abpg" : string{};
    out.push_back({cipsTitle, cips});
    return out;
}

void SvrComparison::reload() {
    const auto day = comboDay.getIndex() + 1;
    const auto hazard = comboHazard.getIndex();
    comboHazard.getView()->setEnabled(day <= 6);
    comboRegion.getView()->setEnabled(day <= 6);
    const auto generationNow = ++generation;
    rowTitles.removeChildren();
    rowValid.removeChildren();
    rowImages.removeChildren();
    titles.clear();
    valids.clear();
    images.clear();
    auto list = panels(day, hazard);
    textNote.setText(day >= 7 ? string{"No ABPG guidance this far out"} : string{});
    for (size_t index = 0; index < list.size(); index += 1) {
        titles.emplace_back(this, list[index].title);
        titles.back().setBold();
        rowTitles.addWidget(titles.back());
        valids.emplace_back(this, "");
        valids.back().setGray();
        rowValid.addWidget(valids.back());
        images.emplace_back(this);
        images.back().setNumberAcross(static_cast<int>(list.size()), getWindowWidth());
        images.back().connect([this, index] {
            if (index < images.size() && !images[index].bytes.isEmpty()) {
                new ImageViewer{this, images[index].bytes};
            }
        });
        rowImages.addWidget(images.back());
    }
    resizeEventCustom();
    for (size_t index = 0; index < list.size(); index += 1) {
        if (index == 0) {
            // the SPC graphic's address needs a page read (it carries the issue time): off the UI thread
            struct Found {
                string url;
                string valid;
            };
            auto found = std::make_shared<Found>();
            new FutureVoid{this,
                [day, found] {
                    const auto urls = UtilitySpcSwo::getImageUrls(day <= 3 ? day : 48);
                    const auto position = day <= 3 ? 0 : day - 4;
                    if (position >= 0 && position < static_cast<int>(urls.size())) {
                        found->url = urls[static_cast<size_t>(position)];
                    }
                    if (day <= 3) {   // the outlook page states the period: "Valid 011300Z - 021200Z"
                        const auto html = QString::fromStdString(UtilityIO::getHtml("https://www.spc.noaa.gov/products/outlook/day" + To::string(day) + "otlk.html"));
                        const auto match = QRegularExpression{"Valid (\\d{2})(\\d{2})(\\d{2})Z - (\\d{2})(\\d{2})(\\d{2})Z"}.match(html);
                        if (match.hasMatch()) {
                            found->valid = validText(fromSpcStamp(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt()),
                                                     fromSpcStamp(match.captured(4).toInt(), match.captured(5).toInt(), match.captured(6).toInt()), false);
                        }
                    }
                },
                [this, found, day, generationNow] {
                    if (generationNow != generation) {
                        return;
                    }
                    if (!found->valid.empty()) {
                        setValid(0, found->valid);
                    }
                    if (found->url.empty()) {
                        return;
                    }
                    new FutureBytes{this, found->url, [this, day, generationNow] (const auto& ba) {
                        if (generationNow != generation || images.empty()) {
                            return;
                        }
                        images[0].setBytes(ba);
                        URL::Meta meta;
                        if (day >= 4 && URL::metaFor(ba, meta) && meta.lastModified.isValid()) {   // days 4-8: no stated period
                            const auto base = meta.lastModified.toUTC().addSecs(-3 * 3600).date();
                            setValid(0, validText(twelveZ(base.addDays(day - 1)), twelveZ(base.addDays(day)), true));
                        }
                    }};
                }};
        } else if (index == 2) {
            if (list[2].url.empty()) {
                continue;
            }
            // the ABPG page names the newest run and the pictures' exact addresses
            struct Found {
                string url;
                string valid;
            };
            auto found = std::make_shared<Found>();
            const auto code = regionCodes[static_cast<size_t>(std::clamp(comboRegion.getIndex(), 0, static_cast<int>(regionCodes.size()) - 1))];
            const auto product = abpgProducts[static_cast<size_t>(std::clamp(hazard, 0, 1))];
            new FutureVoid{this,
                [day, code, product, found] {
                    const auto html = QString::fromStdString(UtilityIO::getHtml(abpgSite + "ANALOG/threats.php?reg=" + code));
                    const auto hours = QString{"%1"}.arg(24 * day, 3, 10, QChar{'0'});
                    const auto match = QRegularExpression{QString{"WEB/SHORT/(\\d{10})/F%1/%2_%1/%3_(\\w+)F%1\\.png"}.arg(hours, QString::fromStdString(code), QString::fromStdString(product))}.match(html);
                    if (!match.hasMatch()) {
                        return;
                    }
                    found->url = abpgSite + match.captured(0).toStdString();
                    const auto run = QDateTime::fromString(match.captured(1), "yyyyMMddHH");
                    const QDateTime runUtc{run.date(), run.time(), QTimeZone::utc()};
                    // "Valid at <run + forecast hour>": the pictures say so themselves
                    found->valid = "run " + stamp(runUtc) + ", valid at " + stamp(runUtc.addSecs(24LL * day * 3600));
                    if (runUtc.addSecs(24LL * day * 3600) < QDateTime::currentDateTimeUtc()) {
                        found->valid += "  - EXPIRED";
                    }
                },
                [this, found, generationNow] {
                    if (generationNow != generation) {
                        return;
                    }
                    setValid(2, found->url.empty() ? string{"nothing published for this region and day"} : found->valid);
                    if (found->url.empty()) {
                        return;
                    }
                    new FutureBytes{this, found->url, [this, generationNow] (const auto& ba) {
                        if (generationNow == generation && images.size() > 2) {
                            images[2].setBytes(ba);
                        }
                    }};
                }};
        } else if (!list[index].url.empty()) {
            new FutureBytes{this, list[index].url, [this, index, day, generationNow] (const auto& ba) {
                if (generationNow != generation || index >= images.size()) {
                    return;
                }
                images[index].setBytes(ba);
                URL::Meta meta;
                if (!URL::metaFor(ba, meta) || !meta.lastModified.isValid()) {
                    return;
                }
                // CSU-MLP day N: the convective day 12Z - 12Z, N - 1 days after the run's date (posted a few hours after 00Z)
                const auto base = meta.lastModified.toUTC().addSecs(-3 * 3600).date();
                setValid(1, validText(twelveZ(base.addDays(day - 1)), twelveZ(base.addDays(day)), true));
            }};
        }
    }
}

void SvrComparison::setValid(size_t panel, const string& text) {
    if (panel < valids.size()) {
        valids[panel].setText(text);
    }
}

void SvrComparison::resizeEventCustom() {
    if (images.empty()) {
        return;
    }
    const auto each = width() / static_cast<int>(images.size()) - 10;
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(each));
    }
    // the title and valid-time text sit exactly over their picture
    for (auto& text : titles) {
        text.getView()->setFixedWidth(each);
        text.getView()->setWordWrap(true);
    }
    for (auto& text : valids) {
        text.getView()->setFixedWidth(each);
        text.getView()->setWordWrap(true);
    }
}
