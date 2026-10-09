// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PRODUCTPICKER_H
#define PRODUCTPICKER_H

#include <functional>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include "gfs/GfsChart.h"

// The chart picker of the models drawn from GRIB: their charts number in the dozens, so they are listed in groups (upper air, surface, precipitation, storms ...) with a search box,
// favorites kept at the top, and the lines and wind barbs that can be ticked onto the chart as well ("this and that"). Choosing a chart (double click, or Enter) calls onPick; ticking an
// extra calls onOverlays at once; starring calls onFavorites.
class ProductPicker : public QDialog {
public:
    struct Entry {
        std::string id;
        std::string label;
        std::string group;
    };
    ProductPicker(QWidget * parent, const std::string& model, const std::vector<Entry>& entries, const std::string& current, const std::vector<std::string>& favorites,
                  const std::vector<GfsChart::OverlayChoice>& extras, const std::vector<std::string>& ticked);
    std::function<void(const std::string&)> onPick;
    std::function<void(const std::vector<std::string>&)> onOverlays;
    std::function<void(const std::vector<std::string>&)> onFavorites;
    // the group names in the order they are listed
    static std::vector<std::string> groupOrder();

private:
    void fill();
    void filter(const QString& text);
    void pickCurrent();
    void toggleFavorite();
    void ticked();
    void updateStar();
    std::string selectedId() const;
    std::vector<Entry> entries;
    std::vector<std::string> favorites;
    std::vector<GfsChart::OverlayChoice> extras;
    std::string current;
    QLineEdit * search{};
    QTreeWidget * tree{};
    QPushButton * star{};
    std::vector<QCheckBox *> boxes;
};

#endif  // PRODUCTPICKER_H
