// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/TableExportDialog.h"
#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include "ui/ChartExport.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"

namespace {
    QString field(QString text, bool csv) {
        if (!csv) {
            return text.replace('\t', ' ').replace('\n', ' ');
        }
        if (text.contains(',') || text.contains('"') || text.contains('\n')) {
            text.replace('"', "\"\"");
            return '"' + text + '"';
        }
        return text;
    }
}

QListWidget * TableExportDialog::column(QDialog * owner, QHBoxLayout * lists, const char * title) {
    auto * box = new QGroupBox{title, owner};
    auto * inner = new QVBoxLayout{box};
    auto * list = new QListWidget{box};
    list->setSelectionMode(QAbstractItemView::NoSelection);
    auto * row = new QHBoxLayout;
    auto * all = UiStandards::button(box, "All");
    auto * none = UiStandards::button(box, "None");
    const auto setAll = [list] (Qt::CheckState state) {
        for (int i = 0; i < list->count(); i++) {
            list->item(i)->setCheckState(state);
        }
    };
    QObject::connect(all, &QPushButton::clicked, list, [setAll] { setAll(Qt::Checked); });
    QObject::connect(none, &QPushButton::clicked, list, [setAll] { setAll(Qt::Unchecked); });
    row->addWidget(all);
    row->addWidget(none);
    row->addStretch();
    inner->addWidget(list, 1);
    inner->addLayout(row);
    lists->addWidget(box, 1);
    return list;
}

TableExportDialog::TableExportDialog(QWidget * parent, Dataset d)
    : QDialog{parent}
    , data{std::move(d)}
{
    setWindowTitle(data.windowTitle);
    auto * whole = new QVBoxLayout{this};
    auto * top = new QHBoxLayout;
    layoutBox = UiStandards::combo(this);
    layoutBox->addItems({data.itemHeader + "s down, the fields across (as on the screen)", "The fields down, " + data.itemHeader.toLower() + "s across", "One line for each " + data.itemHeader.toLower() + " and field"});
    layoutBox->setToolTip("The picture is always drawn with the " + data.itemHeader.toLower() + "s down");
    header = new QCheckBox{"Say what it is", this};
    header->setToolTip("A first line with the table's title");
    details = new QCheckBox{"With " + (data.detailNames.isEmpty() ? QString{"the details"} : data.detailNames.join(", ").toLower()), this};
    details->setVisible(!data.detailNames.isEmpty());
    top->addWidget(layoutBox);
    top->addWidget(header);
    top->addWidget(details);
    top->addStretch();
    whole->addLayout(top);
    auto * lists = new QHBoxLayout;
    itemList = column(this, lists, data.itemHeader.toStdString().c_str());
    fieldList = column(this, lists, "Fields");
    partList = column(this, lists, "Values");
    partList->parentWidget()->setVisible(data.parts.size() > 1);
    whole->addLayout(lists, 1);
    status = new QLabel{this};
    whole->addWidget(status);
    auto * buttons = new QHBoxLayout;
    auto * image = UiStandards::button(this, "Save image...");
    auto * save = UiStandards::button(this, "Save CSV...");
    auto * copy = UiStandards::button(this, "Copy", "Copy the table for a spreadsheet");
    auto * close = UiStandards::button(this, "Close");
    buttons->addStretch();
    buttons->addWidget(image);
    buttons->addWidget(save);
    buttons->addWidget(copy);
    buttons->addWidget(close);
    whole->addLayout(buttons);

    // what was chosen last time
    const auto prefix = data.prefix.toStdString();
    layoutBox->setCurrentIndex(std::clamp(Utility::readPrefInt(prefix + "_LAYOUT", 0), 0, 2));
    header->setChecked(Utility::readPref(prefix + "_HEADER", "true") == "true");
    details->setChecked(Utility::readPref(prefix + "_DETAILS", "true") == "true");
    const auto marks = [&prefix] (const char * what) { return QString::fromStdString(Utility::readPref(prefix + what, "")).split('|', Qt::SkipEmptyParts); };
    const auto fieldMarks = marks("_FIELDS"), partMarks = marks("_PARTS");
    const auto choice = [] (const QStringList& list, const QString& key) {   // "+key" ticked, "-key" not: one that appears later is ticked
        for (const auto& m : list) {
            if (m.size() > 1 && m.mid(1) == key) {
                return m[0] == '+';
            }
        }
        return true;
    };
    const auto add = [] (QListWidget * list, const QString& text, const QString& key, bool on) {
        auto * item = new QListWidgetItem{text, list};
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, key);
    };
    for (const auto& item : data.items) {
        add(itemList, item.label, item.key, true);
    }
    for (const auto& f : data.fields) {
        add(fieldList, f.label, f.key, choice(fieldMarks, f.key));
    }
    for (const auto& p : data.parts) {
        add(partList, p.label, p.key, choice(partMarks, p.key));
    }
    QObject::connect(image, &QPushButton::clicked, this, [this] { saveImage(); });
    QObject::connect(save, &QPushButton::clicked, this, [this] { saveCsv(); });
    QObject::connect(copy, &QPushButton::clicked, this, [this] { copyText(); });
    QObject::connect(close, &QPushButton::clicked, this, [this] { remember(); accept(); });
    QObject::connect(this, &QDialog::finished, this, [this] { remember(); });
    resize(760, 520);
}

void TableExportDialog::remember() {
    const auto prefix = data.prefix.toStdString();
    Utility::writePrefInt(prefix + "_LAYOUT", layoutBox->currentIndex());
    Utility::writePref(prefix + "_HEADER", header->isChecked() ? "true" : "false");
    Utility::writePref(prefix + "_DETAILS", details->isChecked() ? "true" : "false");
    const auto marks = [] (const QListWidget * list) {
        QStringList out;
        for (int i = 0; i < list->count(); i++) {
            out << (list->item(i)->checkState() == Qt::Checked ? "+" : "-") + list->item(i)->data(Qt::UserRole).toString();
        }
        return out.join('|').toStdString();
    };
    Utility::writePref(prefix + "_FIELDS", marks(fieldList));
    Utility::writePref(prefix + "_PARTS", marks(partList));
}

std::vector<size_t> TableExportDialog::ticked(const QListWidget * list) const {
    std::vector<size_t> out;
    for (int i = 0; i < list->count(); i++) {
        if (list->item(i)->checkState() == Qt::Checked) {
            out.push_back(static_cast<size_t>(i));
        }
    }
    return out;
}

QString TableExportDialog::build(bool csv) {
    const auto items = ticked(itemList), fields = ticked(fieldList);
    auto parts = ticked(partList);
    if (data.parts.size() <= 1) {
        parts = {0};
    }
    if (items.empty() || fields.empty() || parts.empty()) {
        status->setText("Tick at least one " + data.itemHeader.toLower() + ", one field and one value.");
        return {};
    }
    status->clear();
    const QString sep = csv ? "," : "\t";
    const bool withDetails = details->isChecked() && !data.detailNames.isEmpty();
    const auto name = [this, &parts] (size_t f, size_t p) {   // the heading of a field's value
        return parts.size() > 1 ? data.fields[f].label + " (" + data.parts[p].label.toLower() + ")" : data.fields[f].label;
    };
    QString out;
    if (header->isChecked()) {
        out += field("# " + data.title, false) + "\n";
    }
    QStringList line;
    switch (layoutBox->currentIndex()) {
    case 0: {   // the items down, the fields across
        line << field(data.itemHeader, csv);
        if (withDetails) {
            for (const auto& n : data.detailNames) line << field(n, csv);
        }
        for (const auto f : fields) for (const auto p : parts) line << field(name(f, p), csv);
        out += line.join(sep) + "\n";
        for (const auto i : items) {
            line = {field(data.items[i].label, csv)};
            if (withDetails) for (const auto& v : data.items[i].details) line << field(v, csv);
            for (const auto f : fields) for (const auto p : parts) line << field(data.cell(i, f, p), csv);
            out += line.join(sep) + "\n";
        }
        break;
    }
    case 1: {   // the fields down, the items across
        line << field("", csv);
        for (const auto i : items) line << field(data.items[i].label, csv);
        out += line.join(sep) + "\n";
        if (withDetails) {
            for (int d = 0; d < data.detailNames.size(); d++) {
                line = {field(data.detailNames[d], csv)};
                for (const auto i : items) line << field(d < data.items[i].details.size() ? data.items[i].details[d] : QString{}, csv);
                out += line.join(sep) + "\n";
            }
        }
        for (const auto f : fields) {
            for (const auto p : parts) {
                line = {field(name(f, p), csv)};
                for (const auto i : items) line << field(data.cell(i, f, p), csv);
                out += line.join(sep) + "\n";
            }
        }
        break;
    }
    default: {   // a line for each item and field
        line << field(data.itemHeader, csv);
        if (withDetails) for (const auto& n : data.detailNames) line << field(n, csv);
        line << field("Field", csv);
        for (const auto p : parts) line << field(data.parts.size() > 1 ? data.parts[p].label : QString{"Value"}, csv);
        out += line.join(sep) + "\n";
        for (const auto i : items) {
            for (const auto f : fields) {
                line = {field(data.items[i].label, csv)};
                if (withDetails) for (const auto& v : data.items[i].details) line << field(v, csv);
                line << field(data.fields[f].label, csv);
                for (const auto p : parts) line << field(data.cell(i, f, p), csv);
                out += line.join(sep) + "\n";
            }
        }
        break;
    }
    }
    return out;
}

// the table as a picture: the items down, the fields across, each value tinted as on the screen (the parts of a value together)
QImage TableExportDialog::draw() {
    const auto items = ticked(itemList), fields = ticked(fieldList);
    auto parts = ticked(partList);
    if (data.parts.size() <= 1) {
        parts = {0};
    }
    if (items.empty() || fields.empty() || parts.empty()) {
        status->setText("Tick at least one " + data.itemHeader.toLower() + ", one field and one value.");
        return {};
    }
    QFont font{QApplication::font()};
    font.setPixelSize(13);
    QFont bold{font};
    bold.setBold(true);
    QFont small{font};
    small.setPixelSize(11);
    const QFontMetrics fm{font}, sm{small};
    const bool withDetails = details->isChecked() && !data.detailNames.isEmpty();
    const int pad = 12, rowH = 26, headH = 62;
    int labelW = fm.horizontalAdvance(data.itemHeader) + 16;
    for (const auto i : items) {
        labelW = std::max(labelW, fm.horizontalAdvance(data.items[i].label) + 16);
    }
    std::vector<int> detailW;
    for (int d = 0; withDetails && d < data.detailNames.size(); d++) {
        int w = fm.horizontalAdvance(data.detailNames[d]) + 16;
        for (const auto i : items) {
            w = std::max(w, d < data.items[i].details.size() ? fm.horizontalAdvance(data.items[i].details[d]) + 16 : 0);
        }
        detailW.push_back(w);
    }
    std::vector<int> cellW;
    std::vector<QStringList> texts(items.size());
    for (size_t n = 0; n < items.size(); n++) {
        for (const auto f : fields) {
            QStringList pieces;
            for (const auto p : parts) {
                const auto t = data.cell(items[n], f, p);
                if (!t.isEmpty()) pieces << t;
            }
            texts[n] << pieces.join("  ");
        }
    }
    for (size_t c = 0; c < fields.size(); c++) {
        int w = 74;
        for (size_t n = 0; n < items.size(); n++) {
            w = std::max(w, fm.horizontalAdvance(texts[n][static_cast<int>(c)]) + 20);
        }
        cellW.push_back(w);
    }
    int width = pad * 2 + labelW;
    for (const auto w : detailW) width += w;
    for (const auto w : cellW) width += w;
    const int height = pad + 28 + headH + rowH * static_cast<int>(items.size()) + 44;
    QImage image{std::max(width, 420), height, QImage::Format_RGB32};
    image.fill(Qt::white);
    QPainter p{&image};
    p.setRenderHint(QPainter::TextAntialiasing);
    int y = pad;
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QRect{pad, y, image.width() - 2 * pad, 24}, Qt::AlignVCenter | Qt::AlignLeft, data.title);
    y += 28;
    int x = pad;
    const auto head = [&] (int w, const QString& text) {
        const QRect cell{x, y, w, headH};
        p.fillRect(cell, QColor{225, 232, 240});
        p.setFont(small);
        p.setPen(QColor{40, 40, 40});
        p.drawText(cell.adjusted(4, 2, -4, -2), Qt::AlignCenter | Qt::TextWordWrap, text);
        p.setPen(QColor{200, 205, 212});
        p.drawRect(cell);
        x += w;
    };
    head(labelW, data.itemHeader);
    for (size_t d = 0; d < detailW.size(); d++) head(detailW[d], data.detailNames[static_cast<int>(d)]);
    for (size_t c = 0; c < fields.size(); c++) head(cellW[c], data.fields[fields[c]].label);
    y += headH;
    p.setFont(font);
    for (size_t n = 0; n < items.size(); n++) {
        x = pad;
        const auto box = [&] (int w, const QString& text, const QColor& tint, bool left) {
            const QRect cell{x, y, w, rowH};
            if (tint.isValid()) {
                p.fillRect(cell, tint);
            }
            p.setPen(QColor{20, 20, 20});
            p.drawText(left ? cell.adjusted(6, 0, -4, 0) : cell, (left ? Qt::AlignVCenter | Qt::AlignLeft : Qt::AlignCenter), text);
            p.setPen(QColor{215, 215, 215});
            p.drawRect(cell);
            x += w;
        };
        box(labelW, data.items[items[n]].label, QColor{}, true);
        for (size_t d = 0; d < detailW.size(); d++) {
            box(detailW[d], static_cast<int>(d) < data.items[items[n]].details.size() ? data.items[items[n]].details[static_cast<int>(d)] : QString{}, QColor{}, true);
        }
        for (size_t c = 0; c < fields.size(); c++) {
            box(cellW[c], texts[n][static_cast<int>(c)], data.tint ? data.tint(items[n], fields[c]) : QColor{}, false);
        }
        y += rowH;
    }
    p.setFont(small);
    p.setPen(QColor{110, 110, 110});
    p.drawText(QRect{pad, image.height() - 30, image.width() - 2 * pad, 22}, Qt::AlignVCenter | Qt::AlignLeft, data.credit + ", made with wxqt-grib");
    return image;
}

void TableExportDialog::saveImage() {
    const auto image = draw();
    if (image.isNull()) {
        return;
    }
    const auto target = ChartExport::askSave(this, "Save the table as a picture", data.windowTitle.toLower(), "png", "PNG images (*.png)");
    if (target.isEmpty()) {
        return;
    }
    status->setText(image.save(target) ? "Saved " + target : "The picture could not be written.");
}

void TableExportDialog::saveCsv() {
    const auto text = build(true);
    if (text.isEmpty()) {
        return;
    }
    const auto target = ChartExport::askSave(this, "Save the table", data.windowTitle.toLower(), "csv", "CSV files (*.csv)");
    if (target.isEmpty()) {
        return;
    }
    QFile file{target};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export", "The file could not be written.");
        return;
    }
    QTextStream out{&file};
    out << text;
    status->setText("Saved " + target);
}

void TableExportDialog::copyText() {
    const auto text = build(false);
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        status->setText("Copied: paste it into a spreadsheet.");
    }
}
