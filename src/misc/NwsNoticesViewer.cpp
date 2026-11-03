// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/NwsNoticesViewer.h"
#include <algorithm>
#include <memory>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "ui/UiStandards.h"

using std::string;
using std::vector;

namespace {
    const string site = "https://www.weather.gov";
    const string page = site + "/notification/";

    QString plain(const QString& html) {
        return QTextDocumentFragment::fromHtml(html).toPlainText().simplified();
    }
}

vector<NwsNoticesViewer::Notice> NwsNoticesViewer::parse(const string& html) {
    vector<Notice> out;
    const auto text = QString::fromStdString(html);
    // the page has a heading and a list for each kind: "Public Information Statements 2026" (anchor "pns") and "Service Change Notices 2026" (anchor "scn")
    const QRegularExpression heading{R"re(<h2>.*?name="(pns|scn)".*?(\d{4}).*?</h2>\s*<ul>(.*?)</ul>)re", QRegularExpression::DotMatchesEverythingOption};
    const QRegularExpression item{R"re(<li>(.*?)</li>)re", QRegularExpression::DotMatchesEverythingOption};
    const QRegularExpression link{R"re(<a [^>]*href="([^"]+)"[^>]*>(.*?)</a>)re", QRegularExpression::DotMatchesEverythingOption};
    const QRegularExpression dated{R"re((Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+(\d{1,2}))re"};
    const QRegularExpression named{R"re(((?:PNS|SCN)\d{2}-\d+[^:]*?)\s*:\s*(.*)$)re"};
    static const QStringList months{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (auto sections = heading.globalMatch(text); sections.hasNext();) {
        const auto section = sections.next();
        const string kind = section.captured(1) == "pns" ? "PNS" : "SCN";
        const int year = section.captured(2).toInt();
        for (auto items = item.globalMatch(section.captured(3)); items.hasNext();) {
            const auto row = items.next().captured(1);
            const auto anchor = link.match(row);
            if (!anchor.hasMatch()) {
                continue;
            }
            const auto whole = plain(row);   // "Oct 30: SCN26-97: Increasing ..." (the date is before the link, or in it)
            const auto day = dated.match(whole.left(12));
            const auto titled = named.match(whole);
            if (!titled.hasMatch()) {
                continue;
            }
            Notice notice;
            notice.kind = kind;
            if (day.hasMatch()) {
                notice.date = QDate{year, static_cast<int>(months.indexOf(day.captured(1))) + 1, day.captured(2).toInt()};
            }
            notice.id = titled.captured(1).toStdString();
            notice.title = titled.captured(2).toStdString();
            auto url = anchor.captured(1).toStdString();
            notice.url = url.compare(0, 4, "http") == 0 ? url : site + (url.empty() || url[0] != '/' ? "/notification/" : "") + url;
            out.push_back(std::move(notice));
        }
    }
    return out;
}

NwsNoticesViewer::NwsNoticesViewer(Window * parent) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("NWS service change notices");
    auto * body = new QWidget{this};
    auto * column = new QVBoxLayout{body};
    auto * top = new QHBoxLayout;
    kindPick = new QComboBox{body};
    kindPick->addItem("All notices", "");
    kindPick->addItem("Service Change Notices (SCN)", "SCN");
    kindPick->addItem("Public Information Statements (PNS)", "PNS");
    kindPick->setToolTip("SCN: a change to a product or service that is decided. PNS: a proposal that asks for public comment, a test, or an announcement.");
    top->addWidget(kindPick);
    filter = new QLineEdit{body};
    filter->setPlaceholderText("Filter the titles (GFS, HRRR, radar...)");
    filter->setClearButtonEnabled(true);
    top->addWidget(filter, 1);
    auto * reload = UiStandards::button(body, "Reload");
    top->addWidget(reload);
    auto * open = UiStandards::button(body, "Open the notices page");
    top->addWidget(open);
    column->addLayout(top);
    status = new QLabel{"Reading weather.gov/notification...", body};
    column->addWidget(status);
    table = new QTableWidget{0, 4, body};
    table->setHorizontalHeaderLabels({"Date", "Notice", "Subject", ""});
    table->setColumnHidden(3, true);   // the address, for the click
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setWordWrap(true);
    table->setToolTip("Click a row to open the notice (a PDF) in the browser.");
    column->addWidget(table, 1);
    setCentralWidget(body);
    setSize(1200, 800);
    QObject::connect(reload, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl{QString::fromStdString(page)}); });
    QObject::connect(kindPick, &QComboBox::currentIndexChanged, this, [this] { fill(); });
    QObject::connect(filter, &QLineEdit::textChanged, this, [this] { fill(); });
    QObject::connect(table, &QTableWidget::cellClicked, this, [this] (int row, int) {
        if (const auto * item = table->item(row, 3)) {
            QDesktopServices::openUrl(QUrl{item->text()});
        }
    });
    QWidget::show();
    load();
    auto * again = new QTimer{this};   // the page is read again every half hour while the window is open (NWS posts a few notices a day)
    again->setInterval(30 * 60 * 1000);
    QObject::connect(again, &QTimer::timeout, this, [this] { load(); });
    again->start();
}

void NwsNoticesViewer::load() {
    const int mine = ++generation;
    status->setText("Reading weather.gov/notification...");
    auto html = std::make_shared<string>();
    new FutureVoid{this,
        [html] { *html = URL::getText(page); },
        [this, html, mine] {
            if (closed || mine != generation) {
                return;
            }
            notices = parse(*html);
            if (notices.empty()) {
                status->setText(html->empty() ? "weather.gov did not answer." : "The page has no notices in the form this screen reads: it may have changed. Open the notices page.");
            }
            fill();
        }};
}

void NwsNoticesViewer::fill() {
    const auto kind = kindPick->currentData().toString().toStdString();
    const auto words = filter->text().trimmed();
    vector<const Notice *> shown;
    for (const auto& notice : notices) {
        if ((kind.empty() || notice.kind == kind) && (words.isEmpty() || QString::fromStdString(notice.title + " " + notice.id).contains(words, Qt::CaseInsensitive))) {
            shown.push_back(&notice);
        }
    }
    std::stable_sort(shown.begin(), shown.end(), [] (const Notice * a, const Notice * b) { return a->date > b->date; });   // the newest first, the two kinds together
    table->setRowCount(static_cast<int>(shown.size()));
    const auto today = QDate::currentDate();
    for (int i = 0; i < static_cast<int>(shown.size()); i++) {
        const auto& notice = *shown[static_cast<size_t>(i)];
        table->setItem(i, 0, new QTableWidgetItem{notice.date.isValid() ? notice.date.toString("MMM d, yyyy") : QString{}});
        auto * id = new QTableWidgetItem{QString::fromStdString(notice.id)};
        if (notice.date.isValid() && notice.date.daysTo(today) <= 7) {   // the last week's in bold
            auto font = id->font();
            font.setBold(true);
            id->setFont(font);
        }
        table->setItem(i, 1, id);
        table->setItem(i, 2, new QTableWidgetItem{QString::fromStdString(notice.title)});
        table->setItem(i, 3, new QTableWidgetItem{QString::fromStdString(notice.url)});
    }
    table->resizeColumnToContents(0);
    table->resizeColumnToContents(1);
    table->resizeRowsToContents();
    if (!notices.empty()) {
        status->setText(QString::number(shown.size()) + " of " + QString::number(notices.size()) + " notices, newest first (read again every 30 minutes, " + QTime::currentTime().toString("HH:mm") + "). The ones of the last week are in bold. A click opens the notice.");
    }
}
