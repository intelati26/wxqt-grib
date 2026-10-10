// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GUIDANCETREE_H
#define GUIDANCETREE_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <QColor>
#include <QComboBox>
#include <QTreeWidget>
#include <QWidget>
#include "hurricane/UtilityAtcf.h"

using std::string;
using std::vector;

// Which model tracks the hurricane screen draws, as a tree that opens and closes: the categories (NHC, global models, hurricane models, ensemble means,
// consensus aids, ensemble members, simple and statistical, other) hold the models, one line for each model whatever the ATCF codes of its runs (GFS is
// AVNO, AVNI, GFSO ...). A category's box switches all its models, and the quick select above the tree sets the usual combinations. The default is the
// official forecast and the best track, GFS and the ECMWF model, the hurricane models and the ensemble means. What is chosen is remembered.
class GuidanceTree : public QWidget {
public:
    explicit GuidanceTree(QWidget * parent = nullptr);
    bool shown(const string& tech) const;        // is this technique's track drawn
    bool officialShown() const { return leafOn("nhc/official"); }
    bool bestShown() const { return leafOn("nhc/best"); }
    bool isOn(const string& id) const { return leafOn(id); }   // any leaf by its id: "members/aifs", "means/ifs", "global/openruns" ...
    QColor colorOf(const string& tech, const QColor& fallback) const;
    // the number of runs each model has for the storm on show: a model with none is greyed
    void setAvailable(const vector<UtilityAtcf::Track>& guidance);
    // the run of models that are not in the ATCF guidance (the ECMWF open data, the official forecast): leaf id -> cycle yyyymmddhh, so the line can say "(06z)"
    void setCycles(const std::map<string, vector<string>>& cycles);
    std::function<void()> changed;

private:
    struct Leaf {
        string id;
        string label;
        vector<string> techs;
        bool onByDefault{false};
        QColor color;
        bool catchAll{false};      // takes every technique no other leaf lists (the "Other" leaf), or the ensemble member pattern
        bool members{false};
    };
    struct Category {
        string id;
        string label;
        bool expanded{true};
        vector<Leaf> leaves;
    };
    static const vector<Category>& catalog();
    bool leafOn(const string& id) const;
    void apply(const std::set<string>& on);
    void save();
    std::set<string> current() const;
    std::set<string> preset(int index) const;
    const Leaf * leafOf(const string& tech) const;
    QTreeWidget * tree{};
    QComboBox * quick{};
    std::map<string, QTreeWidgetItem *> items;
    std::map<string, const Leaf *> byTech;
    void relabel();
    std::map<string, int> counts;
    std::map<string, std::set<string>> cycles;   // leaf id -> the cycles (yyyymmddhh) of the runs it has
    bool haveGuidance{false};
    bool building{false};
};

#endif  // GUIDANCETREE_H
