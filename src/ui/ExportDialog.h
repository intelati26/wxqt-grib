// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <functional>
#include <map>
#include <vector>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QRadioButton>
#include <QString>

// The reusable window of "export this": what to save (one of the kinds the screen offers), and the choices that go with it (an area at full color, a lighter ACIS map behind, a size ...), each choice
// enabled only for the kinds it applies to. The screen draws and writes; this window only asks (and the file name is asked after it by ChartExport::askSave). A screen builds one, adds its kinds and
// options, and reads the answer:
//     ExportDialog ask{this, "Export the view"};
//     ask.addKind("view", "The view as it is", "PNG or PDF of what is on the screen");
//     ask.addOption("area", "Area at full color", {{"", "None"}, {"CWA:SGF", "SGF Springfield"}}, "", {"view"});
//     if (ask.exec() == QDialog::Accepted) { ask.kind(); ask.value("area"); }
class ExportDialog : public QDialog {
public:
    ExportDialog(QWidget * parent, const QString& title, const QString& intro = {});
    void addKind(const QString& id, const QString& label, const QString& tip = {});
    // a choice from a list (id and label of each item); `onlyFor`: the kinds it applies to (empty: all)
    void addOption(const QString& key, const QString& label, const std::vector<std::pair<QString, QString>>& items, const QString& current, const std::vector<QString>& onlyFor = {});
    QString kind() const;                         // the id of the kind chosen
    QString value(const QString& key) const;      // the id of the item chosen for an option ("" when the option does not apply to the kind)
    void selectKind(const QString& id);

private:
    void applyKind();
    struct Option {
        QString key;
        QComboBox * box;
        QLabel * label;
        std::vector<QString> onlyFor;
    };
    struct Kind {
        QString id;
        QRadioButton * radio;
    };
    QLayout * kindsLayout{};
    QLayout * optionsLayout{};
    std::vector<Kind> kinds;
    std::vector<Option> options;
};

#endif  // EXPORTDIALOG_H
