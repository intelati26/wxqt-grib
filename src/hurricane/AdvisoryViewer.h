// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ADVISORYVIEWER_H
#define ADVISORYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QPlainTextEdit>
#include <QTabWidget>
#include "hurricane/HurricaneData.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// NHC's text products for a storm, read in the program: the public advisory, the forecast discussion, the forecast / advisory (the numbers and wind
// radii) and the wind speed probabilities, a tab each, with the advisory's headline above them.
class AdvisoryViewer : public Window {
public:
    AdvisoryViewer(Window * parent, const HurricaneData::StormEntry& entry);
    // any set of NHC text products: a window title and (tab title, page address) pairs
    AdvisoryViewer(Window * parent, const std::string& title, const std::vector<std::pair<std::string, std::string>>& products);

private:
    void build(const std::string& title, const std::vector<std::pair<std::string, std::string>>& products);
    VBox box;
    Text textHeadline;
    QTabWidget * tabs{};
    std::vector<QPlainTextEdit *> pages;
    bool closed{false};
    void closeEventCustom() override { closed = true; }
};

#endif  // ADVISORYVIEWER_H
