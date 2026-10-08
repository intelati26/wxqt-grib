// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FLOATERVIEWER_H
#define FLOATERVIEWER_H

#include <string>
#include <QByteArray>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QTimer>
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// A GOES floater, the satellite view centred on a storm (NOAA / NESDIS STAR, cdn.star.nesdis.noaa.gov/FLOATER): GeoColor to start with, and every other field
// the ABI makes - the Sandwich, air mass, day convection, the cloud microphysics combination and the sixteen bands (visible, near infrared, water vapour,
// infrared ...) - one choice away, with "<" and ">" to work through them. Refreshes every ten minutes.
class FloaterViewer : public Window {
public:
    FloaterViewer(Window * parent, const std::string& stormId, const std::string& title);

private:
    void load();
    void show(const QByteArray& bytes);
    void step(int direction);
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    void rescale();
    std::string url() const;
    VBox box;
    HBox row;
    ComboBox comboProduct;
    ComboBox comboSize;
    Button buttonBack;
    Button buttonForward;
    Button buttonZoom;
    Text textStatus;
    QScrollArea * scroll{};
    QLabel * picture{};
    QPixmap pixmap;
    QByteArray bytes;
    QTimer timer;
    std::string stormId;
    std::string title;
    int generation{0};
    bool closed{false};
};

#endif  // FLOATERVIEWER_H
