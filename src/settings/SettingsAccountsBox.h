// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSACCOUNTSBOX_H
#define SETTINGSACCOUNTSBOX_H

#include <QLabel>
#include <QLineEdit>
#include <QWidget>

// Settings > Accounts: logins of the person's own that some data needs. Now the NASA Earthdata Login token (PO.DAAC's ASCAT winds), kept in the program's settings file, which stays through
// upgrades, and sent only to NASA's own hosts.
class SettingsAccountsBox : public QWidget {
public:
    explicit SettingsAccountsBox(QWidget * parent = nullptr);

private:
    void save();
    void test();
    QLineEdit * token{};
    QLabel * status{};
};

#endif  // SETTINGSACCOUNTSBOX_H
