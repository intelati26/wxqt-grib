// Tests of what was added with the model bar and the hardening: the chart selection both screens hold, the model bar's shared pieces, the hurricane id, the page of the model guidance
// site's pictures, the GDAL helper's reading and its timeout, and the capped decoder. Built from the program's own objects (build_with_app.sh); no network.
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include "util/MyApplication.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include "gfs/ChartSelection.h"
#include "gfs/GdalRaster.h"
#include "gfs/GfsModels.h"
#include "hurricane/HafsIntensity.h"
#include "models/ModelBar.h"
#include "models/ObjectModel.h"
#include "models/UtilityModelNcepInterface.h"
#include "objects/URL.h"
#include "util/UtilityGzip.h"

static int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::printf("FAIL line %d: %s\n", __LINE__, #__VA_ARGS__); failures++; } } while (0)

namespace {
    bool has(const std::vector<std::string>& list, const std::string& id) {
        return std::find(list.begin(), list.end(), id) != list.end();
    }

    void chartSelection() {
        ChartSelection s;
        s.model = "GFS";
        s.param = "500_wnd_ht";
        s.overlays = {"a", "b"};
        s.overlayModels = {"IFS", "NBM"};
        s.member = "c00";
        s.sigma = true;
        s.changeBack = 12;
        s.maxDay = false;
        CHECK(s.usable());
        ChartSelection back;
        CHECK(ChartSelection::fromLine(s.line(), back));
        CHECK(back.model == s.model && back.param == s.param && back.overlays == s.overlays && back.overlayModels == s.overlayModels);
        CHECK(back.member == "c00" && back.sigma && back.changeBack == 12 && !back.maxDay);
        CHECK(back.line() == s.line());
        ChartSelection untouched{"KEEP", "KEEP"};
        CHECK(!ChartSelection::fromLine("not a line", untouched) && untouched.model == "KEEP");   // (a line that is not one leaves the selection as it was)
        CHECK(!ChartSelection::fromLine("", untouched) && untouched.model == "KEEP");
        CHECK((ChartSelection::splitList("a,,b,") == std::vector<std::string>{"a", "b"}) && ChartSelection::joinList({"x", "y"}) == "x,y");
        CHECK(!ChartSelection{"GFS", "no_such_chart"}.usable() && !ChartSelection{"NO_MODEL", "500_wnd_ht"}.usable());

        // the request of the renderer: a plain chart with the other models' lines, a change without them, a maximum of the 24 hours ending at the hour
        const std::vector<int> hours{0, 6, 12, 18, 24, 30, 36, 42, 48};
        ChartSelection plain{"GFS", "500_wnd_ht"};
        plain.overlayModels = {"IFS"};
        plain.member = "c01";
        plain.sigma = true;
        auto v = plain.variant(hours, 30);
        CHECK(v.kind == GfsRender::Variant::Kind::None && v.overlayModels.size() == 1 && v.member == "c01" && v.standardDeviations);
        plain.changeBack = 24;
        v = plain.variant(hours, 30);
        CHECK(v.kind == GfsRender::Variant::Kind::Change && v.hoursBack == 24 && v.overlayModels.empty());
        plain.changeBack = 0;
        plain.maxDay = true;
        v = plain.variant(hours, 30);
        CHECK(v.kind == GfsRender::Variant::Kind::Max && v.hours == (std::vector<int>{6, 12, 18, 24, 30}));
        v = plain.variant(hours, 0);   // one hour only: no maximum to take, the plain chart
        CHECK(v.kind == GfsRender::Variant::Kind::None && v.hours.empty());
    }

    void modelBar() {
        const auto longLabels = ModelBar::changeLabels(false), shortLabels = ModelBar::changeLabels(true);
        CHECK(longLabels.size() == 5 && shortLabels.size() == 4 && longLabels[0] == "Plain chart" && shortLabels[0] == "Plain chart");
        for (const int hours : {0, 6, 12, 24}) {
            int back = -1;
            bool maxDay = true;
            const int index = ModelBar::changeIndex(hours, false);
            CHECK(index >= 0);
            ModelBar::changeFromIndex(index, back, maxDay);
            CHECK(back == hours && !maxDay);
        }
        int back = 7;
        bool maxDay = false;
        ModelBar::changeFromIndex(ModelBar::changeIndex(0, true), back, maxDay);
        CHECK(maxDay && back == 0);
        CHECK(ModelBar::changeIndex(18, false) == -1 && ModelBar::changeIndex(48, false) == -1);   // (a change the box has no item for)

        const auto entries = ModelBar::modelEntries({"HRRR", "GFS", "IFS", "NO_SUCH_MODEL"});
        CHECK(entries.size() == 4);
        const auto find = [&entries] (const std::string& id) { return std::find_if(entries.begin(), entries.end(), [&id] (const ProductPicker::Entry& e) { return e.id == id; }); };
        CHECK(find("GFS") != entries.end() && find("GFS")->label.compare(0, 3, "GFS") == 0 && !find("GFS")->group.empty());
        CHECK(find("NO_SUCH_MODEL") != entries.end() && find("NO_SUCH_MODEL")->label == "NO_SUCH_MODEL");   // (a model with no description is listed by its name)
        CHECK(find("GFS") < find("HRRR") || find("GFS")->group != find("HRRR")->group);   // the main models first within a group
        const auto first = std::find_if(entries.begin(), entries.end(), [] (const ProductPicker::Entry& e) { return e.group == "Deterministic: Global"; });
        CHECK(first != entries.end() && first->id == "GFS");
    }

    void hurricaneIds() {
        CHECK(Hafs::nhcId("09l", 2026) == "al092026" && Hafs::nhcId("18e", 2026) == "ep182026" && Hafs::nhcId("01c", 2025) == "cp012025");
        CHECK(Hafs::nhcId("09x", 2026).empty() && Hafs::nhcId("CONUS", 2026).empty() && Hafs::nhcId("", 2026).empty());
        for (const std::string id : {"al092026", "ep182026", "cp012025"}) {   // the two ids are each other's
            CHECK(Hafs::nhcId(Hafs::modelId(id), std::stoi(id.substr(4))) == id);
        }
    }

    void pictureModelsPage() {
        const auto& drawn = UtilityModelNcepInterface::models();
        const auto& pictures = UtilityModelNcepInterface::pictureModels();
        CHECK(!drawn.empty() && !pictures.empty());
        for (const auto& id : pictures) {
            CHECK(!GfsModels::draws(id) && !has(drawn, id));   // a picture is not on the page of the charts drawn here, and nothing drawn is on the pictures'
        }
        for (const auto& id : drawn) {
            CHECK(GfsModels::draws(id) || GfsModels::find(id) != nullptr);
        }
        CHECK(has(drawn, "GFS") && !has(pictures, "GFS") && has(pictures, "WW3"));
        ObjectModel charts{"NCEP"}, mag{"NCEPMAG"};
        CHECK(charts.kind == "NCEP" && mag.kind == "NCEP");   // the same way of listing and fetching
        CHECK(charts.prefModel == "NCEP" && mag.prefModel == "NCEPMAG");
        CHECK(charts.prefParam != mag.prefParam && charts.prefSector != mag.prefSector && charts.prefRunPosn != mag.prefRunPosn);   // its choices are kept apart
        CHECK(mag.models == pictures && charts.models == drawn);
        CHECK(has(mag.models, mag.model));
    }

    void gdalReading() {
        QTemporaryDir dir;
        CHECK(dir.isValid());
        const auto write = [&dir] (const QString& name, const QByteArray& bytes) {
            QFile f{dir.filePath(name)};
            CHECK(f.open(QIODevice::WriteOnly));
            f.write(bytes);
            return dir.filePath(name);
        };
        const QByteArray header = "ENVI\nsamples = 3\nlines = 2\nbands = 1\ndata type = 4\nbyte order = 0\nmap info = {Geographic Lat/Lon, 1, 1, -100.0, 40.0, 0.5, 0.5, WGS-84}\n";
        float cells[6] = {1, 2, 3, 4, 5, 6};
        const auto raw = write("a.raw", QByteArray{reinterpret_cast<const char *>(cells), sizeof cells});
        const auto hdr = write("a.hdr", header);
        GfsGrid::Grid g;
        int bands = 0;
        std::vector<float> values;
        std::string error;
        CHECK(GdalRaster::readEnvi(raw, hdr, g, bands, values, error));
        CHECK(g.columns == 3 && g.rows == 2 && bands == 1 && values.size() == 6 && values[5] == 6.0f);
        CHECK(std::abs(g.step - 0.5) < 1e-9 && std::abs(g.lon0 - (-100.0 + 0.25)) < 1e-9 && std::abs(g.lat0 - (40.0 - 0.25)) < 1e-9);   // (the header gives the corner of the first cell)
        // the header is not there: said so, not the grid "not plain" of a file that was never read
        CHECK(!GdalRaster::readEnvi(raw, dir.filePath("none.hdr"), g, bands, values, error) && error == "gdal wrote no header for the grid");
        // the raw file is not the size the header says
        const auto shortRaw = write("b.raw", QByteArray{reinterpret_cast<const char *>(cells), 20});
        CHECK(!GdalRaster::readEnvi(shortRaw, hdr, g, bands, values, error) && error == "the decoded field has the wrong size");
        // cells that are not square are refused unless asked for
        const auto skewed = write("c.hdr", "ENVI\nsamples = 3\nlines = 2\nbands = 1\nmap info = {Geographic Lat/Lon, 1, 1, -100.0, 40.0, 0.5, 0.25, WGS-84}\n");
        CHECK(!GdalRaster::readEnvi(raw, skewed, g, bands, values, error) && error == "the grid is not a plain latitude / longitude grid");
        CHECK(GdalRaster::readEnvi(raw, skewed, g, bands, values, error, false));
        // big-endian floats are turned
        const unsigned char big[4] = {0x3f, 0x80, 0x00, 0x00};   // 1.0f
        const auto bigRaw = write("d.raw", QByteArray{reinterpret_cast<const char *>(big), 4});
        const auto bigHdr = write("d.hdr", "ENVI\nsamples = 1\nlines = 1\nbands = 1\nbyte order = 1\nmap info = {Geographic Lat/Lon, 1, 1, 0.0, 0.0, 1.0, 1.0, WGS-84}\n");
        CHECK(GdalRaster::readEnvi(bigRaw, bigHdr, g, bands, values, error) && values.size() == 1 && values[0] == 1.0f);

        // a tool that outlives its timeout is stopped and said so, not read as a success; one that is not there is said too; one that fails gives its message
        GdalRaster::Options options;
        options.gdalBin = dir.path();
        options.timeoutMs = 700;
        write("gdal_translate", "#!/bin/sh\nsleep 20\n");
        QFile::setPermissions(dir.filePath("gdal_translate"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        std::string message;
        CHECK(!GdalRaster::translate("input", dir.filePath("slow"), options, message) && message == "gdal did not finish in time");
        write("gdal_translate", "#!/bin/sh\necho 'no such driver' >&2\nexit 3\n");
        QFile::setPermissions(dir.filePath("gdal_translate"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        CHECK(!GdalRaster::translate("input", dir.filePath("bad"), options, message) && message == "no such driver");
        options.gdalBin = dir.filePath("nowhere");
        CHECK(!GdalRaster::translate("input", dir.filePath("gone"), options, message) && message.compare(0, 6, "gdal c") == 0);
        std::string why;
        CHECK(!GdalRaster::read("input", dir.filePath("gone"), options, g, bands, values, why) && why.find("gdal failed on input") == 0);
    }

    void cappedDecoder() {
        const QByteArray plain(100000, 'x');
        const auto packed = qCompress(plain, 9).mid(4);   // (Qt's zlib stream without its 4 byte length)
        const auto * data = reinterpret_cast<const unsigned char *>(packed.constData());
        std::string out;
        CHECK(UtilityGzip::inflateZlib(data, static_cast<size_t>(packed.size()), 100000, out) && out.size() == 100000 && out[99999] == 'x');
        out.clear();
        CHECK(!UtilityGzip::inflateZlib(data, static_cast<size_t>(packed.size()), 99999, out));   // one byte over what may be: refused, and not decoded on
        CHECK(out.size() <= 99999 + 258);   // (it stops within one copy, 258 bytes at most, of the limit)
        out.clear();
        CHECK(!UtilityGzip::inflateZlib(reinterpret_cast<const unsigned char *>("not zlib at all"), 15, 1000, out));
        CHECK(URL::maxAnswerBytes == 1536ll * 1024 * 1024);
    }
}

int main(int argc, char ** argv) {
    QCoreApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_features_test_prefs.ini", QSettings::IniFormat};
    chartSelection();
    modelBar();
    hurricaneIds();
    pictureModelsPage();
    gdalReading();
    cappedDecoder();
    std::printf("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
