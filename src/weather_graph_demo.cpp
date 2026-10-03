// Standalone demo with its own main(): compiled only with -DWXQT_WEATHER_GRAPH_DEMO (makeAll.py builds every .cpp
// under src/, and a second main() would break the app's link).
#ifdef WXQT_WEATHER_GRAPH_DEMO

// *****************************************************************************
// * Demo: Creating a graph from UtilityHourly.cpp data
// * This example shows how to visualize hourly weather data
// * NOTE: This file is a standalone demo application and should not be part of the main wxqt build.
// * To build as a standalone app, create a separate project for weather_graph_demo.
// *****************************************************************************

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QMessageBox>
#include "ui/Window.h"
#include "ui/WeatherGraph.h"
#include "ui/VBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "misc/UtilityHourly.h"

class WeatherGraphDemo : public Window {
    Q_OBJECT
public:
    WeatherGraphDemo() : Window(nullptr) {
        setTitle("Weather Graph Demo - Hourly Forecast");
        setSize(1000, 700);

        // Create UI controls
        QHBoxLayout* controlLayout = new QHBoxLayout();
        QLabel* locationLabel = new QLabel("Location Number:");
        locationEdit = new QLineEdit("12345");  // Default location
        loadButton = new QPushButton("Load Weather Data");
        controlLayout->addWidget(locationLabel);
        controlLayout->addWidget(locationEdit);
        controlLayout->addWidget(loadButton);

        // Create status label
        statusLabel = new QLabel("Ready - Enter location number and click Load");
        statusLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");

        // Create graph widget
        graphWidget = new WeatherGraph(this);

        // Create info display
        infoText = new QTextEdit();
        infoText->setReadOnly(true);
        infoText->setPlaceholderText("Weather data and graph will appear here...");

        // Layout
        QVBoxLayout* mainLayout = new QVBoxLayout();
        mainLayout->addLayout(controlLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addWidget(graphWidget, 2); // Give graph more space
        mainLayout->addWidget(infoText);

        QWidget* centralWidget = new QWidget();
        centralWidget->setLayout(mainLayout);
        setCentralWidget(centralWidget);

        // Connect button
        connect(loadButton, &QPushButton::clicked, this, &WeatherGraphDemo::loadWeatherData);

        // Load initial data if possible
        loadWeatherData();
    }

private slots:
    void loadWeatherData() {
        bool ok;
        int locationNumber = locationEdit->text().toInt(&ok);
        
        if (!ok) {
            QMessageBox::warning(this, "Invalid Input", 
                                "Please enter a valid location number.");
            return;
        }

        statusLabel->setText("Loading weather data for location " + QString::number(locationNumber) + "...");
        statusLabel->setStyleSheet("QLabel { color: #0066cc; }");

        // Load weather data and update graph
        const auto points = UtilityHourly::getGraphData(locationNumber);
        if (!points.empty()) {
            graphWidget->setData(points, "location " + std::to_string(locationNumber));
            statusLabel->setText("Successfully loaded weather data for location " + 
                               QString::number(locationNumber));
            statusLabel->setStyleSheet("QLabel { color: #009900; }");
            infoText->setText("Weather graph has been loaded with " +
                             QString::number(points.size()) +
                             " hours.\n\n" +
                             "The graph shows:\n" +
                             "- Temperature (red) and dew point (green) lines\n" +
                             "- Chance of precipitation (blue bars)\n" +
                             "- Wind arrows and speeds under the time axis");
        } else {
            statusLabel->setText("Failed to load weather data for location " + 
                               QString::number(locationNumber));
            statusLabel->setStyleSheet("QLabel { color: #cc0000; }");
            infoText->setText("Unable to load weather data for location " +
                             QString::number(locationNumber) +
                             ".\n\nCheck if the location number is valid and you have network access.");
        }
    }

private:
    QLineEdit* locationEdit;
    QPushButton* loadButton;
    QLabel* statusLabel;
    WeatherGraph* graphWidget;
    QTextEdit* infoText;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    WeatherGraphDemo window;
    window.show();
    return app.exec();
}

#endif  // WXQT_WEATHER_GRAPH_DEMO
