// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ModelScreen.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include "models/CamsViewer.h"
#include "models/ModelViewer.h"
#include "mapkit/MasterMapViewer.h"
#include "models/SpcPostViewer.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

const std::vector<ModelScreen::Source>& ModelScreen::sources() {
    static const std::vector<Source> all{
        {"NCEP", "Models drawn here (GFS, ECMWF, RRFS, REFS, GEFS, NBM, SPC REFS ...)"},
        {"NCEPMAG", "NCEP MAG pictures (RAP, NAM, HiResW, WAVEWATCH, NAEFS, polar ...: models not drawn from GRIB)"},
        {"SPCPOST", "SPC Post: calibrated thunder, severe and lightning chances, GEFS-MLP"},
        {"CAMS", "NSSL CAMs pictures (MPAS, WRF, HRRR, RRFS)"},
        {"SPCHRRR", "SPC HRRR pictures (pending discontinuation)"},
        {"NSSLWRF", "NSSL WRF pictures (pending discontinuation)"},
        {"WPCGEFS", "WPC GEFS pictures (pending discontinuation)"},
        {"SPCHREF", "SPC HREF pictures (retired 3 Nov 2026)"},
    };
    return all;
}

ModelScreen::ModelScreen(Window * parent, const string& source)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Models");
    auto * row = new QWidget{this};
    auto * rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(new QLabel{"Source:", row});
    combo = UiStandards::combo(row);
    for (const auto& s : sources()) {
        combo->addItem(s.label, QString{s.id});
    }
    combo->setToolTip("Every source of model charts is a page of this window: the charts drawn here from the models' own data, SPC's REFS store and post-processed chances, "
                      "and the picture sources of other sites");
    rowLayout->addWidget(combo);
    viewBox = UiStandards::combo(row);
    viewBox->addItem("Picture", "picture");
    viewBox->addItem("Map", "map");
    viewBox->setToolTip("The chart as a titled picture, or on the map (scroll and zoom it, put radar, satellite, storms or rivers with it): the same model, chart, area and valid time either way");
    viewBox->setVisible(false);
    rowLayout->addWidget(viewBox);
    rowLayout->addStretch();
    QObject::connect(viewBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int index) {
        if (index == 1 && shown == "NCEP") {
            if (auto * viewer = dynamic_cast<ModelViewer *>(pages["NCEP"])) {
                showMap(viewer->selection());
            }
        } else if (index == 0 && shown == "MAP") {
            if (auto * map = dynamic_cast<MasterMapViewer *>(pages["MAP"]); map != nullptr && !map->modelSelection().model.empty()) {
                applySelection(map->modelSelection());
            } else {
                showSource("NCEP");
            }
        }
    });
    stack = new QStackedWidget{this};
    box.addWidgetReal(row, 0, Qt::Alignment{});
    box.addWidgetReal(stack, 1, Qt::Alignment{});
    box.getAndShow(this);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int index) {
        if (index >= 0) {
            showSource(combo->itemData(index).toString().toStdString());
        }
    });
    showSource(source.empty() ? Utility::readPref("MODEL_SCREEN_SOURCE", "NCEP") : source);
}

Window * ModelScreen::make(const string& id) {
    Window::embedNextWindow();
    Window * page = nullptr;
    if (id == "SPCPOST") {
        page = new SpcPostViewer{this};
    } else if (id == "CAMS") {
        page = new CamsViewer{this};
    } else {
        auto * viewer = new ModelViewer{this, id};
        viewer->openSource = [this] (const string& other) { showSource(other); };   // its "Image models" menu changes the page, not the window
        viewer->openMap = [this] (const ChartSelection& chosen) { showMap(chosen); };
        page = viewer;
    }
    page->setAttribute(Qt::WA_DeleteOnClose, false);   // the page goes with the window
    QObject::connect(page, &QWidget::windowTitleChanged, this, [this, page] (const QString& title) {
        if (stack->currentWidget() == page) {
            setWindowTitle(title);
        }
    });
    stack->addWidget(page);
    return page;
}

void ModelScreen::showSource(const string& requested) {
    string id = requested;
    if (id == "SPCREFS") {   // SPC's REFS store is a model of the Model Viewer (every field of it, members, paintballs): that page, on that model
        if (pages.count("NCEP") == 0) {
            Utility::writePref("NCEP", "SPCREFS");   // the page opens on the model last used
        }
        showSource("NCEP");
        if (auto * viewer = dynamic_cast<ModelViewer *>(pages["NCEP"])) {
            viewer->selectModel("SPCREFS");
        }
        return;
    }
    int index = -1;
    for (size_t i = 0; i < sources().size(); i++) {
        if (id == sources()[i].id) {
            index = static_cast<int>(i);
        }
    }
    if (index < 0) {
        id = "NCEP";
        index = 0;
    }
    if (id == shown) {
        return;
    }
    shown = id;
    Utility::writePref("MODEL_SCREEN_SOURCE", id);
    auto found = pages.find(id);
    Window * page = found != pages.end() ? found->second : (pages[id] = make(id));
    stack->setCurrentWidget(page);
    setWindowTitle(page->windowTitle());
    combo->blockSignals(true);
    combo->setCurrentIndex(index);
    combo->blockSignals(false);
    syncViewBox();
}

void ModelScreen::syncViewBox() {
    viewBox->blockSignals(true);
    viewBox->setCurrentIndex(shown == "MAP" ? 1 : 0);
    viewBox->setVisible(shown == "NCEP" || shown == "MAP");
    viewBox->blockSignals(false);
}

void ModelScreen::closeEventCustom() {
    for (auto& [id, page] : pages) {   // each page closes as its own window would (stops its timers and loops)
        page->close();
    }
}

void ModelScreen::showMap(const ChartSelection& chosen) {
    if (const auto old = pages.find("MAP"); old != pages.end()) {   // made again each time: it opens on the selection it is given
        stack->removeWidget(old->second);
        old->second->close();
        old->second->deleteLater();
        pages.erase(old);
    }
    Window::embedNextWindow();
    auto * map = MasterMapViewer::openModels(this, chosen.model, chosen.param, chosen.sector, chosen.valid, chosen);
    map->setAttribute(Qt::WA_DeleteOnClose, false);
    map->backToPicture = [this] (const ChartSelection& back) { applySelection(back); };
    QObject::connect(map, &QWidget::windowTitleChanged, this, [this, map] (const QString& title) {
        if (stack->currentWidget() == map) {
            setWindowTitle(title);
        }
    });
    stack->addWidget(map);
    pages["MAP"] = map;
    shown = "MAP";
    stack->setCurrentWidget(map);
    setWindowTitle(map->windowTitle());
    syncViewBox();
}

void ModelScreen::applySelection(const ChartSelection& chosen) {
    showSource("NCEP");
    if (auto * viewer = dynamic_cast<ModelViewer *>(pages["NCEP"])) {
        viewer->applySelection(chosen);
    }
}

void ModelScreen::applyVariants(const ChartSelection& chosen) {
    showSource("NCEP");
    if (auto * viewer = dynamic_cast<ModelViewer *>(pages["NCEP"])) {
        viewer->applyVariants(chosen);
    }
}

void ModelScreen::openAt(const string& model, const QDateTime& validUtc) {
    showSource("NCEP");
    if (auto * viewer = dynamic_cast<ModelViewer *>(pages["NCEP"])) {
        viewer->openAt(model, validUtc);
    }
}
