// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SERVERS_H
#define SERVERS_H

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <QByteArray>

// The places the data is read from, by name, kept apart from the models that read them (section 20 of docs/plans.md): a model file's url begins with `{@rrfs-ops}` and the name is looked up here,
// so a server that moves is one line of one file and not an edit of the program. The table is layered, the later over the earlier:
//   1. built in (the program's own, `builtInJson()`: it works with nothing else, offline, the first time),
//   2. the project's file (`servers/servers.json` of the repository, fetched once a day and kept: `refreshFromProject`),
//   3. the person's own file (`servers.json` in the folder of their own model files),
//   4. the preferences (`SERVER_<name>` = a base: Settings),
// and each entry says which one it came from. A server has one base or more (the first is tried first, the others are the fallbacks, in order). A file that is not valid is not used at all (the layer
// below stands) and the reason is kept. A published file can redirect where requests go, so it is checked: `https` only, a host the program already reads from (`allowedHost`) unless the person has
// allowed any host, a short list of plain names, no query or fragment, at most 64 KB.
namespace Servers {
    struct Entry {
        std::string name;
        std::vector<std::string> bases;   // "https://host/path", no trailing slash; the first is tried first
        std::string origin;               // "built in", "project", "yours", "settings"
        std::string note;
    };
    using Table = std::map<std::string, Entry>;

    // The text of the built-in table (the same as servers/servers.json of the repository: a test compares them).
    const std::string& builtInJson();

    // True for a host the program reads from today (and its subdomains): the suffixes of the built-in models' hosts. `anyHost` allows every host.
    bool allowedHost(const std::string& host);

    // Reads a file of this format into `out` (entries added over what it holds, `origin` on each). False, with the reason, when the file is not valid; `out` is then not touched.
    bool parse(const QByteArray& json, const std::string& origin, bool anyHost, Table& out, std::string& why);

    // The table now (a copy: the layers read once and kept; `reload` reads them again).
    Table table();
    void reload();

    // The places for a pattern that begins with `{@name}`: one pattern for each base (the first first), the token replaced. A pattern without the token is its own only place. An unknown name gives none.
    std::vector<std::string> places(const std::string& pattern);
    // A fallback that answered while the first place did not: its base goes first for a day, across restarts (kept in `promoted.json` beside the status; after a day the first place is tried again), in what places() and table() give, so every reader of the server goes to the
    // place that works. `url` is an address that begins with one of the server's bases; false when it is the first one already or belongs to no server. `demotedBase` is the base it was put before ("" if none).
    bool promote(const std::string& url);
    std::string demotedBase(const std::string& name);
    // The name of the server one of whose bases begins `url` ("" when none does).
    std::string nameOfUrl(const std::string& url);
    // The name in a leading `{@name}` ("" when there is none or it is not a plain name).
    std::string nameIn(const std::string& pattern);

    // The project's file: one request now (blocking: call it from a thread), the answer checked and kept for the next start. `done` gets true when a valid file was kept, or false with the reason. The
    // address is the repository's raw `servers/servers.json`; `url` overrides it (a test).
    void refreshFromProject(const std::function<void(bool, const std::string&)>& done, const std::string& url = {});
    // When the last fetch was (an ISO time) and what came of it; "" when there has been none.
    struct FetchStatus { std::string when, result; bool ok{false}; };
    FetchStatus lastFetch();
    // Once a day at most, and only when the person allows it (the preference SERVERS_FETCH, on by default): started in a thread of its own, answering nothing.
    void refreshIfDue();
    // The folders: where the project's file is kept and where the person's own is.
    std::string projectFile();
    std::string userFile();
}

#endif  // SERVERS_H
