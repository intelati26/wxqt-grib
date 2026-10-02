// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ANIMATIONRENDERER_H
#define ANIMATIONRENDERER_H

#include <deque>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QTableWidget>
#include <QElapsedTimer>
#include "objects/UtilityAnimationExport.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::vector;

// The animation renderer: where every "Save animation / image" is encoded, in the background, so a long encode (JPEG XL, AVIF, WebP,
// MP4 of a big loop) does not lock the program up. Save asks for the file name and format as before, then hands the frames to this
// window and returns at once; the window lists the jobs (waiting, rendering, done, failed with the reason), runs them one at a time
// in the order they were queued, and keeps working while the viewers it came from are used or closed.
class AnimationRenderer : public Window {
public:
    // queue a job (and show the window); `from` is the screen that asked, only used to find the main window
    static void submit(QWidget * from, const UtilityAnimationExport::Format&, const vector<QByteArray>& frames, int frameDelayMs,
                       const QByteArray& still, const QString& path, const UtilityAnimationExport::WebpOptions&);

private:
    explicit AnimationRenderer(QWidget * parent);
    struct Job {
        UtilityAnimationExport::Format format;
        vector<QByteArray> frames;
        int frameDelayMs;
        size_t frameCount{0};
        QByteArray still;
        QString path;
        UtilityAnimationExport::WebpOptions webp;
        QString status;     // "Waiting", "Rendering...", "Done", "Failed"
        QString detail;     // the size and time, or the reason it failed
        bool running{false};
        bool finished{false};
        bool cancelled{false};
        QElapsedTimer started;
    };
    void add(std::shared_ptr<Job>);
    void startNext();
    void finish(const std::shared_ptr<Job>&, bool ok, const QString& error);
    void refreshRow(size_t);
    void cancelSelected();
    void clearFinished();
    void openFolder();
    void openFile(int row);
    void updateTitle();
    void closeEventCustom() override;
    VBox box;
    HBox rowButtons;
    Button buttonCancel;
    Button buttonClear;
    Button buttonFolder;
    Text textNote;
    QTableWidget * table;
    std::deque<std::shared_ptr<Job>> jobs;   // parallel to the table's rows
    bool busy{false};
    static QPointer<AnimationRenderer> instance;
};

#endif  // ANIMATIONRENDERER_H
