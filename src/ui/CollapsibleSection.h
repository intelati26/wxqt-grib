// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef COLLAPSIBLESECTION_H
#define COLLAPSIBLESECTION_H

#include <string>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

// A titled part of a side panel that folds away: a bar with an arrow and the title, the contents under it. Whether it is open is kept (the setting named by `prefKey`) for the next time.
class CollapsibleSection : public QWidget {
public:
    CollapsibleSection(const QString& title, const std::string& prefKey, bool openByDefault, QWidget * parent);
    QVBoxLayout * body() const { return content; }   // add the widgets of the section here
    void setOpen(bool open);

private:
    QToolButton * bar{};
    QWidget * holder{};
    QVBoxLayout * content{};
    QString title;
    std::string prefKey;
};

#endif  // COLLAPSIBLESECTION_H
