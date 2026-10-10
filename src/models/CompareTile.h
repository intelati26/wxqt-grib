// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef COMPARETILE_H
#define COMPARETILE_H

#include <memory>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "models/ChartHover.h"
#include "ui/ZoomImage.h"

// One extra tile of the model screen's comparison: a caption saying what it shows, a button to change that, and the chart (zoomable, with the hover read-out). The model screen
// draws into it and keeps its zoom and place equal to the other tiles'.
class CompareTile : public QFrame {
public:
    explicit CompareTile(QWidget * parent) : QFrame{parent}, caption{new QLabel{this}}, change{new QPushButton{"Change...", this}}, image{new ZoomImage{nullptr}} {
        setFrameShape(QFrame::StyledPanel);
        auto * column = new QVBoxLayout{this};
        column->setContentsMargins(2, 2, 2, 2);
        column->setSpacing(2);
        auto * head = new QHBoxLayout;
        caption->setStyleSheet("font-weight: bold;");
        head->addWidget(caption, 1);
        head->addWidget(change);
        column->addLayout(head);
        image->setParent(this);
        image->setCrosshairMode(true);
        column->addWidget(image, 1);
        hover = std::make_unique<ChartHover>(image);
        message = new QLabel{image};
        message->setWordWrap(true);
        message->setAlignment(Qt::AlignCenter);
        message->setStyleSheet("QLabel { color: #d0d0d0; background: transparent; }");
        message->hide();
    }
    // a drawn chart: a new picture of the same chart keeps the zoom
    void setChart(const QByteArray& bytes, std::shared_ptr<GfsChart::Probe> newProbe, const std::string& chartKey) {
        probe = newProbe;
        message->hide();
        if (chartKey == shownChart && image->hasImage()) {
            image->setBytesKeepView(bytes);
        } else {
            image->setBytes(bytes);
        }
        shownChart = chartKey;
        shownBytes = bytes;
        hover->set(newProbe);
    }
    // nothing to show (the chart is not in that model, or the run has no such hour): the reason, over the picture that was
    void setMessage(const QString& text) {
        message->setText(text);
        message->setGeometry(image->rect());
        message->show();
        message->raise();
        hover->set(nullptr);
    }
    void resizeEvent(QResizeEvent * event) override {
        QFrame::resizeEvent(event);
        message->setGeometry(image->rect());
    }
    QLabel * caption;
    QPushButton * change;
    ZoomImage * image;
    std::unique_ptr<ChartHover> hover;
    std::shared_ptr<GfsChart::Probe> probe;
    QByteArray shownBytes;
    std::string shownChart;

private:
    QLabel * message;
};

#endif  // COMPARETILE_H
