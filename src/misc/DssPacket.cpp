// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/DssPacket.h"
#include <map>
#include <memory>
#include <QDesktopServices>
#include <QLocale>
#include <QXmlStreamReader>
#include <QUrl>
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"

namespace {
    constexpr long long placeholderSize = 39396;   // weather.gov answers an office's unused DssPacket.pdf with this same small file

    string mediaUrl(const string& wfo, const string& name) {
        return "https://www.weather.gov/media/" + WString::toLower(wfo) + "/" + name;
    }

    QDateTime stored(const string& key) {
        const auto text = Utility::readPref(key, "");
        return text.empty() ? QDateTime{} : QDateTime::fromString(QString::fromStdString(text), Qt::ISODate);
    }

    void store(const string& key, const QDateTime& value) {
        Utility::writePref(key, value.isValid() ? value.toUTC().toString(Qt::ISODate).toStdString() : "");
    }

    QString when(const QDateTime& t, bool withAgo = true) {
        const auto local = t.toLocalTime();
        if (!withAgo) {
            return QLocale{}.toString(local, "ddd d MMM h:mm AP t");
        }
        const auto hours = t.secsTo(QDateTime::currentDateTimeUtc()) / 3600.0;
        QString ago = hours < 1.0 ? "less than an hour ago" : hours < 48.0 ? QString::number(static_cast<int>(hours)) + " h ago" : QString::number(static_cast<int>(hours / 24.0)) + " days ago";
        return QLocale{}.toString(local, "ddd d MMM h:mm AP t") + " (" + ago + ")";
    }
}

std::vector<string> DssPacket::candidates(const string& wfo) {
    static const std::map<string, string> known{{"sgf", "DssPacket.pdf"}, {"lsx", "DssPacket.pdf"}, {"eax", "DssPacket.pdf"}, {"okx", "DssPacket.pdf"}, {"phi", "current_briefing.pdf"}};
    std::vector<string> out;
    const auto it = known.find(WString::toLower(wfo));
    if (it != known.end()) {
        out.push_back(mediaUrl(wfo, it->second));
    }
    for (const string name : {"DssPacket.pdf", "current_briefing.pdf"}) {
        const auto url = mediaUrl(wfo, name);
        if (std::find(out.begin(), out.end(), url) == out.end()) {
            out.push_back(url);
        }
    }
    return out;
}

void DssPacket::readFeed(const string& wfo, Result& r) {
    const auto text = URL::getText("https://www.weather.gov/source/" + WString::toLower(wfo) + "/dsspacket.xml");
    QXmlStreamReader xml{QString::fromStdString(text)};
    long long start = 0, end = 0, update = 0;
    int order = 0;
    QString element, description;
    bool first = false, got = false;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            element = xml.name().toString();
            if (element == "graphicast") {
                order = 0; first = false;
            }
        } else if (token == QXmlStreamReader::Characters) {
            const auto value = xml.text().toString().trimmed();
            if (value.isEmpty()) {
                continue;
            }
            if (element == "order") {
                order = value.toInt(); first = order == 1;
            } else if (element == "StartTime" && !got) {
                start = value.toLongLong();
            } else if (element == "EndTime" && !got) {
                end = value.toLongLong();
            } else if (element == "Update" && !got) {
                update = value.toLongLong();
            } else if (element == "description" && !got) {
                description = value;
            }
        } else if (token == QXmlStreamReader::EndElement && xml.name() == QString{"graphicast"}) {
            if (first || (start > 0 && !got && order == 0)) {
                got = true;
            }
        }
    }
    if (!got || start <= 0) {
        return;                                                // no status file (the server answers an office without one with a 404 page)
    }
    r.start = QDateTime::fromSecsSinceEpoch(start, Qt::UTC);
    r.end = QDateTime::fromSecsSinceEpoch(end, Qt::UTC);
    r.feedUpdated = update > 0 ? QDateTime::fromSecsSinceEpoch(update, Qt::UTC) : QDateTime{};
    r.description = description.simplified().toStdString();
    const auto now = QDateTime::currentDateTimeUtc();
    r.feed = end > start && r.start <= now && now < r.end ? Result::Feed::Active : Result::Feed::Expired;   // the office's own page: expired when the end has passed, or the end is the start
}

DssPacket::Result DssPacket::check(const string& wfo) {
    Result r;
    readFeed(wfo, r);
    const auto key = WString::toUpper(wfo);
    const auto savedUrl = Utility::readPref("DSS_URL_" + key, "");
    r.previousUploaded = stored("DSS_LM_" + key);
    r.previousChecked = stored("DSS_CHK_" + key);
    const auto now = QDateTime::currentDateTimeUtc();
    if (!savedUrl.empty()) {                                   // the address is known: one conditional request says whether there is a newer file
        const auto h = URL::head(savedUrl, r.previousUploaded);
        if (h.status == 304) {
            r.found = true; r.url = savedUrl; r.uploaded = r.previousUploaded;
            store("DSS_CHK_" + key, now);
            return r;
        }
        if (h.status >= 200 && h.status < 300 && h.size != placeholderSize) {
            r.found = true; r.url = savedUrl; r.uploaded = h.lastModified;
            r.changed = r.previousUploaded.isValid() ? (!h.lastModified.isValid() || h.lastModified > r.previousUploaded) : true;
            store("DSS_LM_" + key, h.lastModified);
            store("DSS_CHK_" + key, now);
            return r;
        }
    }
    for (const auto& url : candidates(wfo)) {                  // a first look, or the saved address stopped working
        const auto h = URL::head(url);
        if (h.status == 0) {
            r.problem = "weather.gov did not answer";
            return r;
        }
        if (h.status >= 200 && h.status < 300 && h.size != placeholderSize) {
            r.found = true; r.url = url; r.uploaded = h.lastModified;
            r.changed = !r.previousUploaded.isValid() || !h.lastModified.isValid() || h.lastModified > r.previousUploaded || url != savedUrl;
            Utility::writePref("DSS_URL_" + key, url);
            store("DSS_LM_" + key, h.lastModified);
            store("DSS_CHK_" + key, now);
            return r;
        }
    }
    r.problem = "no packet found at the names tried";
    return r;
}

DssPacket::DssPacket(Window * parent)
    : Window{parent}
    , wfo{Location::wfo()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Local office support packet - " + wfo + " - " + Location::name());
    status = new QLabel{"Looking at " + QString::fromStdString(wfo) + "'s packet...", this};
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    note = new QLabel{this};
    note->setWordWrap(true);
    UiStandards::muted(note);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    open = UiStandards::button(this, "Open packet", "Opens the office's packet PDF in the browser");
    again = UiStandards::button(this, "Check again", "Asks weather.gov whether there is a newer file");
    open->setEnabled(false);
    box.addWidgetReal(open, 0, Qt::Alignment{});
    box.addWidgetReal(again, 0, Qt::Alignment{});
    box.addStretch();
    box.getAndShow(this);
    QObject::connect(open, &QPushButton::clicked, this, [this] { if (!url.empty()) { QDesktopServices::openUrl(QUrl{QString::fromStdString(url)}); } });
    QObject::connect(again, &QPushButton::clicked, this, [this] { look(); });
    look();
}

void DssPacket::look() {
    again->setEnabled(false);
    status->setText("Looking at " + QString::fromStdString(wfo) + "'s packet...");
    auto result = std::make_shared<Result>();
    const auto office = wfo;
    new FutureVoid{this, [result, office] { *result = check(office); }, [this, result, office] {
        again->setEnabled(true);
        QString head = QString::fromStdString(office) + ": ";
        QString feedLine;
        if (result->feed == Result::Feed::Active) {
            feedLine = "<b>In effect</b> until " + when(result->end, false) + ".";
        } else if (result->feed == Result::Feed::Expired) {
            feedLine = "Expired (" + (result->end > result->start ? "ended " + when(result->end, false) : "the office has no packet in effect") + ").";
        } else {
            feedLine = "The office publishes no status file.";
        }
        if (!result->description.empty()) {
            feedLine += "  \"" + QString::fromStdString(result->description).toHtmlEscaped() + "\"";
        }
        if (!result->found) {
            status->setTextFormat(Qt::RichText);
            status->setText(head + feedLine + "<br>No packet file found (" + QString::fromStdString(result->problem) + ").");
            note->setText("Offices name their packets themselves, and not every one publishes one. The office's own page may have it: weather.gov/" + QString::fromStdString(WString::toLower(office)) + "/");
            url = "https://www.weather.gov/" + WString::toLower(office) + "/";
            open->setText("Office page");
            open->setEnabled(true);
            return;
        }
        url = result->url;
        open->setText("Open packet");
        open->setEnabled(true);
        QString updated = result->uploaded.isValid() ? "Packet last updated " + when(result->uploaded) + "." : "Packet last updated: the server gives no time.";
        QString second;
        if (!result->previousChecked.isValid()) {
            second = "First look: later looks will say whether there is a new file since.";
        } else if (result->changed) {
            second = "New file since your look " + when(result->previousChecked) + ".";
        } else {
            second = "No new file since your check " + when(result->previousChecked) + ".";
        }
        status->setTextFormat(Qt::RichText);
        status->setText(head + feedLine + "<br>" + updated + "<br>" + second);
        note->setText(QString::fromStdString(result->url));
    }};
}
