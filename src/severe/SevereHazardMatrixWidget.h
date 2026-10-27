#pragma once

#include <memory>
#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QColor>
#include <QString>

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
    ~SevereHazardMatrixWidget() override;

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

    // Read the SPC and WPC outlooks (the highest level of each, anywhere in the country) from the National Weather Service's map services (in the background) and fill the grid
    void reload();

    // Reset matrix values to default empty state
    void clearMatrix();

    // Helper to return official SPC/WPC threat QColor
    static QColor getSpcCategoricalColor(const QString &category);
    static QColor getWpcEroColor(const QString &category);
    static QColor getProbabilisticColor(int percentage);

signals:
    void loaded(const QString &problem);   // (a text when nothing could be read)
    void cellClicked(int row, int dayColumn);

private:
    void setupUi();
    void configureTableFormatting();

    QTableWidget *m_tableWidget{nullptr};
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};   // (cleared with the widget: a background read that ends later does not touch it)
};
