// Manual tool: downloads a GFS run and writes the chart as a PNG.   demo <product> <sector> <hour> <out.png>
#include <cmath>
#include <cstdio>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include "gfs/GfsChart.h"

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    if (argc < 5) {
        std::printf("usage: demo product sector hour out.png\n");
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
        }
        curl.start("curl", args);
        curl.waitForFinished(130000);
        return curl.readAllStandardOutput();
    };
    GfsData data{config};
    GfsData::Run run;
    if (!data.latestRun(run)) {
        std::printf("no run\n");
        return 1;
    }
    const auto * product = GfsChart::product(argv[1]);
    const auto * sector = GfsChart::sector(argv[2]);
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
    GfsChart::Options options;
    QFile coast{"/home/mitch/Claude/wxqt-grib/resourceCreation/res/nhc_basins.bin"};
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
    const auto image = GfsChart::render(*product, *sector, grids, run, std::atoi(argv[3]), options);
    if (image.isNull() || !image.save(argv[4])) {
        std::printf("render failed\n");
        return 1;
    }
    std::printf("wrote %s (%dx%d) run %s\n", argv[4], image.width(), image.height(), run.id().c_str());
    return 0;
}
