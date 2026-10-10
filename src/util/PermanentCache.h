// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PERMANENTCACHE_H
#define PERMANENTCACHE_H

#include <functional>
#include <string>
#include <QString>

// Data that changes a few times a year (the HURDAT2 storm database, the SPC tornado database) is kept for good in the user's data folder, compressed, instead of in the
// cache folder that cleaning tools sweep. The first time a screen needs a file it is downloaded and stored; after that it is read from disk, and the network is only asked
// whether a newer file exists. A newer file replaces the older one whole (nothing is merged). With no network the newest stored file is used.
//
// One instance per module ("hurricane", "tornado"): files live in <AppDataLocation>/data/<module>/<name>.z (zlib, via qCompress). A name may contain a slash (daily/260428.csv).
class PermanentCache {
public:
    // legacyFolder: a sub-folder of the old cache location whose files are adopted (moved in, compressed) the first time they are asked for
    explicit PermanentCache(const QString& module, const QString& legacyFolder = {});
    static void setRoot(const QString& root);   // tests: use this folder instead of the user's data folder

    QString folder() const;
    bool has(const std::string& name) const;
    std::string read(const std::string& name) const;              // "" when there is no stored copy
    bool write(const std::string& name, const std::string& text) const;
    std::string newestName(const std::string& glob) const;        // the last name in sorted order matching "1950-*_actual_tornadoes.csv", or ""
    void prune(const std::string& glob, const std::string& keep) const;   // removes every match except keep

    struct Got {
        std::string text;   // empty when nothing could be had
        std::string name;   // the file the text came from (may be older than the one asked for)
        bool downloaded{false};
        bool stale{false};  // the file asked for could not be had; this is an older one
    };
    // The policy in one place. name is the file wanted now ("" if the listing could not be read), glob matches its older versions, minBytes guards against error pages.
    // Stored copy of name -> used with no network. Otherwise download() is called; a good result is stored and the older versions removed. If that fails too, the newest stored version.
    Got fetch(const std::string& name, const std::string& glob, size_t minBytes, const std::function<std::string()>& download) const;

private:
    QString path(const std::string& name) const;
    QString module;
    QString legacy;
};

#endif  // PERMANENTCACHE_H
