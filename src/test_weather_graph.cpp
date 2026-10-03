// *****************************************************************************
// * Test program to demonstrate WeatherGraph functionality
// *****************************************************************************

#include "ui/Window.h"
#include "ui/WeatherGraph.h"
#include "ui/VBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "misc/UtilityHourly.h"

class TestWeatherGraph : public Window {
public:
    TestWeatherGraph() {
        setTitle("Weather Graph Test");
        setSize(800, 600);

        // Create a graph widget
        graphWidget = new WeatherGraph(this);

        // Create UI controls
        HBox* controlRow = new HBox(this);
        Text* statusText = new Text(this);
        statusText->setText("Testing WeatherGraph functionality...");
        ButtonToggle* loadButton = new ButtonToggle(this, "Load Test Data");

        VBox* mainBox = new VBox(this);
        mainBox->addWidgetReal(graphWidget, 1, Qt::Alignment{});
        mainBox->addWidgetReal(controlRow, 0, Qt::Alignment{});

        controlRow->addWidget(statusText, 1);
        controlRow->addWidget(loadButton, 0);

        mainBox->getAndShow(this);

        // Connect button to load test data
        loadButton->connect([this] {
            // Load test data - use a test location
            if (UtilityHourly::getGraphData(12345, graphWidget)) {
                statusText->setText("Successfully loaded weather graph data!");
            } else {
                statusText->setText("Failed to load weather graph data.");
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