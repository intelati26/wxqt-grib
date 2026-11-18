// The feed viewer's two pure parts (misc/FeedViewer): when the feed is asked for again (a workday, 9 am or later, 24 hours after the last check) and the reading of an RSS item (picture, note, links, date).
#include <cstdio>
#include <QDateTime>
#include <QTimeZone>
#include "misc/FeedViewer.h"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s (line %d)\n", #c, __LINE__); failures++; } } while (0)
int main() {
    using F = FeedViewer;
    const auto at = [] (int y, int m, int d, int h, int mi) { return QDateTime{QDate{y, m, d}, QTime{h, mi}, QTimeZone::systemTimeZone()}; };
    const auto secs = [] (const QDateTime& t) { return t.toSecsSinceEpoch(); };
    const auto wed9 = at(2026, 11, 18, 9, 30), tue10 = at(2026, 11, 17, 10, 0), sat10 = at(2026, 11, 21, 10, 0), mon8 = at(2026, 11, 23, 8, 0), mon10 = at(2026, 11, 23, 10, 0);
    CHECK(F::dueToCheck(0, 0, wed9));                       // never read
    CHECK(F::dueToCheck(0, 0, sat10));                      // never read: at the weekend too (nothing to show otherwise)
    CHECK(!F::dueToCheck(0, secs(wed9.addSecs(-600)), wed9));   // a first try failed ten minutes ago
    CHECK(!F::dueToCheck(secs(tue10), 0, wed9));   // Tuesday 10:00 to Wednesday 9:30 is 23.5 hours: not yet
    CHECK(F::dueToCheck(secs(tue10), 0, tue10.addDays(1).addSecs(3600)));    // Wednesday 11:00: 25 hours
    CHECK(!F::dueToCheck(secs(at(2026, 11, 20, 10, 0)), 0, sat10));         // Friday 10:00 checked, Saturday 10:00 (24 h): a weekend, no
    CHECK(!F::dueToCheck(secs(at(2026, 11, 20, 10, 0)), 0, mon8));          // Monday 8:00: before 9
    CHECK(F::dueToCheck(secs(at(2026, 11, 20, 10, 0)), 0, mon10));          // Monday 10:00: a workday after 9, 72 hours
    CHECK(!F::dueToCheck(secs(at(2026, 11, 16, 10, 0)), secs(mon10.addSecs(-120)), mon10));   // the last try (2 minutes ago) failed: wait half an hour
    CHECK(F::dueToCheck(secs(at(2026, 11, 16, 10, 0)), secs(mon10.addSecs(-3600)), mon10));    // ... an hour ago: again
    const char * xml = "<?xml version=\"1.0\"?><rss><channel><title>t</title><item><title><![CDATA[Warm Everywhere]]></title><link>https://x/cat.php?day=2026-11-18</link>"
        "<description><![CDATA[<img src=\"https://x/a.png\" /><p>Mild weather.</p><p><a href=\"https://x/plot\">Generate This Chart on IEM Website</a></p>]]></description></item>"
        "<item><title>Second</title><link>https://x/cat.php?day=2026-11-17</link><description>plain</description></item></channel></rss>";
    const auto items = F::parse(QByteArray{xml});
    CHECK(items.size() == 2);
    CHECK(items.size() > 0 && items[0].title == "Warm Everywhere" && items[0].imageUrl == "https://x/a.png" && items[0].extraLink == "https://x/plot");
    CHECK(items.size() > 0 && items[0].text == "Mild weather." && items[0].date == "Wed 18 Nov 2026");
    CHECK(items.size() > 1 && items[1].imageUrl.isEmpty() && items[1].date == "Tue 17 Nov 2026");
    if (failures == 0) std::printf("feed tests passed\n");
    return failures;
}
