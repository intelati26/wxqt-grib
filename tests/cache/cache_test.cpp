// Tests of PermanentCache: store, read, newer replaces older, offline fallback, adoption of old cache-folder files.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "util/PermanentCache.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    const QString root = QString::fromLocal8Bit(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/wxqt_cache_test";
    QDir{root}.removeRecursively();
    PermanentCache::setRoot(root);
    const PermanentCache c{"tornado"};
    const std::string big(200000, 'x');
    int downloads = 0;
    auto get = [&] (const std::string& text) { return [&downloads, text] { downloads++; return text; }; };

    auto a = c.fetch("1950-2024_a.csv", "1950-*_a.csv", 1000, get(big));
    CHECK(a.downloaded && a.text == big && downloads == 1);
    CHECK(QFile{root + "/data/tornado/1950-2024_a.csv.z"}.size() < 2000);   // compressed
    auto b = c.fetch("1950-2024_a.csv", "1950-*_a.csv", 1000, get("never"));
    CHECK(!b.downloaded && b.text == big && downloads == 1);               // read from disk, no download
    auto n = c.fetch("1950-2025_a.csv", "1950-*_a.csv", 1000, get(big + "y"));
    CHECK(n.downloaded && n.text.size() == big.size() + 1);
    CHECK(!c.has("1950-2024_a.csv") && c.has("1950-2025_a.csv"));           // the newer file replaced the older
    auto off = c.fetch("1950-2026_a.csv", "1950-*_a.csv", 1000, get("<html>error</html>"));
    CHECK(off.stale && off.name == "1950-2025_a.csv" && !off.text.empty()); // bad download: the newest kept
    auto none = c.fetch("", "1950-*_a.csv", 1000, get(""));
    CHECK(none.name == "1950-2025_a.csv" && none.stale);                    // listing unreadable
    CHECK(c.write("daily/260428.csv", "Time,F-Scale\n") && c.read("daily/260428.csv") == "Time,F-Scale\n");
    CHECK(PermanentCache{"empty"}.fetch("", "*", 1, get("")).text.empty());

    // a file left in the old cache folder is taken over
    const auto old = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/wxqt_cache_legacy_test";
    QDir{}.mkpath(old);
    { QFile f{old + "/hurdat2-1851-2020-1.txt"}; f.open(QIODevice::WriteOnly); f.write("AL011851,\n"); }
    const PermanentCache h{"hurricane", "wxqt_cache_legacy_test"};
    CHECK(h.newestName("hurdat2-1851-*.txt") == "hurdat2-1851-2020-1.txt");
    CHECK(h.read("hurdat2-1851-2020-1.txt") == "AL011851,\n");
    CHECK(!QFile::exists(old + "/hurdat2-1851-2020-1.txt") && QFile::exists(root + "/data/hurricane/hurdat2-1851-2020-1.txt.z"));
    QDir{old}.removeRecursively();
    QDir{root}.removeRecursively();
    std::printf(failures ? "%d failures\n" : "all cache tests passed\n", failures);
    return failures ? 1 : 0;
}
