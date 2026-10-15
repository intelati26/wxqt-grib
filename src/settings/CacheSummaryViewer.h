// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CACHESUMMARYVIEWER_H
#define CACHESUMMARYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include "ui/Window.h"

// What the program keeps on disk: every cache and data folder it uses, by source and by kind of file (count, size, oldest, newest, and the rule that removes it), with
// open-folder and delete for any group. The first piece of the history plan (docs/history-archive-plan.md): you cannot decide what to keep without seeing what there is.
class CacheSummaryViewer : public Window {
public:
    explicit CacheSummaryViewer(Window * parent);

    struct Group {
        QString name;          // "AIGFS", "BUFKIT stations" ...
        qint64 files{0};
        qint64 bytes{0};
        QDateTime oldest, newest;
        std::vector<QString> paths;   // the files, for delete
    };
    struct Source {
        QString name;          // "Model data slices"
        QString folder;
        QString rule;          // what removes them
        bool permanent{false}; // not a cache: data that is kept
        std::vector<Group> groups;
        qint64 files() const;
        qint64 bytes() const;
    };

private:
    void scan();
    void show(const std::vector<Source>&);
    void closeEventCustom() override { closed = true; }
    QTreeWidget * tree{};
    QLabel * total{};
    bool closed{false};
    int generation{0};
    std::vector<Source> sources;
};

#endif  // CACHESUMMARYVIEWER_H
