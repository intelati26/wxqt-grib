#include <string>
#include <unordered_map>
#include <vector>
#include "ui/WeatherGraph.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityHourly {
public:
    static string get(int);
    static bool getGraphData(int locationNumber, WeatherGraph* graphWidget);
    static string getGraphJson(int locationNumber);   // the download, safe off the GUI thread
    static bool fillGraph(const string& json, int locationNumber, WeatherGraph* graphWidget);   // GUI thread

private:
    static const unordered_map<string, string> hourlyAbbreviations;
    static string getFooter();
    static string getHourlyString(int);
    static string parse(const string&);
    static string shortenConditions(const string&);
    static vector<string> parseColumn(const string& html, const string& pattern);
};
