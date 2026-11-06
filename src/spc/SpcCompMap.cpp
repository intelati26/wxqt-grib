// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcCompMap.h"
#include <algorithm>
#include <QBuffer>
#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "spc/UtilitySpcCompmap.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SpcCompMap::SpcCompMap(Window * parent)
    : Window{parent}
    , nav{this, OutlookNav::CompositeMap}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
{
    setTitle("SPC Compmap");
    // the layers ticked, in the order ticked (the infrared satellite when there are none kept)
    for (const auto& id : Utility::readPref(prefToken, "16") == "-" ? std::vector<string>{} : [this] {
             std::vector<string> out;
             const auto text = Utility::readPref(prefToken, "16");
             for (size_t at = 0; at < text.size();) {
                 auto end = text.find(',', at);
                 end = end == string::npos ? text.size() : end;
                 auto id = text.substr(at, end - at);
                 id.erase(0, id.find_first_not_of(' '));   // (a list written by hand may have a space after each comma)
                 if (!id.empty()) {
                     out.push_back(id);
                 }
                 at = end + 1;
             }
             return out;
         }()) {
        if (std::find(UtilitySpcCompmap::urlIndices.begin(), UtilitySpcCompmap::urlIndices.end(), id) != UtilitySpcCompmap::urlIndices.end()) {
            ticked.push_back(id);
        }
    }
    boxH.addWidget(nav.combo());
    valid = new QLabel{"", this};
    UiStandards::muted(valid);
    refreshButton = UiStandards::button(this, "Refresh", "Read every layer again");
    boxH.addWidgetReal(refreshButton);
    boxH.addWidgetReal(valid);
    box.addLayout(boxH);
    // the list of layers beside the picture, as the page has it: tick a layer to draw it over the others
    auto * body = new QWidget{this};
    auto * bodyLayout = new QHBoxLayout{body};
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    auto * list = new QWidget{body};
    auto * listLayout = new QVBoxLayout{list};
    listLayout->setContentsMargins(6, 6, 6, 6);
    listLayout->setSpacing(2);
    auto * heading = new QLabel{"Layers (the last one ticked is on top)", list};
    heading->setStyleSheet("font-weight: bold;");
    heading->setWordWrap(true);
    listLayout->addWidget(heading);
    for (size_t i = 0; i < UtilitySpcCompmap::urlIndices.size() && i < UtilitySpcCompmap::labels.size(); i++) {
        const auto id = UtilitySpcCompmap::urlIndices[i];
        auto * check = new QCheckBox{QString::fromStdString(UtilitySpcCompmap::labels[i]), list};
        check->setChecked(std::find(ticked.begin(), ticked.end(), id) != ticked.end());
        check->setToolTip(QString::fromStdString(UtilitySpcCompmap::labels[i]));
        QObject::connect(check, &QCheckBox::toggled, this, [this, id] (bool on) { layerToggled(id, on); });
        boxes[id] = check;
        listLayout->addWidget(check);
    }
    listLayout->addStretch(1);
    auto * scroll = new QScrollArea{body};
    scroll->setWidget(list);
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(330);
    bodyLayout->addWidget(scroll);
    photo.setFlatOnWhite(true);   // (the SPC pictures are made for a white page)
    bodyLayout->addWidget(photo.getView(), 1);
    box.addWidgetReal(body, 1, Qt::Alignment{});
    box.getAndShow(this);
    QObject::connect(refreshButton, &QPushButton::clicked, this, [this] {
        pictures.clear();
        reload();
    });
    reload();
    fetchTimes();
}

void SpcCompMap::saveLayers() const {
    string text;
    for (const auto& id : ticked) {
        text += (text.empty() ? "" : ",") + id;
    }
    Utility::writePref(prefToken, text.empty() ? "-" : text);
}

void SpcCompMap::layerToggled(const string& id, bool on) {
    ticked.erase(std::remove(ticked.begin(), ticked.end(), id), ticked.end());
    if (on) {
        ticked.push_back(id);   // over the others
    }
    saveLayers();
    reload();
}

// every layer that is ticked and not here yet, and the base map
void SpcCompMap::reload() {
    std::vector<string> wanted{"basemap"};
    wanted.insert(wanted.end(), ticked.begin(), ticked.end());
    bool any = false;
    for (const auto& id : wanted) {
        if (pictures.count(id) != 0) {
            continue;
        }
        any = true;
        pictures[id] = QImage{};   // asked for
        const auto url = GlobalVariables::nwsSPCwebsitePrefix + "/exper/compmap/" + id + ".gif";
        new FutureBytes{this, url, [this, id] (const auto& bytes) {
                            if (bytes.isEmpty()) {
                                pictures.erase(id);   // not there (try again at the next Refresh)
                                return;
                            }
                            pictures[id] = QImage::fromData(bytes);
                            compose();
                        }};
    }
    if (!any) {
        compose();
    }
    setTitle("SPC Compmap - " + std::to_string(ticked.size()) + (ticked.size() == 1 ? " layer" : " layers"));
}

// the base map, then the layers in the order ticked, on white
void SpcCompMap::compose() {
    QImage canvas{1024, 768, QImage::Format_RGB32};
    canvas.fill(Qt::white);
    QPainter painter{&canvas};
    std::vector<string> order{"basemap"};
    order.insert(order.end(), ticked.begin(), ticked.end());
    for (const auto& id : order) {
        const auto found = pictures.find(id);
        if (found != pictures.end() && !found->second.isNull()) {
            painter.drawImage(0, 0, found->second);
        }
    }
    painter.end();
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "PNG");
    photo.setBytes(bytes);
}

// "Valid at 21Z on 261106" and the three times of the box under the list
void SpcCompMap::fetchTimes() {
    new FutureBytes{this, GlobalVariables::nwsSPCwebsitePrefix + "/exper/compmap/", [this] (const auto& bytes) {
                        const auto page = QString::fromUtf8(bytes);
                        QStringList parts;
                        static const QRegularExpression at{"Valid at\\s+(\\d\\d)Z on\\s+(\\d{6})"};
                        if (const auto m = at.match(page); m.hasMatch()) {
                            parts << "valid " + m.captured(1) + "Z on " + m.captured(2);
                        }
                        for (const auto& [pattern, label] : {std::pair<const char *, const char *>{"LATEST RAP FCST:\\s*(\\d\\d)Z", "RAP forecast"}, {"12H PCPN VALID:\\s*(\\d\\d)Z", "12 h precipitation"}, {"OBS WX VALID:\\s*(\\d\\d)Z", "observations"}}) {
                            if (const auto m = QRegularExpression{pattern}.match(page); m.hasMatch()) {
                                parts << QString{"%1 %2Z"}.arg(label, m.captured(1));
                            }
                        }
                        valid->setText(parts.join("   "));
                    }};
}
