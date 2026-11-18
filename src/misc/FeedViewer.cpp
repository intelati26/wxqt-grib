// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/FeedViewer.h"
#include <cstdlib>
#include <memory>
#include <mutex>
#include <QDate>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>
#include <QXmlStreamReader>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"
#include "util/PermanentCache.h"
#include "util/UtilityIO.h"
#include "util/UtilityTextReflow.h"

namespace {
    std::mutex keep;
    std::string keptUrl;
    std::vector<FeedViewer::Item> kept;
    std::string newestTitle;

    QString dateOf(const QString& pubDate, const QString& link) {
        auto parsed = QDateTime::fromString(pubDate.left(25).trimmed(), Qt::RFC2822Date);
        if (parsed.isValid()) {
            return parsed.date().toString("ddd d MMM yyyy");
        }
        const auto day = QRegularExpression{"day=([0-9]{4})-([0-9]{2})-([0-9]{2})"}.match(link);   // (the Iowa Environmental Mesonet's items have no date of their own: it is in the link)
        if (day.hasMatch()) {
            const QDate d{day.captured(1).toInt(), day.captured(2).toInt(), day.captured(3).toInt()};
            return d.isValid() ? d.toString("ddd d MMM yyyy") : QString{};
        }
        return {};
    }
}

const FeedViewer::Source& FeedViewer::iem() {
    static const Source source{"iem-feature", "Iowa Environmental Mesonet: the daily feature", "https://mesonet.agron.iastate.edu/feature_rss.php",
                               "The Iowa Environmental Mesonet's daily feature (Iowa State University; Daryl Herzmann): a chart of the day and a note about it, shown as they publish it. "
                               "\"The chart on the IEM site\" makes the same chart with their tools.",
                               "https://mesonet.agron.iastate.edu/onsite/features/", "https://mesonet.agron.iastate.edu/"};
    return source;
}

vector<FeedViewer::Item> FeedViewer::parse(const QByteArray& xml) {
    vector<Item> out;
    QXmlStreamReader reader{xml};
    Item current;
    QString pubDate, description;
    bool inItem = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const auto name = reader.name();
            if (name == "item") {
                inItem = true;
                current = Item{};
                pubDate.clear();
                description.clear();
            } else if (inItem && name == "title") {
                current.title = reader.readElementText().trimmed();
            } else if (inItem && name == "link") {
                current.link = reader.readElementText().trimmed();
            } else if (inItem && name == "pubDate") {
                pubDate = reader.readElementText().trimmed();
            } else if (inItem && name == "description") {
                description = reader.readElementText();
            }
        } else if (reader.isEndElement() && reader.name() == "item") {
            inItem = false;
            const auto image = QRegularExpression{R"re(<img[^>]*\ssrc="([^"]+)")re", QRegularExpression::CaseInsensitiveOption}.match(description);
            if (image.hasMatch()) {
                current.imageUrl = image.captured(1);
            }
            auto anchors = QRegularExpression{R"re(<a[^>]*\shref="([^"]+)"[^>]*>([^<]*)</a>)re", QRegularExpression::CaseInsensitiveOption}.globalMatch(description);
            if (anchors.hasNext()) {
                const auto a = anchors.next();
                current.extraLink = a.captured(1);
                current.extraLabel = a.captured(2).trimmed();
            }
            QTextDocument doc;
            doc.setHtml(description);
            QString plain = doc.toPlainText();
            plain.remove(QChar{0xFFFC});   // (the picture's place in the text)
            plain = plain.trimmed();
            if (!current.extraLabel.isEmpty() && plain.endsWith(current.extraLabel)) {   // (the link's words are a button of ours)
                plain = plain.left(plain.size() - current.extraLabel.size()).trimmed();
            }
            current.text = plain;
            current.date = dateOf(pubDate, current.link);
            if (!current.title.isEmpty()) {
                out.push_back(current);
            }
        }
    }
    return out;
}

bool FeedViewer::dueToCheck(long long lastCheck, long long lastAttempt, const QDateTime& now) {
    const long long at = now.toSecsSinceEpoch();
    if (lastCheck <= 0) {   // never read: once, whenever it is asked
        return lastAttempt <= 0 || at - lastAttempt >= 1800;
    }
    if (lastAttempt > lastCheck && at - lastAttempt < 1800) {   // the last try failed a moment ago
        return false;
    }
    const auto local = now.toLocalTime();
    if (local.date().dayOfWeek() > 5 || local.time() < QTime{9, 0}) {   // a weekend, or before 9
        return false;
    }
    return at - lastCheck >= 24 * 3600;
}

vector<FeedViewer::Item> FeedViewer::read(const Source& source) {
    const PermanentCache store{"feeds"};
    const auto number = [&store] (const string& name) { const auto text = store.read(name); return text.empty() ? 0LL : std::atoll(text.c_str()); };
    const long long checked = number(source.id + ".checked"), attempted = number(source.id + ".attempted");
    const auto now = QDateTime::currentDateTime();
    {
        const std::lock_guard lock{keep};
        if (!kept.empty() && keptUrl == source.url && !dueToCheck(checked, attempted, now)) {
            return kept;
        }
    }
    auto items = parse(QByteArray::fromStdString(store.read(source.id + ".xml")));
    if (items.empty() || dueToCheck(checked, attempted, now)) {
        store.write(source.id + ".attempted", std::to_string(now.toSecsSinceEpoch()));
        const auto bytes = UtilityIO::downloadAsByteArray(source.url);
        if (auto fresh = parse(bytes); !fresh.empty()) {
            store.write(source.id + ".xml", bytes.toStdString());
            store.write(source.id + ".checked", std::to_string(now.toSecsSinceEpoch()));
            items = std::move(fresh);
        }   // (else the copy kept, if there is one: an older feed is better than none)
    }
    if (!items.empty()) {
        const std::lock_guard lock{keep};
        kept = items;
        keptUrl = source.url;
        newestTitle = items.front().title.toStdString();
    }
    return items;
}

bool FeedViewer::newest(const Source& source, Item& item, QByteArray& image) {
    const auto items = read(source);
    if (items.empty() || items.front().imageUrl.isEmpty()) {
        return false;
    }
    item = items.front();
    // the newest picture is kept with the feed: the same item is not downloaded again
    const PermanentCache store{"feeds"};
    if (store.read(source.id + ".image.url") == item.imageUrl.toStdString()) {
        image = QByteArray::fromStdString(store.read(source.id + ".image"));
        if (!image.isEmpty()) {
            return true;
        }
    }
    image = UtilityIO::downloadAsByteArray(item.imageUrl.toStdString());
    if (!image.isEmpty()) {
        store.write(source.id + ".image", image.toStdString());
        store.write(source.id + ".image.url", item.imageUrl.toStdString());
    }
    return !image.isEmpty();
}

string FeedViewer::lastTitle() {
    const std::lock_guard lock{keep};
    return newestTitle;
}

FeedViewer::FeedViewer(Window * parent, const Source& s)
    : Window{parent}
    , source{s}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(source.title);
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    items = UiStandards::combo(top);
    auto * newer = UiStandards::button(top, "<", "The newer one");
    auto * older = UiStandards::button(top, ">", "The older one");
    openItem = UiStandards::button(top, "On the site...", "This item on the site it comes from");
    openChart = UiStandards::button(top, "The chart on the IEM site...", "Makes the same chart with the site's own tools");
    openChart->setVisible(false);
    openVote = UiStandards::button(top, "Good chart survey...", "The site asks readers to rate each feature Good, Bad or Abstain: the newest on its front page, an older one on its own page. Your vote is theirs, made on their site");
    openVote->setVisible(!source.vote.empty());
    topLayout->addWidget(items, 1);
    topLayout->addWidget(newer);
    topLayout->addWidget(older);
    topLayout->addWidget(openItem);
    topLayout->addWidget(openChart);
    topLayout->addWidget(openVote);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    note = new QLabel{"Reading the feed...", this};
    note->setWordWrap(true);
    UiStandards::muted(note);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    auto * row = new QWidget{this};
    auto * rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    text = new QPlainTextEdit{row};
    text->setReadOnly(true);
    text->setMinimumWidth(320);
    UtilityTextReflow::style(text);
    rowLayout->addWidget(text, 2);
    rowLayout->addWidget(&image, 4);
    box.addWidgetReal(row, 1, Qt::Alignment{});
    auto * credit = new QLabel{QString::fromStdString(source.credit), this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1100, 700);
    QObject::connect(items, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int index) { show(index); });
    QObject::connect(newer, &QPushButton::clicked, this, [this] { items->setCurrentIndex(std::max(0, items->currentIndex() - 1)); });
    QObject::connect(older, &QPushButton::clicked, this, [this] { items->setCurrentIndex(std::min(items->count() - 1, items->currentIndex() + 1)); });
    QObject::connect(openItem, &QPushButton::clicked, this, [this] {
        const int i = items->currentIndex();
        QDesktopServices::openUrl(QUrl{i >= 0 && static_cast<size_t>(i) < list.size() && !list[static_cast<size_t>(i)].link.isEmpty() ? list[static_cast<size_t>(i)].link : QString::fromStdString(source.site)});
    });
    QObject::connect(openVote, &QPushButton::clicked, this, [this] {   // the front page asks about the newest item; an older one has its question on its own page
        const int i = items->currentIndex();
        QDesktopServices::openUrl(QUrl{i > 0 && static_cast<size_t>(i) < list.size() && !list[static_cast<size_t>(i)].link.isEmpty() ? list[static_cast<size_t>(i)].link : QString::fromStdString(source.vote)});
    });
    QObject::connect(openChart, &QPushButton::clicked, this, [this] {
        const int i = items->currentIndex();
        if (i >= 0 && static_cast<size_t>(i) < list.size() && !list[static_cast<size_t>(i)].extraLink.isEmpty()) {
            QDesktopServices::openUrl(QUrl{list[static_cast<size_t>(i)].extraLink});
        }
    });
    auto got = std::make_shared<vector<Item>>();
    const auto feed = source;
    new FutureVoid{this, [got, feed] { *got = read(feed); }, [this, got] {
        if (closed) {
            return;
        }
        if (got->empty()) {
            note->setText("The feed could not be read.");
            return;
        }
        list = *got;
        QSignalBlocker block{items};
        for (const auto& item : list) {
            items->addItem((item.date.isEmpty() ? QString{} : item.date + "  -  ") + item.title);
        }
        note->setText(QString::number(list.size()) + " items, the newest first. A click on the picture zooms it.");
        show(0);
    }};
}

void FeedViewer::show(int index) {
    if (index < 0 || static_cast<size_t>(index) >= list.size()) {
        return;
    }
    const auto& item = list[static_cast<size_t>(index)];
    text->setPlainText(item.title + (item.date.isEmpty() ? QString{} : "\n" + item.date) + "\n\n" + item.text);
    openChart->setVisible(!item.extraLink.isEmpty());
    const int mine = ++generation;
    image.setBytes(QByteArray{});
    new FutureBytes{this, item.imageUrl.toStdString(), [this, mine] (const QByteArray& bytes) {
        if (!closed && mine == generation) {
            image.setBytes(bytes);
        }
    }};
}
