// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/WeatherStory.h"
#include <algorithm>
#include <memory>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QLocale>
#include <QPixmap>
#include <QUrl>
#include <QXmlStreamReader>
#include "misc/DssPacket.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "ui/UiStandards.h"

vector<WeatherStory::Story> WeatherStory::active(const string& wfo) {
    vector<Story> out;
    const auto lower = WString::toLower(wfo);
    const auto now = QDateTime::currentDateTimeUtc();
    auto text = URL::getText("https://www.weather.gov/source/" + lower + "/WxStory/WeatherStory.xml");   // Eastern and Western offices
    if (text.find("<graphicast>") == string::npos) {
        text = URL::getText("https://www.weather.gov/source/" + lower + "/wxstory/wxstory.xml");          // Central Region offices keep theirs under another name
    }
    QXmlStreamReader xml{QString::fromStdString(text)};
    Story one;
    long long start = 0, end = 0;
    int radar = 0;
    QString element, full, small;
    bool listed = false;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            element = xml.name().toString();
            if (element == "graphicast") {
                one = Story{}; start = end = 0; radar = 0; full.clear(); small.clear();
            }
        } else if (token == QXmlStreamReader::Characters) {
            const auto value = xml.text().toString().trimmed();
            if (value.isEmpty()) {
                continue;
            }
            if (element == "StartTime") { start = value.toLongLong(); }
            else if (element == "EndTime") { end = value.toLongLong(); }
            else if (element == "radar") { radar = value.toInt(); }
            else if (element == "title") { one.title = value.simplified().toStdString(); }
            else if (element == "description") { one.description = value.simplified().toStdString(); }
            else if (element == "FullImage") { full = value; }
            else if (element == "SmallImage") { small = value; }
        } else if (token == QXmlStreamReader::EndElement && xml.name() == QString{"graphicast"}) {
            listed = true;
            one.start = QDateTime::fromSecsSinceEpoch(start, Qt::UTC);
            one.end = QDateTime::fromSecsSinceEpoch(end, Qt::UTC);
            one.imageUrl = (!full.isEmpty() ? full : small).toStdString();
            if (radar == 0 && !one.imageUrl.empty() && start > 0 && end > start && one.start <= now && now < one.end) {   // the office's own front page shows the ones in effect that are not radar loops
                out.push_back(one);
            }
        }
    }
    if (listed) {
        for (auto& s : out) {
            s.updated = URL::head(s.imageUrl).lastModified;
        }
    } else {
        for (int n = 1; n <= 6; n++) {                          // no list: the fixed picture files; one counts when it was uploaded within two days
            const auto url = "https://www.weather.gov/images/" + lower + "/wxstory/Tab" + std::to_string(n) + "FileL.png";
            const auto h = URL::head(url);
            if (h.status >= 200 && h.status < 300 && h.lastModified.isValid() && h.lastModified.secsTo(now) < 2 * 86400) {
                Story s;
                s.title = "Weather story " + std::to_string(n);
                s.imageUrl = url;
                s.updated = h.lastModified;
                out.push_back(s);
            }
        }
    }
    std::sort(out.begin(), out.end(), [] (const Story& a, const Story& b) {   // the newest first: the upload time when the server gives one, else the start of the window
        const auto ta = a.updated.isValid() ? a.updated : a.start, tb = b.updated.isValid() ? b.updated : b.start;
        return ta > tb;
    });
    return out;
}

WeatherStoryViewer::WeatherStoryViewer(Window * parent)
    : Window{parent}
    , wfo{Location::wfo()}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Weather story - " + wfo + " - " + Location::name());
    strip = new QWidget{this};
    new QHBoxLayout{strip};
    strip->layout()->setContentsMargins(0, 0, 0, 0);
    static_cast<QHBoxLayout *>(strip->layout())->addStretch();
    box.addWidgetReal(strip, 0, Qt::Alignment{});
    text = new QLabel{"Reading " + QString::fromStdString(wfo) + "'s weather story...", this};
    text->setWordWrap(true);
    box.addWidgetReal(text, 0, Qt::Alignment{});
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    note = new QLabel{this};
    note->setWordWrap(true);
    UiStandards::muted(note);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    auto * page = UiStandards::button(this, "Office page", "Opens the office's weather story page on weather.gov");
    auto * packet = UiStandards::button(this, "Packet...", "The office's decision support packet: whether one is in effect and when it was last updated");
    box.addWidgetReal(page, 0, Qt::Alignment{});
    box.addWidgetReal(packet, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 860);
    QObject::connect(page, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl{QString::fromStdString("https://www.weather.gov/" + WString::toLower(wfo) + "/weatherstory")}); });
    QObject::connect(packet, &QPushButton::clicked, this, [this] { new DssPacket{this}; });
    auto got = std::make_shared<vector<WeatherStory::Story>>();
    const auto office = wfo;
    new FutureVoid{this, [got, office] { *got = WeatherStory::active(office); }, [this, got] {
        if (closed) {
            return;
        }
        stories = *got;
        if (stories.empty()) {
            text->setText(QString::fromStdString(wfo) + " has no weather story in effect (or does not publish one this way).");
            return;
        }
        pictures.assign(stories.size(), QByteArray{});
        auto * layout = static_cast<QHBoxLayout *>(strip->layout());
        for (size_t i = 0; i < stories.size(); i++) {         // a preview button for each, the picture arriving into it
            auto * button = UiStandards::button(strip, "", QString::fromStdString(stories[i].title));
            button->setCheckable(true);
            button->setAutoExclusive(true);
            button->setIconSize(QSize{176, 120});
            button->setFixedSize(190, 134);
            layout->insertWidget(static_cast<int>(i), button);
            previews.push_back(button);
            QObject::connect(button, &QPushButton::clicked, this, [this, i] { choose(i); });
            new FutureBytes{this, stories[i].imageUrl, [this, i] (const QByteArray& bytes) {
                if (closed || bytes.isEmpty()) {
                    return;
                }
                pictures[i] = bytes;
                QPixmap pixmap;
                pixmap.loadFromData(bytes);
                previews[i]->setIcon(QIcon{pixmap.scaled(176, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation)});
                if (previews[i]->isChecked()) {
                    image.setBytes(bytes);
                }
            }};
        }
        choose(0);
    }};
}

void WeatherStoryViewer::choose(size_t index) {
    if (index >= stories.size()) {
        return;
    }
    const auto& s = stories[index];
    previews[index]->setChecked(true);
    text->setText(QString::fromStdString(s.description.empty() ? s.title : s.title + ": " + s.description));
    QString line;
    if (s.updated.isValid()) {
        line = "Picture last updated " + QLocale{}.toString(s.updated.toLocalTime(), "ddd d MMM h:mm AP t") + ".  ";
    }
    if (s.end.isValid()) {
        line += "In effect until " + QLocale{}.toString(s.end.toLocalTime(), "ddd d MMM h:mm AP t") + ".";
    }
    note->setText(line);
    if (!pictures[index].isEmpty()) {
        image.setBytes(pictures[index]);                        // else it is set when it arrives
    }
}
