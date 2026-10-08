// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/GuidanceTree.h"
#include <algorithm>
#include <sstream>
#include <QPixmap>
#include <QVBoxLayout>
#include "util/Utility.h"

namespace {
    QIcon swatch(const QColor& color) {
        QPixmap pixmap{14, 14};
        pixmap.fill(color);
        return QIcon{pixmap};
    }
}

const vector<GuidanceTree::Category>& GuidanceTree::catalog() {
    static const vector<Category> all{
        {"nhc", "NHC", true, {
            {"nhc/official", "Official forecast", {"OFCL", "OFCI", "OFC2", "OFCP", "OFPI", "OFP2", "OFCO"}, true, QColor{255, 255, 255}},
            {"nhc/best", "Best track so far", {}, true, QColor{235, 235, 235}},
        }},
        {"global", "Global models", true, {
            {"global/gfs", "GFS", {"AVNO", "AVNI", "AVNX", "AVXI", "GFSO", "GFSI"}, true, QColor{0, 205, 255}},
            {"global/ecmwf", "ECMWF (Euro) in NHC's guidance", {"EMX", "EMXI", "EMX2", "ECMF", "EMDT"}, true, QColor{255, 175, 40}},
            {"global/openruns", "ECMWF IFS and AIFS single runs (open data)", {}, true, QColor{255, 140, 30}},
            {"global/cmc", "Canadian (CMC)", {"CMC", "CMCI"}, false, QColor{255, 80, 80}},
            {"global/ukmet", "UKMET", {"UKM", "UKMI", "UKX", "UKXI", "UKX2"}, false, QColor{120, 150, 255}},
            {"global/navgem", "NAVGEM", {"NGX", "NGXI", "NGX2", "NVGM", "NVGI"}, false, QColor{190, 190, 120}},
            {"global/ai", "AI models (AIFS, Google, others)", {"AIFS", "AIFI", "GDMI", "GDMN", "FNV3", "FNVI", "AIDA", "GRAP", "PANG", "AVNP"}, false, QColor{40, 255, 170}},
            {"global/other", "Other global (JMA, NAM, ...)", {"JGSM", "JGSI", "NAM", "NAMI", "GOES"}, false, QColor{170, 170, 170}},
        }},
        {"hurricane", "Hurricane models", true, {
            {"hurricane/hafsa", "HAFS-A", {"HFSA", "HFAI", "HAFA"}, true, QColor{255, 90, 220}},
            {"hurricane/hafsb", "HAFS-B", {"HFSB", "HFBI", "HAFB"}, true, QColor{190, 100, 255}},
            {"hurricane/hwrf", "HWRF", {"HWRF", "HWFI", "HWRP", "HWFP"}, true, QColor{255, 140, 160}},
            {"hurricane/hmon", "HMON", {"HMON", "HMNI"}, true, QColor{150, 235, 120}},
            {"hurricane/coamps", "COAMPS-TC", {"CTCX", "CTCI", "COTC", "COTI"}, false, QColor{230, 200, 90}},
            {"hurricane/gfdl", "GFDL", {"GFDL", "GHMI"}, false, QColor{200, 160, 255}},
        }},
        {"means", "Ensemble means", true, {
            {"means/gefs", "GFS ensemble mean", {"AEMN", "AEMI"}, true, QColor{60, 220, 255}},
            {"means/eps", "ECMWF ensemble mean", {"EEMN"}, true, QColor{255, 200, 70}},
            {"means/ifs", "IFS ensemble mean (ECMWF open data)", {}, true, QColor{255, 140, 30}},
            {"means/aifs", "AIFS ensemble mean (ECMWF open data)", {}, true, QColor{40, 255, 170}},
            {"means/fnv3", "DeepMind FNV3 ensemble mean (Google Weather Lab, experimental)", {}, true, QColor{228, 130, 255}},
            {"means/wnv3", "DeepMind WeatherNext 3 ensemble mean (Google Weather Lab, experimental)", {}, true, QColor{255, 130, 190}},
            {"means/cmc", "Canadian ensemble mean", {"CEMN"}, true, QColor{255, 120, 120}},
            {"means/uk", "UKMET ensemble mean", {"UEMN"}, true, QColor{140, 170, 255}},
        }},
        {"consensus", "Consensus aids", false, {
            {"consensus/track", "Track consensus (TVCN, TVCX, HCCA, ...)", {"TVCN", "TVCX", "TVCA", "TVCE", "TVCC", "TVDG", "IVCN", "IVCX", "ICON", "CCON", "HCCA", "HCCI", "GFEX", "TCOA", "TCON", "TCCN", "NNIC", "FSSE"}, false, QColor{255, 214, 0}},
        }},
        {"members", "Ensemble members", false, {
            {"members/ifs", "IFS ENS members (ECMWF open data)", {}, false, QColor{255, 150, 60}},
            {"members/aifs", "AIFS ENS members (ECMWF open data)", {}, false, QColor{60, 220, 170}},
            {"members/fnv3", "DeepMind FNV3 members (Google Weather Lab, experimental)", {}, false, QColor{214, 110, 255}},
            {"members/wnv3", "DeepMind WeatherNext 3 members (Google Weather Lab, experimental)", {}, false, QColor{255, 100, 170}},
            {"members/all", "Members (GEFS and others)", {}, false, QColor{140, 165, 255}, false, true},
        }},
        {"simple", "Simple and statistical", false, {
            {"simple/all", "BAM, CLIPER, TABS, SHIPS and the rest", {"TABS", "TABM", "TABD", "TABE", "BAMS", "BAMM", "BAMD", "BAMA", "BAMB", "LBAR", "CLIP", "CLP5", "XTRP", "A98E", "A97E", "TCLP", "NHC5", "OCD5", "BCD5", "SHF5",
                                          "SHIP", "DSHP", "LGEM", "SHFR", "DRCL", "DRCC", "AFWA"}, false, QColor{120, 225, 120}},
        }},
        {"other", "Other guidance", false, {
            {"other/all", "Everything else", {}, false, QColor{190, 190, 190}, true},
        }},
    };
    return all;
}

GuidanceTree::GuidanceTree(QWidget * parent) : QWidget{parent} {
    auto * layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    quick = new QComboBox{this};
    quick->addItems({"Quick select...", "Default: NHC, GFS / Euro, hurricane models, ensemble means", "Default and the consensus aids", "All global and hurricane models", "NHC only", "Everything"});
    layout->addWidget(quick);
    tree = new QTreeWidget{this};
    tree->setHeaderHidden(true);
    tree->setIndentation(14);
    tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // the panel around it scrolls; the tree is as tall as its open lines
    layout->addWidget(tree);
    building = true;
    for (const auto& category : catalog()) {
        auto * top = new QTreeWidgetItem{tree};
        top->setText(0, QString::fromStdString(category.label));
        top->setFlags(top->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        QFont font = top->font(0);
        font.setBold(true);
        top->setFont(0, font);
        for (const auto& leaf : category.leaves) {
            auto * item = new QTreeWidgetItem{top};
            item->setText(0, QString::fromStdString(leaf.label));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setIcon(0, swatch(leaf.color));
            item->setData(0, Qt::UserRole, QString::fromStdString(leaf.id));
            items[leaf.id] = item;
            for (const auto& tech : leaf.techs) {
                byTech[tech] = &leaf;
            }
        }
        top->setExpanded(category.expanded);
    }
    // what was chosen last time, else the default
    std::set<string> on;
    const auto saved = Utility::readPref("HURRICANE_GUIDE", "");
    if (saved.empty()) {
        on = preset(1);
    } else {
        std::stringstream stream{saved};
        string id;
        while (std::getline(stream, id, ',')) {
            on.insert(id);
        }
    }
    apply(on);
    building = false;
    const auto fit = [this] {
        int rows = 0;
        for (int i = 0; i < tree->topLevelItemCount(); i++) {
            const auto * top = tree->topLevelItem(i);
            rows += 1 + (top->isExpanded() ? top->childCount() : 0);
        }
        tree->setFixedHeight(rows * std::max(18, tree->sizeHintForRow(0)) + 8);
    };
    QObject::connect(tree, &QTreeWidget::itemExpanded, [fit] (QTreeWidgetItem *) { fit(); });
    QObject::connect(tree, &QTreeWidget::itemCollapsed, [fit] (QTreeWidgetItem *) { fit(); });
    fit();
    QObject::connect(tree, &QTreeWidget::itemChanged, [this] (QTreeWidgetItem *, int) {
        if (building) {
            return;
        }
        save();
        if (changed) {
            changed();
        }
    });
    QObject::connect(quick, &QComboBox::activated, [this] (int index) {
        if (index > 0) {
            building = true;
            apply(preset(index));
            building = false;
            save();
            if (changed) {
                changed();
            }
        }
        quick->setCurrentIndex(0);
    });
}

std::set<string> GuidanceTree::preset(int index) const {
    std::set<string> on;
    for (const auto& category : catalog()) {
        for (const auto& leaf : category.leaves) {
            bool want = false;
            switch (index) {
                case 1: want = leaf.onByDefault; break;
                case 2: want = leaf.onByDefault || category.id == "consensus"; break;
                case 3: want = category.id == "nhc" || category.id == "global" || category.id == "hurricane"; break;
                case 4: want = category.id == "nhc"; break;
                case 5: want = true; break;
                default: break;
            }
            if (want) {
                on.insert(leaf.id);
            }
        }
    }
    return on;
}

void GuidanceTree::apply(const std::set<string>& on) {
    for (const auto& [id, item] : items) {
        item->setCheckState(0, on.count(id) != 0 ? Qt::Checked : Qt::Unchecked);
    }
}

std::set<string> GuidanceTree::current() const {
    std::set<string> on;
    for (const auto& [id, item] : items) {
        if (item->checkState(0) == Qt::Checked) {
            on.insert(id);
        }
    }
    return on;
}

void GuidanceTree::save() {
    string text;
    for (const auto& id : current()) {
        text += (text.empty() ? "" : ",") + id;
    }
    Utility::writePref("HURRICANE_GUIDE", text.empty() ? "none" : text);   // an empty choice is not "unset"
}

bool GuidanceTree::leafOn(const string& id) const {
    const auto found = items.find(id);
    return found != items.end() && found->second->checkState(0) == Qt::Checked;
}

const GuidanceTree::Leaf * GuidanceTree::leafOf(const string& tech) const {
    const auto found = byTech.find(tech);
    if (found != byTech.end()) {
        return found->second;
    }
    // an ensemble member: AP01, EE02 ... (two letters and two digits)
    const bool member = tech.size() == 4 && std::isdigit(static_cast<unsigned char>(tech[2])) && std::isdigit(static_cast<unsigned char>(tech[3]));
    for (const auto& category : catalog()) {
        for (const auto& leaf : category.leaves) {
            if ((member && leaf.members) || (!member && leaf.catchAll)) {
                return &leaf;
            }
        }
    }
    return nullptr;
}

bool GuidanceTree::shown(const string& tech) const {
    const auto * leaf = leafOf(tech);
    return leaf != nullptr && leafOn(leaf->id);
}

QColor GuidanceTree::colorOf(const string& tech, const QColor& fallback) const {
    const auto * leaf = leafOf(tech);
    return leaf != nullptr && leaf->color.isValid() && !leaf->members ? leaf->color : fallback;
}

void GuidanceTree::setAvailable(const vector<UtilityAtcf::Track>& guidance) {
    counts.clear();
    for (auto it = cycles.begin(); it != cycles.end();) {   // the ATCF leaves are rebuilt from these tracks; the others keep what setCycles gave
        const auto leafIt = items.find(it->first);
        bool atcf = false;
        for (const auto& category : catalog()) {
            for (const auto& leaf : category.leaves) {
                if (leaf.id == it->first && (!leaf.techs.empty() || leaf.catchAll || leaf.members)) {
                    atcf = true;
                }
            }
        }
        (void)leafIt;
        it = atcf ? cycles.erase(it) : std::next(it);
    }
    for (const auto& track : guidance) {
        if (const auto * leaf = leafOf(track.tech)) {
            counts[leaf->id]++;
            if (track.cycle.size() == 10) {
                cycles[leaf->id].insert(track.cycle);
            }
        }
    }
    haveGuidance = !guidance.empty();
    relabel();
}

void GuidanceTree::setCycles(const std::map<string, vector<string>>& given) {
    for (const auto& [id, list] : given) {
        auto& set = cycles[id];
        set.clear();
        for (const auto& cycle : list) {
            if (cycle.size() == 10) {
                set.insert(cycle);
            }
        }
    }
    relabel();
}

// "GFS  (2)  (06z)": the runs available, and the hour of the cycle (newest first; two when the runs are of different cycles)
void GuidanceTree::relabel() {
    building = true;
    for (const auto& category : catalog()) {
        for (const auto& leaf : category.leaves) {
            auto * item = items[leaf.id];
            const int n = counts.count(leaf.id) != 0 ? counts[leaf.id] : 0;
            const bool always = leaf.id == "nhc/official" || (leaf.techs.empty() && !leaf.catchAll && !leaf.members);   // the ones that are not ATCF guidance have no count
            QString text = QString::fromStdString(leaf.label);
            if (!always && haveGuidance) {
                text += n > 0 ? "  (" + QString::number(n) + ")" : QString{"  (none now)"};
            }
            const auto found = cycles.find(leaf.id);
            if (found != cycles.end() && !found->second.empty()) {
                QStringList hours;
                for (auto it = found->second.rbegin(); it != found->second.rend() && hours.size() < 2; ++it) {
                    hours << QString::fromStdString(it->substr(8, 2)) + "z";
                }
                text += "  (" + hours.join(", ") + ")";
            }
            item->setText(0, text);
            item->setToolTip(0, text);   // the whole line, for the ones the panel cuts
            item->setForeground(0, !always && haveGuidance && n == 0 ? QBrush{QColor{140, 140, 140}} : QBrush{});
        }
    }
    building = false;
}
