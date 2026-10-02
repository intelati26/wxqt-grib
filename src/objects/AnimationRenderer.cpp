// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/AnimationRenderer.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QHeaderView>
#include <QProgressBar>
#include <QTableWidgetItem>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include "util/AppState.h"

QPointer<AnimationRenderer> AnimationRenderer::instance;

AnimationRenderer::AnimationRenderer(QWidget * parent)
    : Window{parent}
    , buttonCancel{this, None, "Cancel waiting job"}
    , buttonClear{this, None, "Clear finished"}
    , buttonFolder{this, None, "Open folder"}
    , textNote{this, "Saved animations and images are encoded here, one at a time, so the program stays usable. Double-click a finished one to open it."}
    , table{new QTableWidget{0, 4, this}}
{
    setAttribute(Qt::WA_DeleteOnClose, false);   // kept (hidden) when closed, so a running job is never lost
    setTitle("Animation renderer");
    textNote.setWordWrap(true);
    table->setHorizontalHeaderLabels({"File", "Format", "Frames", "Status"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    buttonCancel.connect([this] { cancelSelected(); });
    buttonClear.connect([this] { clearFinished(); });
    buttonFolder.connect([this] { openFolder(); });
    QObject::connect(table, &QTableWidget::cellDoubleClicked, this, [this] (int row, int) { openFile(row); });
    rowButtons.addWidget(buttonCancel);
    rowButtons.addWidget(buttonClear);
    rowButtons.addWidget(buttonFolder);
    rowButtons.addStretch();
    box.addWidget(textNote);
    box.addWidgetReal(table, 1, Qt::Alignment{});
    box.addLayout(rowButtons);
    box.getAndShow(this);
    resize(900, 360);
}

void AnimationRenderer::submit(QWidget * from, const UtilityAnimationExport::Format& format, const vector<QByteArray>& frames,
                               int frameDelayMs, const QByteArray& still, const QString& path,
                               const UtilityAnimationExport::WebpOptions& webp) {
    if (instance.isNull()) {
        // a child of the main window (the top of the asking screen's parent chain), so it lives and dies with the program
        QWidget * top = from;
        while (top != nullptr && top->parentWidget() != nullptr) {
            top = top->parentWidget();
        }
        instance = new AnimationRenderer{top};
    }
    auto job = std::make_shared<Job>();
    job->format = format;
    job->frames = frames;
    job->frameCount = frames.size();
    job->frameDelayMs = frameDelayMs;
    job->still = still;
    job->path = path;
    job->webp = webp;
    job->status = "Waiting";
    instance->add(job);
    instance->show();
    instance->raise();
}

void AnimationRenderer::add(std::shared_ptr<Job> job) {
    jobs.push_back(job);
    table->insertRow(table->rowCount());
    for (int column = 0; column < 4; column += 1) {
        table->setItem(table->rowCount() - 1, column, new QTableWidgetItem{});
    }
    refreshRow(jobs.size() - 1);
    updateTitle();
    startNext();
}

void AnimationRenderer::refreshRow(size_t row) {
    const auto& job = *jobs[row];
    const auto r = static_cast<int>(row);
    table->item(r, 0)->setText(QFileInfo{job.path}.fileName());
    table->item(r, 0)->setToolTip(job.path);
    table->item(r, 1)->setText(job.format.label);
    table->item(r, 2)->setText(job.frameCount >= 2 ? QString::number(job.frameCount) : QString{"1 image"});
    auto text = job.status;
    if (!job.detail.isEmpty()) {
        text += ": " + job.detail;
    }
    table->item(r, 3)->setText(text);
    table->item(r, 3)->setToolTip(text);
    if (job.running && table->cellWidget(r, 3) == nullptr) {
        // a moving bar while it renders (the encoders report no progress)
        auto * bar = new QProgressBar{};
        bar->setRange(0, 0);
        bar->setTextVisible(false);
        bar->setMaximumHeight(8);
        table->setCellWidget(r, 3, bar);
    } else if (!job.running && table->cellWidget(r, 3) != nullptr) {
        table->removeCellWidget(r, 3);
    }
}

void AnimationRenderer::updateTitle() {
    int waiting = 0;
    int running = 0;
    for (const auto& job : jobs) {
        if (job->running) {
            running += 1;
        } else if (!job->finished) {
            waiting += 1;
        }
    }
    QString title = "Animation renderer";
    if (running + waiting > 0) {
        title += " - " + QString::number(running + waiting) + " to do";
    }
    setTitle(title.toStdString());
    buttonClear.getView()->setEnabled(static_cast<int>(jobs.size()) > running + waiting);
}

void AnimationRenderer::startNext() {
    if (busy || AppState::quitting) {
        return;
    }
    for (size_t i = 0; i < jobs.size(); i += 1) {
        auto job = jobs[i];
        if (job->finished || job->running) {
            continue;
        }
        busy = true;
        job->running = true;
        job->status = "Rendering...";
        job->started.start();
        refreshRow(i);
        updateTitle();
        auto * watcher = new QFutureWatcher<QString>{this};
        QObject::connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, job] {
            const auto error = watcher->result();
            watcher->deleteLater();
            finish(job, error.isEmpty(), error);
        });
        // a copy of the job's inputs for the worker: the window's own data is only touched on the UI thread
        watcher->setFuture(QtConcurrent::run([job] {
            if (AppState::quitting) {
                return QString{"the program was closing"};
            }
            QString error;
            const auto ok = UtilityAnimationExport::encodeToFileWith(job->webp, job->format, job->frames, job->frameDelayMs,
                                                                     job->still, job->path, error);
            if (!ok) {
                QFile::remove(job->path);
                return error.isEmpty() ? QString{"the encoder failed"} : error;
            }
            return QString{};
        }));
        return;
    }
}

void AnimationRenderer::finish(const std::shared_ptr<Job>& job, bool ok, const QString& error) {
    job->running = false;
    job->finished = true;
    const auto seconds = job->started.elapsed() / 1000.0;
    if (ok) {
        job->status = "Done";
        const auto size = QFileInfo{job->path}.size();
        job->detail = (size < 1048576 ? QString::number(size / 1024) + " KB" : QString::number(size / 1048576.0, 'f', 1) + " MB") +
                      " in " + QString::number(seconds, 'f', 1) + " s";
    } else {
        job->status = "Failed";
        job->detail = error;
        show();     // a failure should not go unseen
        raise();
    }
    job->frames.clear();    // the pictures are no longer needed: free them
    job->still.clear();
    for (size_t i = 0; i < jobs.size(); i += 1) {
        if (jobs[i] == job) {
            refreshRow(i);
        }
    }
    busy = false;
    updateTitle();
    startNext();
}

void AnimationRenderer::cancelSelected() {
    const auto row = table->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= jobs.size()) {
        return;
    }
    auto& job = *jobs[static_cast<size_t>(row)];
    if (job.running || job.finished) {
        return;     // an encode already going cannot be stopped part-way
    }
    job.finished = true;
    job.cancelled = true;
    job.status = "Cancelled";
    job.frames.clear();
    job.still.clear();
    refreshRow(static_cast<size_t>(row));
    updateTitle();
}

void AnimationRenderer::clearFinished() {
    for (int row = static_cast<int>(jobs.size()) - 1; row >= 0; row -= 1) {
        if (jobs[static_cast<size_t>(row)]->finished) {
            table->removeRow(row);
            jobs.erase(jobs.begin() + row);
        }
    }
    updateTitle();
}

void AnimationRenderer::openFolder() {
    const auto row = table->currentRow();
    const auto path = row >= 0 && static_cast<size_t>(row) < jobs.size() ? jobs[static_cast<size_t>(row)]->path
                                                                         : (jobs.empty() ? QString{} : jobs.back()->path);
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo{path}.absolutePath()));
    }
}

void AnimationRenderer::openFile(int row) {
    if (row < 0 || static_cast<size_t>(row) >= jobs.size()) {
        return;
    }
    const auto& job = *jobs[static_cast<size_t>(row)];
    if (job.finished && job.status == "Done") {
        QDesktopServices::openUrl(QUrl::fromLocalFile(job.path));
    }
}

void AnimationRenderer::closeEventCustom() {
    // only hides: the jobs go on
}
