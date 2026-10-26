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
#include "util/UtilityTextReflow.h"
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
    UtilityTextReflow::style(text);
    rowLayout->addWidget(text, 2);
    rowLayout->addWidget(&image, 3);
    box.addWidgetReal(row, 1, Qt::Alignment{});
    auto * credit = new QLabel{"NOAA / NWS National Water Center, an experimental product: a short range, episodic discussion and graphic of where rapid-onset flooding may be a threat, from the National Water Model and other "
                               "guidance. It is issued when there is a threat. The text comes from the NWS products service, which keeps the last few discussions; the graphics go back further.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1200, 780);
    QObject::connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl{QString::fromStdString(site)}); });
    QObject::connect(times, &QComboBox::currentIndexChanged, this, [this] (int index) { showImage(index); });
    auto list = std::make_shared<vector<std::pair<string, QString>>>();   // file, label
    auto stamps = std::make_shared<vector<long>>();                         // the time of each graphic
    auto products = std::make_shared<vector<std::pair<long, string>>>();    // the NWS's text products (api.weather.gov keeps the last few): issued, text
    new FutureVoid{this, [list, stamps, products] {
        const auto ahd = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(indexUrl)).object().value("ahd").toObject();
        const auto latest = timeOf(ahd.value("latest-date").toString());
        list->push_back({ahd.value("latest").toString("AHD.png").toStdString(), "Newest" + (latest.isValid() ? "  -  " + latest.toString("ddd d MMM HH:mm") + "Z" : QString{})});
        stamps->push_back(latest.isValid() ? static_cast<long>(latest.toSecsSinceEpoch()) : 0L);
        const auto archive = ahd.value("archive").toArray();
        for (int i = archive.size() - 1; i >= 0 && list->size() < 60; i--) {
            const auto name = archive.at(i).toString();
            // "AHD_2026-10-22T16:03:26.png"
            const auto t = timeOf(name.mid(4, 19));
            list->push_back({"Archive/" + name.toStdString(), t.isValid() ? t.toString("ddd d MMM yyyy HH:mm") + "Z" : name});
            stamps->push_back(t.isValid() ? static_cast<long>(t.toSecsSinceEpoch()) : 0L);
        }
        for (const auto& item : QJsonDocument::fromJson(UtilityIO::downloadAsByteArray("https://api.weather.gov/products/types/AHD")).object().value("@graph").toArray()) {
            const auto id = item.toObject().value("id").toString();
            const auto one = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray("https://api.weather.gov/products/" + id.toStdString())).object();
            const auto issued = timeOf(one.value("issuanceTime").toString());
            if (issued.isValid() && !one.value("productText").toString().isEmpty()) {
                products->push_back({static_cast<long>(issued.toSecsSinceEpoch()), one.value("productText").toString().trimmed().toStdString()});
            }
        }
    }, [this, list, stamps, products] {
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
        stampList = *stamps;
        history = *products;
        showImage(0);
    }};
}

void AhdViewer::showImage(int index) {
    if (index < 0 || static_cast<size_t>(index) >= files.size()) {
        return;
    }
    // the text of the discussion issued nearest the graphic (within three hours); the NWS keeps only the last few
    const long stamp = static_cast<size_t>(index) < stampList.size() ? stampList[static_cast<size_t>(index)] : 0L;
    const std::pair<long, string> * nearest = nullptr;
    for (const auto& item : history) {
        if (stamp != 0 && std::labs(item.first - stamp) < 3 * 3600 && (nearest == nullptr || std::labs(item.first - stamp) < std::labs(nearest->first - stamp))) {
            nearest = &item;
        }
    }
    if (nearest != nullptr) {
        UtilityTextReflow::setProduct(text, QString::fromStdString(nearest->second));
    } else {
        text->setPlainText("The NWS keeps only its last " + QString::number(history.size()) + " discussions as text; this graphic's is no longer kept. The graphic is the whole product from before.");
    }
    note->setText(QString::number(files.size()) + " graphics (back to June 2022); the text is that of the discussion issued with the graphic, for the last " + QString::number(history.size()) + " discussions.");
    new FutureBytes{this, imagesFolder + files[static_cast<size_t>(index)], [this] (const QByteArray& bytes) {
        if (!closed && !bytes.isEmpty()) {
            image.setBytes(bytes);
        }
    }};
}
