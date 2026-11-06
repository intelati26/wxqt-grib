// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcTstormOutlooks.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "spc/UtilitySpc.h"
#include <algorithm>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "util/UtilityList.h"

SpcTstormOutlooks::SpcTstormOutlooks(Window * parent)
    : Window{parent}
{
    setTitle("SPC Thunderstorm Outlooks");
    const auto urls = UtilitySpc::getTstormOutlookUrls();
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
    }
    auto * row = new QHBoxLayout;
    row->setSpacing(4);
    for (auto index : range(urls.size())) {
        auto * header = new QLabel{"...", this};
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        headers.push_back(header);
        auto * cell = new QVBoxLayout;
        cell->setSpacing(0);
        cell->addWidget(header);
        cell->addWidget(images[index].getView());
        row->addLayout(cell);
    }
    box.addLayoutReal(row);
    box.addStretch();
    box.getAndShow(this);
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new ImageViewer{this, images[index].bytes}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); images[index].resizeToWidth(static_cast<float>(width()) / static_cast<float>(images.size()) - 10); }};
        // the highest probability of the period, from the outlook's KML
        const auto kml = urls[index].substr(0, urls[index].size() - 4) + ".kml";
        new FutureBytes{this, kml, [this, index] (const auto& data) {
            const auto period = UtilitySpc::parseTstormKml(data.toStdString());
            headers[index]->setText(QString::fromStdString((period.valid.empty() ? string{} : period.valid + ": ") + "<b>" + (period.percent == 0 ? string{"No areas"} : std::to_string(period.percent) + "%") + "</b>"));
            headers[index]->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + (period.color.empty() ? string{"#F0F0F0"} : period.color) + ";"));
        }};
    }
}

void SpcTstormOutlooks::resizeEventCustom() {
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width()) / static_cast<float>(std::max<size_t>(images.size(), 1)) - 10);
    }
}
