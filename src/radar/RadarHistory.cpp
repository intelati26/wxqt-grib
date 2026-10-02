// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/RadarHistory.h"
#include <memory>
#include "objects/FutureVoid.h"

namespace {
    const char * const editFormat{"yyyy-MM-dd HH:mm:ss 'UTC'"};

    QString describe(const QDateTime& utc) {
        return utc.toString("yyyy-MM-dd HH:mm:ss") + " UTC  (" + utc.toLocalTime().toString("ddd d MMM HH:mm:ss t") + " local)";
    }
}

RadarHistory::RadarHistory(Window * parent,
                           const function<void(const QDateTime&)>& apply,
                           const function<vector<QDateTime>(const QDateTime&, const QDateTime&)>& scans,
                           const function<QDateTime()>& shown)
    : Window{parent}
    , textNote{this, "Pick a time (UTC): the radar shows the scan at or before it. Archive: Unidata's S3 bucket of NEXRAD Level III files, early 2022 onward."}
    , textStatus{this, ""}
    , edit{new QDateTimeEdit{this}}
    , buttonShow{this, None, "Show this time"}
    , buttonDayBack{this, None, "- 1 day"}
    , buttonDayForward{this, None, "+ 1 day"}
    , buttonPrevious{this, None, "Previous scan"}
    , buttonNext{this, None, "Next scan"}
    , buttonLive{this, None, "Back to live"}
    , apply{apply}
    , scans{scans}
    , shown{shown}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Radar history");
    textNote.setWordWrap(true);
    edit->setDisplayFormat(editFormat);
    edit->setTimeSpec(Qt::UTC);
    edit->setCalendarPopup(true);
    edit->setDateTimeRange(QDateTime{QDate{2022, 2, 18}, QTime{0, 0}, Qt::UTC}, QDateTime::currentDateTimeUtc());
    const auto current = shown();
    setEdit(current.isValid() ? current : QDateTime::currentDateTimeUtc().addSecs(-3600));
    buttonShow.connect([this] { showTime(); });
    buttonDayBack.connect([this] { setEdit(editTime().addDays(-1)); showTime(); });
    buttonDayForward.connect([this] { setEdit(editTime().addDays(1)); showTime(); });
    buttonPrevious.connect([this] { step(-1); });
    buttonNext.connect([this] { step(1); });
    buttonLive.connect([this] { live(); });
    rowTime.addWidgetReal(edit);
    rowTime.addWidget(buttonShow);
    rowTime.addWidget(buttonDayBack);
    rowTime.addWidget(buttonDayForward);
    rowTime.addStretch();
    rowButtons.addWidget(buttonPrevious);
    rowButtons.addWidget(buttonNext);
    rowButtons.addWidget(buttonLive);
    rowButtons.addStretch();
    box.addWidget(textNote);
    box.addLayout(rowTime);
    box.addLayout(rowButtons);
    box.addWidget(textStatus);
    box.addStretch();
    box.getAndShow(this);
    resize(640, 190);
    refreshStatus();
}

QDateTime RadarHistory::editTime() const {
    auto time = edit->dateTime();
    time.setTimeSpec(Qt::UTC);
    return time;
}

void RadarHistory::setEdit(const QDateTime& utc) {
    edit->setDateTime(utc.toUTC());
}

void RadarHistory::refreshStatus() {
    const auto current = shown();
    textStatus.setText(current.isValid() ? QString{"Showing the scan at or before "} + describe(current) : QString{"Showing the newest scan (live)."});
}

void RadarHistory::showTime() {
    apply(editTime());
    refreshStatus();
}

void RadarHistory::live() {
    apply(QDateTime{});
    refreshStatus();
}

// previous / next scan of the radar and product on screen, looked up in the archive off the UI thread
void RadarHistory::step(int direction) {
    const auto base = shown().isValid() ? shown() : editTime();
    textStatus.setText(QString{"Looking up scans..."});
    auto found = std::make_shared<QDateTime>();
    new FutureVoid{this,
        [this, found, base, direction] {
            const auto now = QDateTime::currentDateTimeUtc();
            if (direction < 0) {
                // the scans up to the time shown: the last one is what is on screen, the one before it is the previous scan
                const auto times = scans(base.addSecs(-3 * 3600), base);
                if (times.size() >= 2) {
                    *found = times[times.size() - 2];
                } else if (times.size() == 1) {
                    const auto earlier = scans(times.front().addSecs(-3 * 3600), times.front().addSecs(-1));
                    if (!earlier.empty()) {
                        *found = earlier.back();
                    }
                }
            } else {
                const auto times = scans(base, std::min(base.addSecs(3 * 3600), now));
                for (const auto& time : times) {
                    if (time > base) {
                        *found = time;
                        break;
                    }
                }
            }
        },
        [this, found] {
            if (closed) {
                return;
            }
            if (!found->isValid()) {
                textStatus.setText(QString{"No scan found in that direction."});
                return;
            }
            setEdit(*found);
            apply(*found);
            refreshStatus();
        }};
}
