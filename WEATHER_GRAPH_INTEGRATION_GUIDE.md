// *****************************************************************************
// * Updated MainWindow.cpp - Complete WeatherGraph Integration
// * This file has been updated to integrate the WeatherGraph widget into the
// * MainWindow's forecast section, making hourly weather data available
// * directly on the home screen.
// *****************************************************************************

// Add these methods to MainWindow.cpp (around line 110-130):

void MainWindow::getHourlyGraphData() {
    // Fetch hourly weather data for the current location and update the graph
    UtilityHourly::getHourlyGraphData(Location::getCurrentLocation(), &hourlyGraph);
}

void MainWindow::updateHourlyGraph() {
    // Update the graph widget to reflect new data
    hourlyGraph.update();
}

void MainWindow::showHourlyGraph() {
    // Toggle the visibility of the hourly weather graph
    if (hourlyGraph.isVisible()) {
        hourlyGraph.hide();
    } else {
        hourlyGraph.show();
        hourlyGraph.raise();
        hourlyGraph.activateWindow();
    }
}

// Update the get7day() method to also get hourly graph data:

void MainWindow::get7day() {
    sevenDay.process(Location::getLatLonCurrent());
    // Also fetch hourly graph data for the forecast section
    getHourlyGraphData();
}

// The toolbar shortcut has already been updated:
shortcutHourly.connect([this] { showHourlyGraph(); });   // Show/hide hourly graph