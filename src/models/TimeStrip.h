// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TIMESTRIP_H
#define TIMESTRIP_H

#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

// The time controls of a forecast screen, as the model sites have them: a timeline of the forecast hours with a handle, buttons for first / back / play / forward / last, a loop speed,
// and a button that opens a grid of every hour to jump to. It only reports what was asked (the screen draws the frames and runs the play timer, as it knows what is ready).
class TimeStrip : public QWidget {
public:
    explicit TimeStrip(QWidget * parent) : QWidget{parent} {
        auto * column = new QVBoxLayout{this};
        column->setContentsMargins(26, 0, 26, 0);
        column->setSpacing(2);
        ticks = new Ticks{this};
        column->addWidget(ticks);
        slider = new QSlider{Qt::Horizontal, this};
        slider->setTracking(true);
        column->addWidget(slider);
        auto * row = new QHBoxLayout;
        row->addStretch();
        const auto button = [this, row] (const QString& text, const QString& tip, std::function<void()> action) {
            auto * b = new QPushButton{text, this};
            b->setToolTip(tip);
            b->setFixedWidth(46);
            QObject::connect(b, &QPushButton::clicked, this, [action] { action(); });
            row->addWidget(b);
            return b;
        };
        button("|<", "First hour", [this] { choose(0); });
        button("<", "Back an hour", [this] { choose(slider->value() - 1); });
        play = button(QString::fromUtf8("▶"), "Play (loops)", [this] {
            playing = !playing;
            play->setText(QString::fromUtf8(playing ? "❚❚" : "▶"));
            if (onPlay) onPlay(playing);
        });
        button(">", "Forward an hour", [this] { choose(slider->value() + 1); });
        button(">|", "Last hour", [this] { choose(slider->maximum()); });
        row->addSpacing(16);
        status = new QLabel{this};
        row->addWidget(status);
        row->addSpacing(16);
        hours = new QPushButton{"Hours \xE2\x96\xBE", this};
        QObject::connect(hours, &QPushButton::clicked, this, [this] { openGrid(); });
        row->addWidget(hours);
        row->addSpacing(16);
        row->addWidget(new QLabel{"Loop speed:", this});
        speed = new QSlider{Qt::Horizontal, this};
        speed->setRange(1, 10);
        speed->setValue(5);
        speed->setFixedWidth(110);
        row->addWidget(speed);
        row->addStretch();
        column->addLayout(row);
        QObject::connect(slider, &QSlider::valueChanged, this, [this] (int v) {
            if (!quiet && onSelect) onSelect(v);
            ticks->update();
        });
    }
    // the forecast hours ("003", "006" ...) and the one shown
    void setTimes(const std::vector<std::string>& labels, int current) {
        quiet = true;
        names = labels;
        slider->setRange(0, std::max<int>(0, static_cast<int>(labels.size()) - 1));
        slider->setValue(std::clamp(current, 0, slider->maximum()));
        quiet = false;
        ticks->names = labels;
        ticks->update();
    }
    void setCurrent(int index) {
        quiet = true;
        slider->setValue(std::clamp(index, 0, slider->maximum()));
        quiet = false;
        ticks->update();
    }
    // the hours that are drawn (a bar under the numbers) and a count beside the buttons: "Ready 37 of 129"
    void setLoaded(const std::vector<bool>& ready, const QString& summary) {
        ticks->loaded = ready;
        ticks->update();
        status->setText(summary);
    }
    int current() const { return slider->value(); }
    int count() const { return static_cast<int>(names.size()); }
    int intervalMs() const { return 1400 - (speed->value() - 1) * 140; }   // loop speed 1 .. 10: 1.4 s down to 0.14 s a frame
    void stop() {
        playing = false;
        play->setText(QString::fromUtf8("▶"));
    }
    std::function<void(int)> onSelect;
    std::function<void(bool)> onPlay;

private:
    // the hour numbers above the slider, as many as fit
    struct Ticks : public QWidget {
        std::vector<std::string> names;
        std::vector<bool> loaded;   // the hours drawn: a bar under the numbers
        explicit Ticks(QWidget * parent) : QWidget{parent} { setFixedHeight(22); }
        void paintEvent(QPaintEvent *) override {
            if (names.size() < 2) {
                return;
            }
            QPainter p{this};
            if (loaded.size() == names.size()) {   // which hours are drawn and ready
                const double left = 7.0, span = width() - 14.0;
                const double slot = span / static_cast<double>(names.size());
                for (size_t i = 0; i < names.size(); i++) {
                    p.fillRect(QRectF{left + slot * static_cast<double>(i), 17.0, std::max(1.0, slot - 0.5), 4.0}, loaded[i] ? QColor{"#3b8fd9"} : QColor{128, 128, 128, 60});
                }
            }
            QFont f = font();
            f.setBold(true);
            f.setPointSizeF(f.pointSizeF() * 0.85);
            p.setFont(f);
            const double margin = 7.0, span = width() - 2 * margin;
            const int labelWidth = p.fontMetrics().horizontalAdvance("000") + 14;
            const int every = std::max(1, static_cast<int>(std::ceil(labelWidth / (span / (names.size() - 1)))));
            for (size_t i = 0; i < names.size(); i += static_cast<size_t>(every)) {
                const double x = margin + span * i / (names.size() - 1);
                p.drawText(QRectF{x - labelWidth / 2.0, 0, static_cast<double>(labelWidth), 16.0}, Qt::AlignCenter, QString::fromStdString(names[i]));
            }
        }
    };
    void choose(int index) {
        index = std::clamp(index, 0, slider->maximum());
        if (index != slider->value()) {
            slider->setValue(index);   // reports through valueChanged
        }
    }
    void openGrid() {
        auto * popup = new QFrame{this, Qt::Popup};
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setFrameShape(QFrame::StyledPanel);
        auto * grid = new QGridLayout{popup};
        grid->setSpacing(3);
        for (size_t i = 0; i < names.size(); i++) {
            auto * b = new QPushButton{QString::fromStdString(names[i]), popup};
            b->setCheckable(true);
            b->setChecked(static_cast<int>(i) == slider->value());
            b->setFixedWidth(46);
            QObject::connect(b, &QPushButton::clicked, popup, [this, popup, i] {
                choose(static_cast<int>(i));
                popup->close();
            });
            grid->addWidget(b, static_cast<int>(i / 8), static_cast<int>(i % 8));
        }
        popup->move(hours->mapToGlobal(QPoint{0, -popup->sizeHint().height()}));
        popup->show();
    }
    Ticks * ticks{};
    QSlider * slider{};
    QSlider * speed{};
    QPushButton * play{};
    QPushButton * hours{};
    QLabel * status{};
    std::vector<std::string> names;
    bool playing{false};
    bool quiet{false};
};

#endif  // TIMESTRIP_H
