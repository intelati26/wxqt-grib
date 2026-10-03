// *****************************************************************************
// * Integration: Adding WeatherGraph to MainWindow Home Screen
// * This implementation adds the WeatherGraph widget to the MainWindow's
// * forecast section so hourly weather data can be displayed on the home screen.
// *****************************************************************************

#include "ui/MainWindow.h"
#include "ui/WeatherGraph.h"
#include "misc/UtilityHourly.h"

// Update MainWindow.h to add WeatherGraph widget

// Add to MainWindow class in MainWindow.h:
// private:
//     WeatherGraph hourlyGraph;  // Add this line after other widgets

// Update MainWindow.cpp to integrate WeatherGraph

// In MainWindow constructor:
// , hourlyGraph{this}  // Add this after other widget initializations

// In MainWindow.cpp, add to addWidgets() function:
// forecastLayout.addWidget(&hourlyGraph, 0, Qt::AlignTop);
// hourlyGraph.setFixedHeight(300);  // Set reasonable height

// In MainWindow.cpp, add to reload() function:
// new FutureVoid{this, [this] { getHourlyGraphData(); }, [this] { updateHourlyGraph(); }};

// Add private methods:
// void getHourlyGraphData();
// void updateHourlyGraph();

// Also update the toolbar shortcut for hourly to show the graph:
// shortcutHourly.connect([this] { showHourlyGraph(); });

// Add showHourlyGraph() method:
// void showHourlyGraph() {
//     // Toggle visibility of hourlyGraph widget
//     hourlyGraph.setVisible(!hourlyGraph.isVisible());
// }

// In MainWindow.cpp, add the implementations:

void MainWindow::getHourlyGraphData() {
    hourlyGraph.setData(UtilityHourly::getHourlyWeatherData(Location::getCurrentLocation()));
}

void MainWindow::updateHourlyGraph() {
    hourlyGraph.update();
}

void MainWindow::showHourlyGraph() {
    hourlyGraph.setVisible(!hourlyGraph.isVisible());
    hourlyGraph.raise();
    hourlyGraph.activateWindow();
}