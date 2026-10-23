// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/AhdViewer.h"
#include <memory>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTimeZone>
#include <QUrl>
#include <QWidget>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"
#include "util/UtilityIO.h"

namespace {
    const string imagesFolder{"https://www.weather.gov/images/owp/AHD/"};
    const string indexUrl{"https://www.weather.gov/source/owp/AHD/wpod_products.json"};

    QDateTime timeOf(const QString& iso) {
        auto t = QDateTime::fromString(iso, Qt::ISODate);
        t.setTimeZone(QTimeZone::utc());
        return t;
    }
}

const string AhdViewer::site{"https://water.noaa.gov/operations/ahd"};

bool AhdViewer::recent(int hours, string& imageUrl) {
    const auto doc = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(indexUrl));
    const auto ahd = doc.object().value("ahd").toObject();
    const auto latest = timeOf(ahd.value("latest-date").toString());
    if (!latest.isValid() || latest.secsTo(QDateTime::currentDateTimeUtc()) > hours * 3600LL) {
        return false;
    }
    imageUrl = imagesFolder + ahd.value("latest").toString("AHD.png").toStdString();
    return true;
}

AhdViewer::AhdViewer(Window * parent)
    : Window{parent}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Area hydrologic discussion - experimental");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    times = UiStandards::combo(top);
    auto * open = UiStandards::button(top, "Site...", "water.noaa.gov's page of the Area Hydrologic Discussion (National Water Center)");
    topLayout->addWidget(times, 1);
    topLayout->addWidget(open);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    note = new QLabel{"Reading the discussion...", this};
    note->setWordWrap(true);
    UiStandards::muted(note);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    auto * row = new QWidget{this};
    auto * rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    text = new QPlainTextEdit{row};
    text->setReadOnly(true);
    {
        QFont mono{"monospace"};
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSizeF(9.5);
        text->setFont(mono);
    }
    text->setMinimumWidth(420);
    rowLayout->addWidget(text, 2);
    rowLayout->addWidget(&image, 3);
    box.addWidgetReal(row, 1, Qt::Alignment{});
    auto * credit = new QLabel{"NOAA / NWS National Water Center, an experimental product: a short range, episodic discussion and graphic of where rapid-onset flooding may be a threat, from the National Water Model and other "
                               "guidance. It is issued when there is a threat. The text is the newest one; the archive has the graphics only.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1200, 780);
    QObject::connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl{QString::fromStdString(site)}); });
    QObject::connect(times, &QComboBox::currentIndexChanged, this, [this] (int index) { showImage(index); });
    auto list = std::make_shared<vector<std::pair<string, QString>>>();   // file, label
    auto body = std::make_shared<string>();
    new FutureVoid{this, [list, body] {
        const auto ahd = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(indexUrl)).object().value("ahd").toObject();
        const auto latest = timeOf(ahd.value("latest-date").toString());
        list->push_back({ahd.value("latest").toString("AHD.png").toStdString(), "Newest" + (latest.isValid() ? "  -  " + latest.toString("ddd d MMM HH:mm") + "Z" : QString{})});
        const auto archive = ahd.value("archive").toArray();
        for (int i = archive.size() - 1; i >= 0 && list->size() < 60; i--) {
            const auto name = archive.at(i).toString();
            // "AHD_2026-10-22T16:03:26.png"
            const auto t = timeOf(name.mid(4, 19));
            list->push_back({"Archive/" + name.toStdString(), t.isValid() ? t.toString("ddd d MMM yyyy HH:mm") + "Z" : name});
        }
        const auto products = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray("https://api.weather.gov/products/types/AHD")).object().value("@graph").toArray();
        if (!products.isEmpty()) {
            int newest = 0;   // (the list is not always newest first)
            for (int i = 1; i < products.size(); i++) {
                if (timeOf(products.at(i).toObject().value("issuanceTime").toString()) > timeOf(products.at(newest).toObject().value("issuanceTime").toString())) {
                    newest = i;
                }
            }
            const auto one = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray("https://api.weather.gov/products/" + products.at(newest).toObject().value("id").toString().toStdString())).object();
            *body = one.value("productText").toString().trimmed().toStdString();
        }
    }, [this, list, body] {
        if (closed) {
            return;
        }
        if (list->empty()) {
            note->setText("The discussion could not be read.");
            return;
        }
        QSignalBlocker block{times};
        for (const auto& [file, label] : *list) {
            files.push_back(file);
            times->addItem(label);
        }
        text->setPlainText(body->empty() ? QString{"No text found."} : QString::fromStdString(*body));
        note->setText(QString::number(files.size()) + " graphics; the text is the newest discussion.");
        showImage(0);
    }};
}

void AhdViewer::showImage(int index) {
    if (index < 0 || static_cast<size_t>(index) >= files.size()) {
        return;
    }
    new FutureBytes{this, imagesFolder + files[static_cast<size_t>(index)], [this] (const QByteArray& bytes) {
        if (!closed && !bytes.isEmpty()) {
            image.setBytes(bytes);
        }
    }};
}
