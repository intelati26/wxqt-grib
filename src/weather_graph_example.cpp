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
        if (UtilityHourly::getGraphData(12345, graph)) {  // Replace with actual location number
            graph->show();
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    WeatherGraphExample window;
    window.show();
    return app.exec();
}