// *****************************************************************************
// * Simple test program to demonstrate WeatherGraph functionality
// * Uses wxqt-grib's existing UI components correctly
// *****************************************************************************

#include <QApplication>
#include "ui/Window.h"
#include "ui/WeatherGraph.h"
#include "ui/VBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Button.h"
#include "misc/UtilityHourly.h"

class TestWeatherGraph : public Window {
public:
    TestWeatherGraph() {
        setTitle("Weather Graph Test");
        setSize(800, 600);

        // Create a graph widget using wxqt-grib's constructor pattern
        graphWidget = new WeatherGraph(this);

        // Create UI controls using wxqt-grib's patterns
        VBox mainBox(this);
        mainBox.addWidgetReal(graphWidget, 1, Qt::AlignTop | Qt::AlignCenter);

        HBox controlRow(this);
        Text statusText(this);
        statusText.setText("Testing WeatherGraph functionality...");
        Button loadButton(this, "Load Test Data");

        // Add widgets using wxqt-grib's API
        controlRow.addWidget(statusText, 1);
        controlRow.addWidget(loadButton, 0);

        // Arrange layout using wxqt-grib's pattern
        mainBox.addWidgetReal(&controlRow, 0, Qt::AlignTop | Qt::AlignCenter);
        mainBox.getAndShow(this);

        // Connect button to load test data
        loadButton.connect([this] {
            if (UtilityHourly::getGraphData(12345, graphWidget)) {
                statusText.setText("Successfully loaded weather graph data!");
            } else {
                statusText.setText("Failed to load weather graph data.");
            }
        });
    }

private:
    WeatherGraph* graphWidget;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestWeatherGraph window;
    window.show();
    return app.exec();
}