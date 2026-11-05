// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NWSNOTICESVIEWER_H
#define NWSNOTICESVIEWER_H

#include <string>
#include <vector>
#include <QComboBox>
#include <QDate>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include "ui/Window.h"

// The National Weather Service's notices of changes to products and services, from https://www.weather.gov/notification/: the Service Change Notices (SCN) and the Public
// Information Statements (PNS: the proposals that ask for comments, tests, new names), the newest first. A row opens the notice (a PDF) in the browser.
class NwsNoticesViewer : public Window {
public:
    explicit NwsNoticesViewer(Window * parent);

    struct Notice {
        QDate date;
        std::string kind;    // "SCN" or "PNS"
        std::string id;      // "SCN26-100 Updated"
        std::string title;
        std::string url;
    };
    // the notices the page lists, in the page's order (the newest of each kind first); empty when the page could not be read or has none
    static std::vector<Notice> parse(const std::string& html);

private:
    void load();
    void fill();
    void closeEventCustom() override { closed = true; }

    QComboBox * kindPick{};
    QLineEdit * filter{};
    QLabel * status{};
    QTableWidget * table{};
    std::vector<Notice> notices;
    int generation{0};
    bool closed{false};
};

#endif  // NWSNOTICESVIEWER_H
