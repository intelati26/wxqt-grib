// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/FloaterViewer.h"
#include <algorithm>
#include <QDateTime>
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "util/Utility.h"

namespace {
    struct Product {
        const char * folder;
        const char * label;
    };
    const Product products[] = {
        {"GEOCOLOR", "GeoColor (true colour by day, infrared at night)"},
        {"Sandwich", "Sandwich (visible with enhanced infrared)"},
        {"AirMass", "Air mass (RGB)"},
        {"DayConvection", "Day convection (RGB)"},
        {"DayNightCloudMicroCombo", "Day / night cloud microphysics (RGB)"},
        {"13", "Band 13 - clean infrared, 10.3 um"},
        {"02", "Band 02 - red visible, 0.64 um"},
        {"07", "Band 07 - shortwave infrared, 3.9 um"},
        {"08", "Band 08 - upper-level water vapour, 6.2 um"},
        {"09", "Band 09 - mid-level water vapour, 6.9 um"},
        {"10", "Band 10 - lower-level water vapour, 7.3 um"},
        {"01", "Band 01 - blue visible, 0.47 um"},
        {"03", "Band 03 - near infrared (vegetation), 0.86 um"},
        {"04", "Band 04 - cirrus, 1.37 um"},
        {"05", "Band 05 - snow / ice, 1.6 um"},
        {"06", "Band 06 - cloud particle size, 2.2 um"},
        {"11", "Band 11 - cloud-top phase, 8.4 um"},
        {"12", "Band 12 - ozone, 9.6 um"},
        {"14", "Band 14 - infrared, 11.2 um"},
        {"15", "Band 15 - dirty infrared, 12.3 um"},
        {"16", "Band 16 - carbon dioxide, 13.3 um"},
    };
    const char * sizes[] = {"500x500", "1000x1000", "latest"};   // latest is the 2000 x 2000 picture
}

FloaterViewer::FloaterViewer(Window * parent, const std::string& id, const std::string& name)
    : Window{parent}
    , comboProduct{this, {}}
    , comboSize{this, {"500 px", "1000 px", "2000 px (full size)"}}
    , buttonBack{this, None, "<"}
    , buttonForward{this, None, ">"}
    , buttonZoom{this, None, "Zoom viewer..."}
    , textStatus{this, ""}
    , stormId{id}
    , title{name}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("GOES floater - " + name);
    std::vector<std::string> labels;
    for (const auto& p : products) {
        labels.push_back(p.label);
    }
    comboProduct.setList(labels);
    comboProduct.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("FLOATER_PRODUCT", 0), 0, static_cast<int>(labels.size()) - 1)));
    comboSize.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("FLOATER_SIZE", 1), 0, 2)));
    comboProduct.connect([this] { Utility::writePrefInt("FLOATER_PRODUCT", comboProduct.getIndex()); load(); });
    comboSize.connect([this] { Utility::writePrefInt("FLOATER_SIZE", comboSize.getIndex()); load(); });
    buttonBack.connect([this] { step(-1); });
    buttonForward.connect([this] { step(1); });
    buttonZoom.connect([this] {
        if (!bytes.isEmpty()) {
            new ImageViewer{this, bytes, title + " - " + products[std::clamp(comboProduct.getIndex(), 0, 20)].label};
        }
    });
    buttonBack.getView()->setFixedWidth(34);
    buttonForward.getView()->setFixedWidth(34);
    row.addWidget(buttonBack);
    row.addWidget(comboProduct);
    row.addWidget(buttonForward);
    row.addWidget(comboSize);
    row.addWidget(buttonZoom);
    row.addStretch();
    scroll = new QScrollArea{this};
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: #101820; }");
    picture = new QLabel{scroll};
    picture->setAlignment(Qt::AlignCenter);
    scroll->setWidget(picture);
    scroll->setWidgetResizable(true);
    box.addLayout(row);
    box.addWidget(textStatus);
    box.addWidgetReal(scroll, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 960);
    timer.setInterval(600000);
    QObject::connect(&timer, &QTimer::timeout, [this] { load(); });
    timer.start();
    load();
}

std::string FloaterViewer::url() const {
    return "https://cdn.star.nesdis.noaa.gov/FLOATER/data/" + stormId + "/" + products[std::clamp(comboProduct.getIndex(), 0, 20)].folder + "/" +
        sizes[std::clamp(comboSize.getIndex(), 0, 2)] + (comboSize.getIndex() == 2 ? ".jpg" : ".jpg");
}

void FloaterViewer::step(int direction) {
    const int count = static_cast<int>(sizeof(products) / sizeof(products[0]));
    const int next = (comboProduct.getIndex() + direction + count) % count;
    comboProduct.setIndex(static_cast<size_t>(next));   // the change signal loads it
}

void FloaterViewer::load() {
    const int mine = ++generation;
    textStatus.setText("Loading " + std::string{products[std::clamp(comboProduct.getIndex(), 0, 20)].label} + "...");
    new FutureBytes{this, url(), [this, mine] (const QByteArray& data) {
        if (closed || mine != generation) {
            return;
        }
        show(data);
    }};
}

void FloaterViewer::show(const QByteArray& data) {
    QPixmap loaded;
    if (data.size() < 500 || !loaded.loadFromData(data)) {
        textStatus.setText(std::string{"This picture is not available for this storm right now (the floater may have moved or ended)."});
        return;
    }
    bytes = data;
    pixmap = loaded;
    rescale();
    textStatus.setText(std::string{products[std::clamp(comboProduct.getIndex(), 0, 20)].label} + "  -  NOAA / NESDIS STAR floater, loaded " +
        QDateTime::currentDateTimeUtc().toString("HH:mm").toStdString() + "Z, refreshed every 10 minutes");
}

void FloaterViewer::rescale() {
    if (pixmap.isNull()) {
        return;
    }
    const QSize room = scroll->viewport()->size() - QSize{4, 4};
    picture->setPixmap(pixmap.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void FloaterViewer::resizeEventCustom() {
    rescale();
}
