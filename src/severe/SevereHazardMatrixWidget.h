#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QColor>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// Data payload representing a single hazard cell (e.g., Tornado Day 1 or ERO Day 2)
struct HazardCellData {
    QString label;              // e.g., "10% Sig", "ENH", "MOD", "30%"
    QColor backgroundColor;     // Background color matching SPC/WPC threat levels
    QColor textColor{Qt::black}; // Text color (white/black depending on contrast)
    QString tooltip;            // Detailed text summary on hover
};

class SevereHazardMatrixWidget : public QWidget {
    Q_OBJECT

public:
    explicit SevereHazardMatrixWidget(QWidget *parent = nullptr);
    ~SevereHazardMatrixWidget() override = default;

    // Row definitions matching SPC & WPC categories
    enum RowIndex {
        RowTornado = 0,
        RowWind,
        RowHail,
        RowAnySevere,
        RowRainfallERO,
        RowHeavySnow,
        RowFreezingRain,
        RowCount
    };

    // Update a single cell in the Day 1-8 grid
    void updateHazardCell(RowIndex row, int dayColumn, const HazardCellData &data);

    // Batch update matrix from SPC & WPC GeoJSON payload
    void populateFromGeoJson(const QJsonObject &spcData, const QJsonObject &wpcData);

    // Reset matrix values to default empty state
    void clearMatrix();

    // Helper to return official SPC/WPC threat QColor
    static QColor getSpcCategoricalColor(const QString &category);
    static QColor getWpcEroColor(const QString &category);
    static QColor getProbabilisticColor(int percentage);

signals:
    void cellClicked(int row, int dayColumn);

private:
    void setupUi();
    void configureTableFormatting();

    QTableWidget *m_tableWidget{nullptr};
};
