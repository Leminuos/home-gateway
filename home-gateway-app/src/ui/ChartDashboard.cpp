#include "ChartDashboard.h"
#include "./ui_ChartDashboard.h"

#include "logging/SensorLogger.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QScroller>
#include <QScrollerProperties>
#include <QVBoxLayout>

#include <limits>

static const qint64 kHour = 3600;

ChartDashboard::ChartDashboard(SensorLogger *logger, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChartDashboard)
    , mRangeGroup(new QButtonGroup(this))
    , mRangeSec(1 * kHour)
{
    ui->setupUi(this);

    mCharts[Temp] = makeChart(QStringLiteral("Temperature (°C)"), QColor("#e6755b"));
    mCharts[Humi] = makeChart(QStringLiteral("Humidity (%)"), QColor("#4f9ad6"));
    mCharts[Light] = makeChart(QStringLiteral("Light (lx)"), QColor("#e0b341"));

    for (int i = 0; i < MetricCount; ++i) {
        ui->chartsLayout->addWidget(mCharts[i].view);
    }

    // Kéo bằng ngón tay để scroll (tslib). Chart không tương tác nên cho trong
    // suốt với chuột để gesture truyền tới viewport của scroll area.
    QScroller::grabGesture(ui->chartScroll->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller *scroller = QScroller::scroller(ui->chartScroll->viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::AxisLockThreshold, 1.0);
    props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.5);
    props.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0);
    props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0);
    scroller->setScrollerProperties(props);

    mRangeGroup->setExclusive(true);
    mRangeGroup->addButton(ui->range1hButton, 1);
    mRangeGroup->addButton(ui->range6hButton, 6);
    mRangeGroup->addButton(ui->range24hButton, 24);
    QObject::connect(mRangeGroup, &QButtonGroup::idClicked, [this](int hours) {
        setRangeSeconds((qint64)hours * kHour);
    });

    // Nạp lịch sử sẵn có vào buffer thô để vẽ ngay khi mở app.
    if (logger) {
        mRaw = logger->samples();
    }

    refresh();
}

ChartDashboard::~ChartDashboard()
{
    delete ui;
}

ChartDashboard::ChartItem ChartDashboard::makeChart(const QString &title, const QColor &color)
{
    ChartItem item;

    item.series = new QLineSeries();
    QPen pen(color);
    pen.setWidth(2);
    item.series->setPen(pen);

    QFont titleFont;
    titleFont.setPointSize(6);
    titleFont.setBold(true);

    QFont labelFont;
    labelFont.setPointSize(5);

    item.chart = new QChart();
    item.chart->addSeries(item.series);
    item.chart->legend()->hide();
    item.chart->setTitle(title);
    item.chart->setTitleFont(titleFont);
    item.chart->setMargins(QMargins(0, 0, 0, 0));
    item.chart->setContentsMargins(0, 0, 0, 0);
    item.chart->setBackgroundRoundness(6);

    item.axisX = new QDateTimeAxis();
    item.axisX->setTickCount(3);
    item.axisX->setFormat(QStringLiteral("hh:mm"));
    item.axisX->setLabelsFont(labelFont);
    item.chart->addAxis(item.axisX, Qt::AlignBottom);
    item.series->attachAxis(item.axisX);

    item.axisY = new QValueAxis();
    item.axisY->setTickCount(4);
    item.axisY->setLabelFormat(QStringLiteral("%.0f"));
    item.axisY->setLabelsFont(labelFont);
    item.chart->addAxis(item.axisY, Qt::AlignLeft);
    item.series->attachAxis(item.axisY);

    item.view = new QChartView(item.chart);
    item.view->setRenderHint(QPainter::Antialiasing);
    item.view->setMinimumHeight(190);
    item.view->setFrameShape(QFrame::NoFrame);
    item.view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    item.view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    item.view->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    item.view->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    return item;
}

void ChartDashboard::setRangeSeconds(qint64 seconds)
{
    mRangeSec = seconds;
    refresh();
}

void ChartDashboard::appendSample(qint64 t, double temp, int humi, int lux)
{
    mRaw.append(SensorLogger::Sample{t, temp, humi, lux});

    // Giữ buffer trong 24h (khoảng dài nhất) để bounded bộ nhớ.
    const qint64 cutoff = t - 24 * kHour;
    while (!mRaw.isEmpty() && mRaw.first().t < cutoff) {
        mRaw.removeFirst();
    }

    refresh();
}

qint64 ChartDashboard::stepForRange(qint64 rangeSec) const
{
    if (rangeSec <= 1 * kHour) {
        return 120;     // 1h  -> 2 phút/điểm
    }
    if (rangeSec <= 6 * kHour) {
        return 600;     // 6h  -> 10 phút/điểm
    }
    return 1800;        // 24h -> 30 phút/điểm
}

void ChartDashboard::rebuildSeries()
{
    for (int i = 0; i < MetricCount; ++i) {
        mCharts[i].series->clear();
    }

    const qint64 nowSec = QDateTime::currentSecsSinceEpoch();
    const qint64 fromSec = nowSec - mRangeSec;
    const qint64 step = stepForRange(mRangeSec);

    qint64 curBucket = std::numeric_limits<qint64>::min();
    double sumT = 0, sumH = 0, sumL = 0;
    int cnt = 0;

    auto flush = [&]() {
        if (cnt > 0) {
            const qreal x = (qreal)(curBucket * 1000);
            mCharts[Temp].series->append(x, sumT / cnt);
            mCharts[Humi].series->append(x, sumH / cnt);
            mCharts[Light].series->append(x, sumL / cnt);
        }
    };

    // mRaw đã theo thứ tự thời gian -> gom mẫu cùng bucket bằng một lượt quét.
    for (const SensorLogger::Sample &s : mRaw) {
        if (s.t < fromSec || s.t > nowSec) {
            continue;
        }
        const qint64 bucket = (s.t / step) * step;
        if (bucket != curBucket) {
            flush();
            curBucket = bucket;
            sumT = sumH = sumL = 0;
            cnt = 0;
        }
        sumT += s.temp;
        sumH += s.humi;
        sumL += s.lux;
        ++cnt;
    }
    flush();
}

void ChartDashboard::refresh()
{
    rebuildSeries();

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 fromMs = nowMs - mRangeSec * 1000;

    for (int i = 0; i < MetricCount; ++i) {
        ChartItem &c = mCharts[i];
        c.axisX->setRange(QDateTime::fromMSecsSinceEpoch(fromMs),
                          QDateTime::fromMSecsSinceEpoch(nowMs));

        // Auto-range trục Y theo các điểm nằm trong cửa sổ thời gian.
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        const QList<QPointF> points = c.series->points();
        for (const QPointF &p : points) {
            if (p.x() < fromMs) {
                continue;
            }
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }

        if (minY > maxY) {
            // Chưa có điểm trong cửa sổ -> dải mặc định.
            minY = 0.0;
            maxY = 1.0;
        }

        double pad = (maxY - minY) * 0.1;
        if (pad < 1.0) {
            pad = 1.0;
        }
        c.axisY->setRange(minY - pad, maxY + pad);
    }
}
