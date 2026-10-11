// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/HelpViewer.h"
#include <map>
#include <QDesktopServices>
#include <QHeaderView>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"

namespace {
    QString html(const std::string& text) {
        return QString::fromStdString(text).toHtmlEscaped();
    }

    QString modelPage(const GfsModels::Def& d) {
        QString out = "<h2>" + html(d.id) + "</h2><p><i>" + html(d.label) + "</i></p>";
        out += "<h3>Why it matters</h3><p>" + (d.why.empty() ? QString{"<i>No description yet. A model file can carry one with a <code>why</code> line.</i>"} : html(d.why)) + "</p>";
        int charts = 0;
        for (const auto& p : GfsChart::products()) {
            charts += p.source == d.id ? 1 : 0;
        }
        QString hours;
        for (const auto& h : d.hours) {
            hours += (hours.isEmpty() ? "" : ", ") + QString::number(h.from) + "-" + QString::number(h.to) + " h every " + QString::number(h.step);
        }
        out += "<h3>In this app</h3><ul>";
        out += "<li>" + QString::number(charts) + (charts == 1 ? " chart" : " charts") + " in the Model Viewer (its model picker group: " + html(GfsModels::pickerGroup(d.id)) + ")</li>";
        out += "<li>Forecast hours: " + hours + "</li>";
        out += "<li>Runs: " + (d.hourlyRuns ? QString{"every hour"} : QString{"every "} + QString::number(d.cycleStep) + " hours") + "</li>";
        if (d.clone.enabled) {
            out += "<li>Drawn with the same charts as the GFS wherever it has the fields</li>";
        }
        out += "</ul>";
        QString source, read;
        for (const auto& l : d.links) {
            const auto link = "<li><a href=\"" + QString::fromStdString(l.url).toHtmlEscaped() + "\">" + html(l.label) + "</a></li>";
            (l.kind == "source" ? source : read) += link;
        }
        if (!source.isEmpty()) {
            out += "<h3>Where the data is read from</h3><ul>" + source + "</ul>";
        }
        if (!read.isEmpty()) {
            out += "<h3>Further reading</h3><ul>";
            for (const auto& l : d.links) {
                if (l.kind == "source") {
                    continue;
                }
                const auto what = l.kind == "wikipedia" ? "Wikipedia" : l.kind == "background" ? "Background (Wikipedia)" : "From the people who run it";
                out += "<li><a href=\"" + QString::fromStdString(l.url).toHtmlEscaped() + "\">" + html(l.label) + "</a> <span style=\"color:gray\">- " + what + "</span></li>";
            }
            out += "</ul>";
        }
        return out;
    }

    QString addModelPage() {
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        QString out = "<h2>Adding a model</h2>"
            "<p>A model is a text file. Put a <code>name.model</code> file in</p><p><code>" + folder.toHtmlEscaped() + "</code></p>"
            "<p>and restart the program: it appears in the Model Viewer's model picker and in Settings &gt; Model Sources, where its server is checked.</p>"
            "<p>A file says where the model's files are (<code>url</code>, with <code>{date}</code>, <code>{cycle}</code>, <code>{hour3}</code> and so on), what kind of file it is "
            "(<code>kind = grib</code> with an index, <code>whole</code> for one field per file, <code>netcdf</code>), when runs appear, the forecast hours, and which of the GFS charts it can draw "
            "(<code>clone = true</code> with the variables and levels it has). Add <code>why</code> and <code>link</code> lines and it gets a page here like the others.</p>"
            "<pre>id = MYMODEL\nlabel = My regional model, 3 km\nkind = grib\nurl = https://example.org/{date}/{cycle}/m.t{cycle}z.f{hour3}.grib2\n"
            "cycle_hours = 6\nlag_hours = 4\nhours = 0-48/1\nclone = true\nclone_variables = HGT, TMP, UGRD, VGRD, PRMSL\n"
            "why = What it adds and when to trust it.\nlink = source | The data's page | https://example.org/data</pre>"
            "<p>The full list of keys is in <code>docs/model-files.md</code> in the source.</p>";
        const auto problems = GfsModels::userProblems();
        if (!problems.empty()) {
            out += "<h3>Files that were not used</h3><ul>";
            for (const auto& p : problems) {
                out += "<li>" + html(p) + "</li>";
            }
            out += "</ul>";
        }
        return out;
    }

    QString sourcesPage() {
        return "<h2>Model sources</h2><p>Models and analyses change: files move, products are retired and replaced. Settings &gt; Model Sources asks each model's server for its newest run "
               "and whether that run lists the fields of the model's first chart, when you open the tab. Nothing is downloaded and nothing runs in the background.</p>"
               "<p>A <b>PROBLEM</b> there means the server did not answer, or the files are no longer where the program looks. The model's page here has the people who run it, "
               "which is where a notice of a change is usually found.</p>";
    }
}

HelpViewer::HelpViewer(Window * parent)
    : Window{parent}
{
    setTitle("Help and further reading");
    auto * split = new QSplitter{Qt::Horizontal, this};
    tree = new QTreeWidget{split};
    tree->setHeaderHidden(true);
    page = new QTextBrowser{split};
    page->setOpenExternalLinks(true);
    split->addWidget(tree);
    split->addWidget(page);
    split->setStretchFactor(1, 1);
    split->setSizes({260, 700});
    auto * models = new QTreeWidgetItem{tree, {"Models"}};
    std::map<std::string, QTreeWidgetItem *> groups;
    for (const auto& d : GfsModels::all()) {
        const auto group = GfsModels::pickerGroup(d.id);
        auto & node = groups[group];
        if (node == nullptr) {
            node = new QTreeWidgetItem{models, {QString::fromStdString(group)}};
        }
        auto * item = new QTreeWidgetItem{node, {QString::fromStdString(d.id)}};
        item->setData(0, Qt::UserRole, QString::fromStdString("model:" + d.id));
        item->setToolTip(0, QString::fromStdString(d.label));
    }
    auto * howTo = new QTreeWidgetItem{tree, {"Adding a model"}};
    howTo->setData(0, Qt::UserRole, "add");
    auto * check = new QTreeWidgetItem{tree, {"Model sources"}};
    check->setData(0, Qt::UserRole, "sources");
    tree->expandItem(models);
    QObject::connect(tree, &QTreeWidget::currentItemChanged, this, [this] (QTreeWidgetItem * item) {
        if (item != nullptr) {
            showTopic(item->data(0, Qt::UserRole).toString().toStdString());
        }
    });
    box.addWidgetReal(split, 1, Qt::Alignment{});
    box.getAndShow(this);
    setSize(1000, 640);
    page->setHtml("<h2>Help and further reading</h2><p>Pick a model on the left to see why it matters, what the app shows of it and where its data and its documentation are. "
                  "<i>Adding a model</i> says how to add one of your own.</p>");
}

void HelpViewer::showTopic(const std::string& topic) {
    if (topic.compare(0, 6, "model:") == 0) {
        if (const auto * d = GfsModels::find(topic.substr(6))) {
            page->setHtml(modelPage(*d));
            return;
        }
    } else if (topic == "add") {
        page->setHtml(addModelPage());
        return;
    } else if (topic == "sources") {
        page->setHtml(sourcesPage());
        return;
    }
}
