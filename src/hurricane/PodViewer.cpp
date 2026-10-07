// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/PodViewer.h"
#include <QTextBrowser>
#include "ui/VBox.h"

namespace {
    QString q(const std::string& s) {
        return QString::fromStdString(s).toHtmlEscaped();
    }

    void table(QString& out, const vector<UtilityPod::Requirement>& list) {
        for (const auto& requirement : list) {
            out += "<h3>" + q(requirement.title) + "</h3>";
            if (requirement.flights.empty()) {
                out += "<p>No flights listed.</p>";
                continue;
            }
            out += "<table border='1' cellspacing='0' cellpadding='4'>"
                   "<tr style='background:#cfe2f3'><th>Flight</th><th>Aircraft</th><th>Fix time (Z)</th><th>Mission</th><th>Departs</th><th>Target</th>"
                   "<th>On station</th><th>Altitude</th><th>Type</th><th>WRA</th><th>Remarks</th></tr>";
            for (const auto& f : requirement.flights) {
                out += "<tr><td>" + q(f.ordinal) + "</td><td><b>" + q(f.aircraft) + "</b></td><td>" + q(f.fixTimes) + "</td><td>" + q(f.mission) + "</td><td>" + q(f.departure) +
                    "</td><td>" + q(f.position) + "</td><td>" + q(f.onStation) + "</td><td>" + q(f.altitude) + "</td><td>" + q(f.type) + "</td><td>" + q(f.wra) + "</td><td>" +
                    q(f.remarks) + "</td></tr>";
            }
            out += "</table>";
        }
    }
}

QString PodViewer::html(const HurricaneData::PodData& data) {
    const auto& pod = data.pod;
    QString out = "<h2>Tropical Cyclone Plan of the Day " + q(pod.number) + "</h2><p>Valid " + q(pod.valid) + " &nbsp; (" + q(data.file) + ")</p>";
    out += "<h3 style='color:#134'>Atlantic</h3>";
    if (pod.noAtlantic && pod.atlantic.empty()) {
        out += "<p>Negative reconnaissance requirements.</p>";
    }
    table(out, pod.atlantic);
    out += "<h3 style='color:#134'>Pacific</h3>";
    if (pod.noPacific && pod.pacific.empty()) {
        out += "<p>Negative reconnaissance requirements.</p>";
    }
    table(out, pod.pacific);
    if (!pod.notes.empty()) {
        out += "<h3>Outlook and remarks</h3>";
        for (const auto& n : pod.notes) {
            out += "<p>" + q(n) + "</p>";
        }
    }
    out += "<p style='color:#666'>Lines: A fix time, B mission, C departure, D forecast position of the feature, E time on station, F altitude (SFC to 10,000 ft is a "
           "dropsonde mission), G type of mission, H WRA (Weather Reconnaissance Area) activation, I remarks. From the NHC reconnaissance archive (REPRPD).</p>";
    return out;
}

QString PodViewer::summary(const HurricaneData::PodData& data) {
    const auto& pod = data.pod;
    int flights = 0;
    QString where;
    for (const auto& requirement : pod.atlantic) {
        flights += static_cast<int>(requirement.flights.size());
        const auto title = QString::fromStdString(requirement.title);
        where += (where.isEmpty() ? "" : ", ") + title.left(title.indexOf(" ("));
    }
    if (flights == 0) {
        return "Plan of the Day " + QString::fromStdString(pod.number) + ": no Atlantic flights";
    }
    return "Plan of the Day " + QString::fromStdString(pod.number) + ": " + QString::number(flights) + " Atlantic flights for " + where;
}

PodViewer::PodViewer(Window * parent, const std::shared_ptr<HurricaneData::PodData>& pod)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Recon - Plan of the Day");
    auto * browser = new QTextBrowser{this};
    browser->setHtml(pod->error.empty() ? html(*pod) : QString::fromStdString(pod->error).toHtmlEscaped());
    box.addWidgetReal(browser, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1100, 640);
}
