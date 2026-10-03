// Standalone demo with its own main(): compiled only with -DWXQT_WEATHER_GRAPH_DEMO (makeAll.py builds every .cpp
// under src/, and a second main() would break the app's link).
#ifdef WXQT_WEATHER_GRAPH_DEMO

// *****************************************************************************
// * Example: Creating a weather graph from UtilityHourly.cpp
// * This demonstrates how to use the WeatherGraph widget with UtilityHourly data
// *****************************************************************************

#include "ui/Window.h"
#include "ui/WeatherGraph.h"
#include "ui/VBox.h"
#include "misc/UtilityHourly.h"
#include <QApplication>

class WeatherGraphExample : public Window {
public:
    WeatherGraphExample() : Window(nullptr) {
        setTitle("Weather Graph Example");
        setSize(800, 600);

        auto* graph = new WeatherGraph(this);

        VBox layout;
        layout.addWidgetReal(graph, 1, Qt::Alignment{});
        layout.getAndShow(this);

        // Load weather data and update graph
        graph->setData(UtilityHourly::getGraphData(0), "location 1");   // location index 0 = the first saved location
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    WeatherGraphExample window;
    window.show();
    return app.exec();
}

#endif  // WXQT_WEATHER_GRAPH_DEMO
