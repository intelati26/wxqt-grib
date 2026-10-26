// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TABLEEXPORTDIALOG_H
#define TABLEEXPORTDIALOG_H

#include <functional>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QString>
#include <QStringList>

// What to put in a table that is saved as CSV, copied for a spreadsheet or drawn as a picture, for any table of items (a watch, a place, a station) by fields (a hazard, a parameter), as the forecast
// points' export does: which items, which fields, which parts of a value (a per cent and its level), the layout (items down and fields across as on the screen, the other way round, or a line for each item
// and field), a first line that says what the table is. The choices are kept for the next time under `prefix`. The caller gives the table as a Dataset and does no drawing or writing.
class TableExportDialog : public QDialog {
public:
    struct Item {
        QString key, label;
        QStringList details;   // more columns of the item (the time a watch ends), named in Dataset::detailNames
    };
    struct Field {
        QString key, label;
    };
    struct Part {
        QString key, label;    // "percent" "Per cent": the pieces a value comes in; one part needs no choice
    };
    struct Dataset {
        QString prefix;        // the saved choices' name ("WATCHPROB")
        QString windowTitle;   // "Export watch probabilities"
        QString title;         // the table's heading: the first line of the text, over the picture
        QString credit;        // under the picture
        QString itemHeader;    // the first column: "Watch"
        QStringList detailNames;
        std::vector<Item> items;
        std::vector<Field> fields;
        std::vector<Part> parts;
        std::function<QString(size_t item, size_t field, size_t part)> cell;   // the text of a value; empty when there is none
        std::function<QColor(size_t item, size_t field)> tint;                 // the picture's color behind it (invalid: none)
    };
    TableExportDialog(QWidget * parent, Dataset data);
    // what the buttons write, for a test or a caller that saves without asking
    QString text(bool csv) { return build(csv); }
    QImage picture() { return draw(); }

private:
    QString build(bool csv);
    QImage draw();
    void saveCsv();
    void saveImage();
    void copyText();
    void remember();
    std::vector<size_t> ticked(const QListWidget * list) const;
    static QListWidget * column(QDialog * owner, QHBoxLayout * lists, const char * title);
    Dataset data;
    QComboBox * layoutBox{};
    QCheckBox * header{};
    QCheckBox * details{};
    QListWidget * itemList{};
    QListWidget * fieldList{};
    QListWidget * partList{};
    QLabel * status{};
};

#endif  // TABLEEXPORTDIALOG_H
