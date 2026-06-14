#ifndef CHARTDASHBOARD_H
#define CHARTDASHBOARD_H

#include <QWidget>

#include <QVector>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "logging/SensorLogger.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ChartDashboard; }
QT_END_NAMESPACE

class QButtonGroup;

QT_CHARTS_USE_NAMESPACE

// Màn Dashboard: 3 chart (nhiệt độ / độ ẩm / ánh sáng) vẽ bằng Qt Charts, có
// bộ chọn khoảng thời gian 1h / 6h / 24h. Dữ liệu lấy từ SensorHistory.
class ChartDashboard : public QWidget
{
    Q_OBJECT

public:
    explicit ChartDashboard(SensorLogger *logger, QWidget *parent = nullptr);
    ~ChartDashboard();

public slots:
    // Thêm 1 mẫu mới (t = epoch giây) vào cả 3 chart rồi cập nhật trục.
    void appendSample(qint64 t, double temp, int humi, int lux);

private:
    enum Metric { Temp = 0, Humi = 1, Light = 2, MetricCount = 3 };

    struct ChartItem
    {
        QChart *chart;
        QLineSeries *series;
        QDateTimeAxis *axisX;
        QValueAxis *axisY;
        QChartView *view;
    };

    ChartItem makeChart(const QString &title, const QColor &color);
    void setRangeSeconds(qint64 seconds);
    // Bước gom mẫu (giây) cho từng khoảng thời gian -> số điểm vừa phải, dễ nhìn.
    qint64 stepForRange(qint64 rangeSec) const;
    // Dựng lại 3 series từ buffer thô, gom mẫu theo bucket = step và lấy trung bình.
    void rebuildSeries();
    void refresh();

    Ui::ChartDashboard *ui;
    QButtonGroup *mRangeGroup;
    ChartItem mCharts[MetricCount];
    qint64 mRangeSec;
    QVector<SensorLogger::Sample> mRaw;
};

#endif // CHARTDASHBOARD_H
