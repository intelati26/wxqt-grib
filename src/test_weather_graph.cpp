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

        // Create a graph widget
        graphWidget = new WeatherGraph(this);

        // Create UI controls
        HBox controlRow;
        Button loadButton(this, "Load Test Data");
        Text statusText(this);
        statusText.setText("Testing WeatherGraph functionality...");

        // Add widgets
        controlRow.addWidget(statusText, 1);
        controlRow.addWidget(loadButton, 0);

        // Create main VBox and add components
        VBox mainBox;
        mainBox.addWidgetReal(graphWidget, 1);
        mainBox.addLayout(controlRow, 0);
        mainBox.getAndShow(this);

        // Store pointer to statusText for lambda access
        statusTextPtr = &statusText;
        
        // Connect button to load test data
        loadButton.connect([this] {
            if (UtilityHourly::getGraphData(12345, graphWidget)) {
                statusTextPtr->setText("Successfully loaded weather graph data!");
            } else {
                statusTextPtr->setText("Failed to load weather graph data.");
            }
        });
    }

private:
    WeatherGraph* graphWidget;
    Text* statusTextPtr;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestWeatherGraph window;
    window.show();
    return app.exec();
}