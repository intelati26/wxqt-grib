// Manual tool: downloads a GFS run and writes the chart as a PNG.   demo <product> <sector> <hour> <out.png>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include "gfs/GfsChart.h"
#include "gfs/GfsClimate.h"
#include "gfs/GfsModels.h"

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    if (argc == 2 && std::string{argv[1]} == "--list") {   // every chart the registry draws: model, id, label
        for (const auto& p : GfsChart::products()) {
            std::string lines;
            for (const auto& c : p.contours) {
                lines += (lines.empty() ? "" : ";") + c.title;
            }
            std::printf("%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\n", p.source.c_str(), p.id.c_str(), p.label.c_str(), GfsChart::category(p).c_str(), lines.c_str(), p.barbU.empty() ? 0 : 1, p.streamU.empty() ? 0 : 1, p.fillTitle.c_str());
        }
        return 0;
    }
    if (argc < 5) {
        std::printf("usage: demo product sector hour out.png [GFS|NBM|AIGFS|GEFS [cycle]]   or   demo --list\n");
        return 2;
    }
    GfsData::Config config;
    config.gdalBin = "/usr/bin";
    config.cacheFolder = "/tmp/gfsdemo";
    config.bytes = [] (const std::string& url, long long start, long long end) {
        QProcess curl;
        QStringList args{"-s", "-m", "120", QString::fromStdString(url)};
        if (end >= 0) {
            args << "-r" << QString::number(start) + "-" + QString::number(end);
        } else if (start > 0) {   // the last record of a file: to the end
            args << "-r" << QString::number(start) + "-";
        }
        curl.start("curl", args);
        curl.waitForFinished(130000);
        return curl.readAllStandardOutput();
    };
    const std::string sourceArg = argc > 5 ? argv[5] : "GFS";
    const bool hurricane = sourceArg.compare(0, 4, "HAFS") == 0;   // "HAFSA:09l": the model and the storm
    const std::string modelName = hurricane ? sourceArg.substr(0, sourceArg.find(':')) : sourceArg;
    const auto * def = GfsModels::find(modelName);
    if (!def) {
        std::printf("unknown model %s\n", modelName.c_str());
        return 2;
    }
    GfsData data{config, def->source(hurricane ? sourceArg.substr(sourceArg.find(':') + 1) : std::string{})};
    GfsData::Run run;
    if (const char * forced = std::getenv("DEMO_RUN"); forced && std::string{forced}.size() == 10) {   // DEMO_RUN=2026100818: that run, not the newest (to set a chart beside the model guidance site's of the same run)
        run = {std::string{forced}.substr(0, 8), std::string{forced}.substr(8, 2)};
    } else if (!data.latestRun(run)) {
        std::printf("no run\n");
        return 1;
    }
    if (argc > 6) {   // a cycle to use instead of the newest ("18")
        run.cycle = argv[6];
    }
    const auto * baseProduct = GfsChart::product(argv[1], modelName);
    std::vector<std::string> overlayIds;   // DEMO_OVERLAYS=mslp,barbs_500
    if (const char * env = std::getenv("DEMO_OVERLAYS")) {
        std::stringstream in{env};
        for (std::string id; std::getline(in, id, ',');) overlayIds.push_back(id);
    }
    const auto composed = baseProduct ? GfsChart::compose(*baseProduct, overlayIds) : GfsChart::Product{};
    const auto * product = baseProduct ? &composed : nullptr;
    GfsChart::Sector stormSector;
    const auto * sector = hurricane ? &stormSector : GfsChart::sector(argv[2]);
    if (!product || !sector) {
        std::printf("unknown product or sector\n");
        return 2;
    }
    GfsChart::Grids grids;
    std::string error;
    if (!data.load(run, GfsChart::needs(*product, std::atoi(argv[3])), grids, error)) {
        std::printf("load failed: %s\n", error.c_str());
        return 1;
    }
    if (hurricane) {   // the chart is the grid of the first field, as the app does
        for (const auto& need : GfsChart::needs(*product, std::atoi(argv[3]))) {
            const auto found = grids.find(need.want.key);
            if (found != grids.end() && !found->second.empty() && need.want.stat != "ww3") {
                const auto& g = found->second;
                stormSector = GfsChart::gridSector(argv[2], g);
                break;
            }
        }
        if (stormSector.id.empty() && !grids.empty()) {
            const auto& g = grids.begin()->second;
            stormSector = GfsChart::gridSector(argv[2], g);
        }
    }
    GfsClimate climate{"/usr/bin"};
    GfsChart::Options options;
    options.climate = &climate;
    if (const char * mag = std::getenv("DEMO_MAG"); mag && std::string{mag} == "0") {   // DEMO_MAG=0: our own color scales
        options.magColors = false;
    }
    QFile coast{"/home/mitch/Claude/wxqt-grib/resourceCreation/res/nhc_basins.bin"};   // (the app itself draws Coast::borders(): the world plus the states)
    if (coast.open(QIODevice::ReadOnly)) {
        const auto bytes = coast.readAll();
        const auto * values = reinterpret_cast<const float *>(bytes.constData());
        std::vector<std::pair<float, float>> line;
        for (qsizetype i = 0; i + 1 < bytes.size() / 4; i += 2) {
            if (std::isnan(values[i])) {
                options.lines.push_back(std::move(line));
                line.clear();
            } else {
                line.emplace_back(values[i], values[i + 1]);
            }
        }
    }
    if (hurricane) {
        QProcess curl;
        curl.start("curl", {"-s", "-m", "60", QString::fromStdString(data.fileUrl(run, 0, "trak"))});
        curl.waitForFinished(70000);
        options.track = GfsChart::parseTrack(curl.readAllStandardOutput().toStdString());
        std::printf("track points: %zu\n", options.track.size());
        options.trackName = modelName == "HAFSB" ? "HAFS-B" : "HAFS-A";
        const std::string otherName = modelName == "HAFSB" ? "HAFSA" : "HAFSB";
        QProcess curl2;
        curl2.start("curl", {"-s", "-m", "60", QString::fromStdString(GfsData::hafs(otherName, sourceArg.substr(sourceArg.find(':') + 1)).fileUrl(run, 0, "trak"))});
        curl2.waitForFinished(70000);
        options.trackOther = GfsChart::parseTrack(curl2.readAllStandardOutput().toStdString());
        options.trackOtherName = otherName == "HAFSB" ? "HAFS-B" : "HAFS-A";
        if (std::string{argv[1]}.compare(0, 5, "swath") == 0) {
            stormSector = GfsChart::cropToTrack(stormSector, options.track, std::atoi(argv[3]), 4.0);
        }
    }
    const auto image = GfsChart::render(*product, *sector, grids, run, std::atoi(argv[3]), options);
    if (image.isNull() || !image.save(argv[4])) {
        std::printf("render failed\n");
        return 1;
    }
    std::printf("wrote %s (%dx%d) run %s\n", argv[4], image.width(), image.height(), run.id().c_str());
    return 0;
}
