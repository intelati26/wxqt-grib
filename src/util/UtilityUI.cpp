// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "util/UtilityUI.h"
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>  // KEEP
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    #include <QInputDevice>
#else
    #include <QTouchDevice>
#endif

vector<int> UtilityUI::getScreenBounds() {
    const auto screen = QGuiApplication::primaryScreen();
    // int height = screen->availableSize().height();
    // int width = screen->availableSize().width();
    const auto height = screen->availableGeometry().height();
    const auto width = screen->availableGeometry().width();
    // qDebug() << height << " " << width << " " << screen->orientation();
    // 970   1920   Qt::PrimaryOrientation (Mac)
    // 720   360   Qt::PortraitOrientation (PinePhone Pro)
    // 360   720   Qt::LandscapeOrientation (PinePhone Pro)
    if (screen->orientation() == Qt::PortraitOrientation || screen->orientation() == Qt::LandscapeOrientation) {
        // return {height, width};
    }
    return {width, height};
}

bool UtilityUI::isMobile() {
    // at5 only
    // QList<const QTouchDevice *> QTouchDevice::devices()
    // auto touchDevices = QTouchDevice::devices()

    // qDebug() << QInputEvent::device();
    // QInputDevice::DeviceType::Mouse TouchScreen
    // QList<const QInputDevice *> QInputDevice::devices()

    #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        // QList<const QInputDevice *>
        const auto devices = QInputDevice::devices();
        // qDebug() << devices;
        for (auto device : devices) {
            qDebug() << device->type();
        }
        // QInputDevice::DeviceType::TouchScreen
        // QInputDevice::DeviceType QInputDevice::type() const
        // bool QInputDevice::hasCapability(QInputDevice::Capability capability) const
    #else
        QList<const QTouchDevice *> devices = QTouchDevice::devices();
        qDebug() << "touch device count: " << devices.size();
    #endif

    const auto screen = QGuiApplication::primaryScreen();
    // qDebug() << height << " " << width << " " << screen->orientation();
//    qDebug() << screen->orientation();
    // 970   1920   Qt::PrimaryOrientation (Mac)
    // 720   360   Qt::PortraitOrientation (PinePhone Pro)
    // 360   720   Qt::LandscapeOrientation (PinePhone Pro)
    if (screen->orientation() == Qt::PortraitOrientation || screen->orientation() == Qt::LandscapeOrientation) {
        return true;
    }
    return false;
}

int UtilityUI::getImageWidth(int numberOfImages) {
    const auto dim = getScreenBounds();
    return static_cast<int>(dim[0] / numberOfImages) - 15;
}

int UtilityUI::getImageWidth2(int numberOfImages, double width) {
    return static_cast<int>(width / numberOfImages) - 15;
}

void UtilityUI::updateImage(QLabel * label, const QByteArray& data, int imageSize) {
    const auto pixmap = QPixmap::fromImage(QImage::fromData(data));
    const double width = pixmap.width();
    const double height = pixmap.height();
    const auto pixmapResized = pixmap.scaled(imageSize, static_cast<int>(imageSize * (height / width)), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(pixmapResized);
}

void UtilityUI::removeChildren(QLayout * layout) {
    const auto deleteWidgets = true;
    while (QLayoutItem * item = layout->takeAt(0)) {
        if (deleteWidgets) {
            if (QWidget * widget = item->widget())
                widget->deleteLater();
        }
        if (QLayout * childLayout = item->layout())
            removeChildren(childLayout);
        delete item;
    }
}
