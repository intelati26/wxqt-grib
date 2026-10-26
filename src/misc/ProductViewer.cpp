// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/ProductViewer.h"
#include <memory>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QWidget>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"
#include "util/UtilityIO.h"
#include "util/UtilityTextReflow.h"

namespace {
    // the preformatted text of a page of HTML (the SPC's watch pages hold the product in <pre>), as plain text
    QString preformatted(const QString& html) {
        QString out;
        for (auto m = QRegularExpression{"<pre[^>]*>(.*?)</pre>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption}.globalMatch(html); m.hasNext();) {
            auto part = m.next().captured(1);
            part.remove(QRegularExpression{"<[^>]*>"});
            part.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"").replace("&nbsp;", " ");
            out += part.trimmed() + "\n\n";
        }
        return out.trimmed();
    }
}

ProductViewer::ProductViewer(Window * parent, const string& title, const string& imageUrl, const string& text, const string& textUrl)
    : Window{parent}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(title);
    auto * row = new QWidget{this};
    auto * layout = new QHBoxLayout{row};
    layout->setContentsMargins(0, 0, 0, 0);
    body = new QPlainTextEdit{row};
    body->setReadOnly(true);
    body->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    body->setMinimumWidth(380);
    UtilityTextReflow::style(body);
    layout->addWidget(body, 2);
    layout->addWidget(&image, 3);
    box.addWidgetReal(row, 1, Qt::Alignment{});
    note = new QLabel{imageUrl.empty() ? "" : "The picture published with the product.", this};
    UiStandards::muted(note);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1250, 800);
    if (!text.empty()) {
        UtilityTextReflow::setProduct(body, QString::fromStdString(text));
    } else if (!textUrl.empty()) {
        body->setPlainText("Reading the text...");
        auto read = std::make_shared<QString>();
        new FutureVoid{this, [read, textUrl] {
            const auto raw = QString::fromUtf8(UtilityIO::downloadAsByteArray(textUrl));
            *read = raw.contains("<pre", Qt::CaseInsensitive) ? preformatted(raw) : raw.trimmed();
        }, [this, read] {
            if (!closed) {
                UtilityTextReflow::setProduct(body, read->isEmpty() ? QString{"The text could not be read."} : *read);
            }
        }};
    }
    if (!imageUrl.empty()) {
        new FutureBytes{this, imageUrl, [this] (const QByteArray& bytes) {
            if (!closed && !bytes.isEmpty()) {
                image.setBytes(bytes);
            }
        }};
    }
}
