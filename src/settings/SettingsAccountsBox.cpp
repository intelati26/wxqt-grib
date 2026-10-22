// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsAccountsBox.h"
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "ui/FlowBox.h"
#include "util/Earthdata.h"
#include "ui/UiStandards.h"

SettingsAccountsBox::SettingsAccountsBox(QWidget * parent) : QWidget{parent} {
    auto * column = new QVBoxLayout{this};
    auto * title = new QLabel{"<b>NASA Earthdata Login token</b>", this};
    auto * about = new QLabel{"Some data is only given to a logged-in request: PO.DAAC's ASCAT ocean winds, for one. The account is free (urs.earthdata.nasa.gov); on its page, "
                              "<i>Generate Token</i> makes a token, which lasts about 60 days. Paste it here and it is kept in this program's settings file, which stays through upgrades, and "
                              "sent only to NASA's Earthdata addresses (never to any other site). When it expires, paste the new one.", this};
    about->setWordWrap(true);
    token = new QLineEdit{this};
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("the token (a long line of letters, digits and dots)");
    token->setText(QString::fromStdString(Earthdata::token()));
    token->setMinimumWidth(320);
    auto * show = new QCheckBox{"Show", this};
    auto * saveButton = UiStandards::button(this, "Save");
    auto * testButton = UiStandards::button(this, "Save and test");
    auto * clearButton = UiStandards::button(this, "Remove");
    auto * openButton = UiStandards::button(this, "Earthdata login...", "Open the Earthdata Login page in the browser");
    status = new QLabel{this};
    status->setWordWrap(true);
    auto * row = new FlowLayout{6};
    row->addWidget(token);
    row->addWidget(show);
    row->addWidget(saveButton);
    row->addWidget(testButton);
    row->addWidget(clearButton);
    row->addWidget(openButton);
    column->addWidget(title);
    column->addWidget(about);
    column->addLayout(row);
    column->addWidget(status);
    column->addStretch();
    status->setText(Earthdata::configured() ? "A token is saved." : "No token is saved.");
    QObject::connect(show, &QCheckBox::toggled, this, [this] (bool on) { token->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    QObject::connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    QObject::connect(testButton, &QPushButton::clicked, this, [this] { test(); });
    QObject::connect(token, &QLineEdit::editingFinished, this, [this] { save(); });
    QObject::connect(clearButton, &QPushButton::clicked, this, [this] {
        token->clear();
        Earthdata::setToken({});
        status->setText("The token was removed.");
    });
    QObject::connect(openButton, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl{"https://urs.earthdata.nasa.gov/"}); });
}

void SettingsAccountsBox::save() {
    Earthdata::setToken(token->text().trimmed().toStdString());
    status->setText(Earthdata::configured() ? "Saved." : "No token is saved.");
}

// the test asks NASA for a few bytes of the newest ASCAT file with the token (in the background: it is a network call)
void SettingsAccountsBox::test() {
    save();
    status->setText("Testing with NASA...");
    auto answer = std::make_shared<std::string>();
    auto * window = dynamic_cast<Window *>(this->window());
    if (window == nullptr) {
        *answer = Earthdata::test();
        status->setText(answer->empty() ? "NASA accepted the token." : QString::fromStdString(*answer));
        return;
    }
    new FutureVoid{window, [answer] { *answer = Earthdata::test(); }, [this, answer] { status->setText(answer->empty() ? "NASA accepted the token." : QString::fromStdString(*answer)); }};
}
