// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/CacheSummaryViewer.h"
#include <algorithm>
#include <map>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

qint64 CacheSummaryViewer::Source::files() const {
    qint64 n = 0;
    for (const auto& g : groups) {
        n += g.files;
    }
    return n;
}

qint64 CacheSummaryViewer::Source::bytes() const {
    qint64 n = 0;
    for (const auto& g : groups) {
        n += g.bytes;
    }
    return n;
}

namespace {
    QString sizeText(qint64 bytes) {
        if (bytes >= 1024LL * 1024 * 1024) {
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        }
        if (bytes >= 1024LL * 1024) {
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        return QString::number(bytes / 1024.0, 'f', 0) + " KB";
    }

    QString dateText(const QDateTime& t) {
        return t.isValid() ? t.toString("yyyy-MM-dd HH:mm") : QString{};
    }

    // the kind of a file inside a source: its name up to the first of the separators, for the folders whose names start with the model, product or kind
    enum class By { Nothing, Prefix, Token3, Subfolder };

    struct Spec {
        QString name;
        QString folder;
        QString rule;
        bool permanent;
        By by;
    };

    std::vector<Spec> specs() {
        const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        const QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const int hours = Utility::readPrefInt("MODEL_CACHE_HOURS", 48);
        const int megabytes = Utility::readPrefInt("MODEL_CACHE_MB", 2048);
        return {
            {"Model data slices (GFS, RRFS, HRRR, ...)", cache + "/gfs-v3", QString{"older than %1 hours go, then the oldest past %2 MB (Settings)"}.arg(hours).arg(megabytes), false, By::Prefix},
            {"Run files: GRIB slices, charts, station soundings, map lines", temp + "/wxqt_grib", "removed when a newer run is read; the temporary folder is cleared when the computer restarts", false, By::Prefix},
            {"MRMS scans (decoded)", temp + "/wxqt_mrms", "dropped after a few hours when a scan list is read", false, By::Token3},
            {"SPC Post pictures", temp + "/wxqt_spcpost", "by run", false, By::Prefix},
            {"Satellite tiles (NASA GIBS)", cache + "/gibs", "complete days only: older than 30 days go, then the oldest past 600 MB", false, By::Nothing},
            {"CPC daily rain analysis (master map)", cache + "/cpc-precip", "days past 120 are removed; the last two weeks are asked for again until the analysis is reprocessed", false, By::Nothing},
            {"NSSL CAMs answers", cache + "/cams", "model list 6 hours, sectors a week, the rest asked live", false, By::Nothing},
            {"Hurricane files", cache + "/hurricane", "by age", false, By::Nothing},
            {"Tornado files", cache + "/tornado", "by age", false, By::Nothing},
            {"Kept data (climate, drought, hurricane, recon, tornado)", data + "/data", "the newest version is kept, older removed: this is data, not a cache", true, By::Subfolder},
            {"Archive (history plan)", data + "/archive", "never removed by the program", true, By::Subfolder},
        };
    }

    QString groupOf(const QString& relative, By by) {
        const QString file = relative.section('/', -1);
        switch (by) {
            case By::Nothing: return "files";
            case By::Subfolder: return relative.contains('/') ? relative.section('/', 0, 0) : QString{"files"};
            case By::Token3: {
                const auto parts = file.split('_');   // mrmsd4_MRMS_<product>_...
                return parts.size() > 2 ? parts[2].left(40) : file.section('.', 0, 0);
            }
            default: {
                QString prefix = file.section('_', 0, 0).section('.', 0, 0);
                if (prefix.isEmpty()) {
                    prefix = "files";
                }
                static const std::map<QString, QString> names{{"g", "GRIB slices"}, {"gc", "GRIB slices"}, {"gu", "GRIB slices (wind)"}, {"gv", "GRIB slices (wind)"}, {"gg", "GRIB slices"},
                                                              {"r", "charts"}, {"r3", "charts"}, {"r4", "charts"}, {"r5", "charts"}, {"r6", "charts"}, {"r7", "charts"},
                                                              {"bk", "BUFKIT station files"}, {"snd", "model sounding data"}};
                const auto found = names.find(prefix);
                return found == names.end() ? prefix : found->second;
            }
        }
    }
}

CacheSummaryViewer::CacheSummaryViewer(Window * parent) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Data kept - caches and what is in them");
    auto * column = new QVBoxLayout;
    auto * top = new QHBoxLayout;
    auto * refresh = UiStandards::button(this, "Refresh");
    auto * open = UiStandards::button(this, "Open folder");
    auto * remove = UiStandards::button(this, "Delete selected...");
    total = new QLabel{"Reading the folders...", this};
    top->addWidget(refresh);
    top->addWidget(open);
    top->addWidget(remove);
    top->addWidget(total, 1);
    column->addLayout(top);
    tree = new QTreeWidget{this};
    tree->setColumnCount(6);
    tree->setHeaderLabels({"Source / kind", "Files", "Size", "Oldest", "Newest", "Removed by"});
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column <= 4; column++) {
        tree->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(true);
    column->addWidget(tree, 1);
    auto * central = new QWidget{this};
    central->setLayout(column);
    setCentralWidget(central);
    setSize(1100, 640);
    QWidget::show();
    QObject::connect(refresh, &QPushButton::clicked, [this] { scan(); });
    QObject::connect(open, &QPushButton::clicked, [this] {
        auto * item = tree->currentItem();
        while (item != nullptr && item->parent() != nullptr) {
            item = item->parent();
        }
        if (item != nullptr) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(0, Qt::UserRole).toString()));
        }
    });
    QObject::connect(remove, &QPushButton::clicked, [this] {
        auto * item = tree->currentItem();
        if (item == nullptr) {
            return;
        }
        const int source = item->data(0, Qt::UserRole + 1).toInt();
        const int group = item->data(0, Qt::UserRole + 2).toInt();
        if (source < 0 || source >= static_cast<int>(sources.size())) {
            return;
        }
        const auto& s = sources[static_cast<size_t>(source)];
        std::vector<QString> paths;
        qint64 bytes = 0;
        for (size_t g = 0; g < s.groups.size(); g++) {
            if (group < 0 || static_cast<int>(g) == group) {
                paths.insert(paths.end(), s.groups[g].paths.begin(), s.groups[g].paths.end());
                bytes += s.groups[g].bytes;
            }
        }
        const QString what = group < 0 ? s.name : s.name + ": " + s.groups[static_cast<size_t>(group)].name;
        const QString warning = s.permanent ? "\n\nThis is kept data, not a cache: it is not downloaded again by itself." : "\n\nIt is downloaded again when it is next needed.";
        if (QMessageBox::question(this, "Delete", "Delete " + QString::number(paths.size()) + " files (" + sizeText(bytes) + ") of\n" + what + "?" + warning) != QMessageBox::Yes) {
            return;
        }
        for (const auto& path : paths) {
            QFile::remove(path);
        }
        scan();
    });
    scan();
}

void CacheSummaryViewer::scan() {
    total->setText("Reading the folders...");
    generation += 1;
    const int mine = generation;
    auto result = std::make_shared<std::vector<Source>>();
    new FutureVoid{this,
        [result] {
            for (const auto& spec : specs()) {
                Source source;
                source.name = spec.name;
                source.folder = spec.folder;
                source.rule = spec.rule;
                source.permanent = spec.permanent;
                std::map<QString, Group> groups;
                QDirIterator it{spec.folder, QDir::Files, QDirIterator::Subdirectories};
                const QDir base{spec.folder};
                while (it.hasNext()) {
                    const QString path = it.next();
                    const QFileInfo info{path};
                    auto& g = groups[groupOf(base.relativeFilePath(path), spec.by)];
                    g.name = groupOf(base.relativeFilePath(path), spec.by);
                    g.files++;
                    g.bytes += info.size();
                    const auto when = info.lastModified();
                    if (!g.oldest.isValid() || when < g.oldest) {
                        g.oldest = when;
                    }
                    if (!g.newest.isValid() || when > g.newest) {
                        g.newest = when;
                    }
                    g.paths.push_back(path);
                }
                for (auto& [name, g] : groups) {
                    source.groups.push_back(std::move(g));
                }
                std::sort(source.groups.begin(), source.groups.end(), [] (const Group& a, const Group& b) { return a.bytes > b.bytes; });
                result->push_back(std::move(source));
            }
        },
        [this, result, mine] {
            if (closed || mine != generation) {
                return;
            }
            show(*result);
        }};
}

void CacheSummaryViewer::show(const std::vector<Source>& found) {
    sources = found;
    tree->clear();
    qint64 all = 0, files = 0;
    for (size_t s = 0; s < sources.size(); s++) {
        const auto& source = sources[s];
        all += source.bytes();
        files += source.files();
        auto * top = new QTreeWidgetItem{tree};
        top->setText(0, source.name + (source.groups.empty() ? "   (empty)" : ""));
        top->setText(1, QString::number(source.files()));
        top->setText(2, sizeText(source.bytes()));
        QDateTime oldest, newest;
        for (const auto& g : source.groups) {
            if (!oldest.isValid() || g.oldest < oldest) oldest = g.oldest;
            if (!newest.isValid() || g.newest > newest) newest = g.newest;
        }
        top->setText(3, dateText(oldest));
        top->setText(4, dateText(newest));
        top->setText(5, source.rule);
        top->setToolTip(0, source.folder);
        top->setData(0, Qt::UserRole, source.folder);
        top->setData(0, Qt::UserRole + 1, static_cast<int>(s));
        top->setData(0, Qt::UserRole + 2, -1);
        QFont bold = top->font(0);
        bold.setBold(true);
        top->setFont(0, bold);
        for (size_t g = 0; g < source.groups.size(); g++) {
            const auto& group = source.groups[g];
            auto * child = new QTreeWidgetItem{top};
            child->setText(0, group.name);
            child->setText(1, QString::number(group.files));
            child->setText(2, sizeText(group.bytes));
            child->setText(3, dateText(group.oldest));
            child->setText(4, dateText(group.newest));
            child->setData(0, Qt::UserRole + 1, static_cast<int>(s));
            child->setData(0, Qt::UserRole + 2, static_cast<int>(g));
        }
        top->setExpanded(source.bytes() > 50LL * 1024 * 1024);
    }
    total->setText(QString::number(files) + " files, " + sizeText(all) + " on disk");
}
