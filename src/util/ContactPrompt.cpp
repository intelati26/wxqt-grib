// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/ContactPrompt.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include "util/Utility.h"

namespace ContactPrompt {
    bool valid(const QString& email) {
        static const QRegularExpression pattern{R"(^[^@\s]+@[^@\s]+\.[^@\s]{2,}$)"};
        return pattern.match(email.trimmed()).hasMatch();
    }

    bool askIfMissing(QWidget * parent) {
        if (!Utility::readPref("CONTACT_EMAIL", "").empty()) {
            return false;
        }
        QDialog dialog{parent};
        dialog.setWindowTitle("Contact email");
        dialog.setMinimumWidth(560);
        auto * layout = new QVBoxLayout{&dialog};
        auto * text = new QLabel{
            "The National Weather Service and NOAA ask programs that download their data to identify themselves, so they can reach "
            "the person behind a program that is misbehaving instead of blocking it.\n\n"
            "Please enter an email address. It is added to the User-Agent of the requests this program sends to weather servers "
            "(for example \"wxqt you@example.com\") and is not used for anything else. You can change it later in Settings > General.\n\n"
            "This question appears at every start until an address is saved."};
        text->setWordWrap(true);
        layout->addWidget(text);
        auto * entry = new QLineEdit{&dialog};
        entry->setPlaceholderText("you@example.com");
        layout->addWidget(entry);
        auto * problem = new QLabel{&dialog};
        problem->setStyleSheet("QLabel { color: #b00020; }");
        layout->addWidget(problem);
        auto * buttons = new QDialogButtonBox{&dialog};
        auto * save = buttons->addButton("Save", QDialogButtonBox::AcceptRole);
        buttons->addButton("Not now", QDialogButtonBox::RejectRole);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(save, &QPushButton::clicked, &dialog, [&dialog, entry, problem] {
            if (valid(entry->text())) {
                dialog.accept();
            } else {
                problem->setText("That does not look like an email address (name@host.domain).");
            }
        });
        QObject::connect(entry, &QLineEdit::returnPressed, save, &QPushButton::click);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        Utility::writePref("CONTACT_EMAIL", entry->text().trimmed().toStdString());
        return true;
    }
}
