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
#include "models/SpcPostViewer.h"
#include "spcrefs/SpcRefsViewer.h"
#include "util/Utility.h"

const std::vector<ModelScreen::Source>& ModelScreen::sources() {
    static const std::vector<Source> all{
        {"NCEP", "Models drawn here (GFS, ECMWF, RRFS, REFS, GEFS, NBM, SPC REFS ...)"},
        {"SPCREFS", "SPC REFS store: every product (members, paintballs, swaths)"},
        {"SPCPOST", "SPC Post: calibrated thunder, severe and lightning chances, GEFS-MLP"},
        {"CAMS", "NSSL CAMs pictures (MPAS, WRF, HRRR, RRFS)"},
        {"SPCHRRR", "SPC HRRR pictures (pending discontinuation)"},
        {"NSSLWRF", "NSSL WRF pictures (pending discontinuation)"},
        {"WPCGEFS", "WPC GEFS pictures (pending discontinuation)"},
        {"SPCHREF", "SPC HREF pictures (pending discontinuation)"},
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
    combo = new QComboBox{row};
    for (const auto& s : sources()) {
        combo->addItem(s.label, QString{s.id});
    }
    combo->setToolTip("Every source of model charts is a page of this window: the charts drawn here from the models' own data, SPC's REFS store and post-processed chances, "
                      "and the picture sources of other sites");
    rowLayout->addWidget(combo);
    rowLayout->addStretch();
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
    if (id == "SPCREFS") {
        page = new SpcRefsViewer{this};
    } else if (id == "SPCPOST") {
        page = new SpcPostViewer{this};
    } else if (id == "CAMS") {
        page = new CamsViewer{this};
    } else {
        auto * viewer = new ModelViewer{this, id};
        viewer->openSource = [this] (const string& other) { showSource(other); };   // its "Image models" menu changes the page, not the window
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
}

void ModelScreen::closeEventCustom() {
    for (auto& [id, page] : pages) {   // each page closes as its own window would (stops its timers and loops)
        page->close();
    }
}
