#include "ui/analysis/AnalysisWidget.h"
#include "core/analysis/QualityAnalyzer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QPen>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>

#include <algorithm>

AnalysisWidget::AnalysisWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void AnalysisWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // --- File pickers group -------------------------------------------------
    auto *filesGroup = new QGroupBox("Файлы для сравнения");
    auto *filesLayout = new QFormLayout(filesGroup);
    filesLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto makeFileRow = [&](QLineEdit *&edit, QLabel *&info, const char *placeholder) {
        auto *row = new QHBoxLayout();
        edit = new QLineEdit();
        edit->setReadOnly(true);
        edit->setPlaceholderText(placeholder);
        auto *btn = new QPushButton("Обзор...");
        btn->setFixedWidth(90);
        row->addWidget(edit);
        row->addWidget(btn);
        info = new QLabel("—");
        info->setStyleSheet("color: gray;");
        return std::make_pair(row, btn);
    };

    auto [origRow, origBtn] = makeFileRow(m_origPathEdit, m_origInfoLabel, "Оригинальный WAV-файл...");
    auto [stegoRow, stegoBtn] = makeFileRow(m_stegoPathEdit, m_stegoInfoLabel, "Стего WAV-файл...");

    filesLayout->addRow("Оригинал:", origRow);
    filesLayout->addRow("", m_origInfoLabel);
    filesLayout->addRow("Стего:", stegoRow);
    filesLayout->addRow("", m_stegoInfoLabel);

    // --- Compute button ------------------------------------------------------
    m_computeBtn = new QPushButton("Вычислить MSE / NMSE и построить графики");
    m_computeBtn->setFixedHeight(36);
    m_computeBtn->setEnabled(false);

    // --- Metrics group -------------------------------------------------------
    auto *metricsGroup = new QGroupBox("Метрики качества");
    auto *metricsLayout = new QFormLayout(metricsGroup);

    m_mseValueLabel  = new QLabel("—");
    m_nmseValueLabel = new QLabel("—");
    m_mseValueLabel->setStyleSheet("font-weight: bold;");
    m_nmseValueLabel->setStyleSheet("font-weight: bold;");

    metricsLayout->addRow("MSE:", m_mseValueLabel);
    metricsLayout->addRow("NMSE:", m_nmseValueLabel);

    // --- Charts in a scroll area --------------------------------------------
    auto *chartsWidget = new QWidget();
    auto *chartsLayout = new QVBoxLayout(chartsWidget);
    chartsLayout->setSpacing(6);

    m_origChartView  = new QChartView(new QChart());
    m_stegoChartView = new QChartView(new QChart());
    m_diffChartView  = new QChartView(new QChart());

    for (auto *cv : {m_origChartView, m_stegoChartView, m_diffChartView}) {
        cv->setRenderHint(QPainter::Antialiasing);
        cv->setMinimumHeight(180);
        chartsLayout->addWidget(cv);
    }

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidget(chartsWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // --- Status label -------------------------------------------------------
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // --- Assemble -----------------------------------------------------------
    mainLayout->addWidget(filesGroup);
    mainLayout->addWidget(m_computeBtn);
    mainLayout->addWidget(metricsGroup);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addWidget(m_statusLabel);

    // --- Connections --------------------------------------------------------
    connect(origBtn,  &QPushButton::clicked, this, &AnalysisWidget::browseOriginal);
    connect(stegoBtn, &QPushButton::clicked, this, &AnalysisWidget::browseStego);
    connect(m_computeBtn, &QPushButton::clicked, this, &AnalysisWidget::onComputeClicked);
}

bool AnalysisWidget::tryLoadWav(const QString &path, WavFile &wav,
                                 QLineEdit *pathEdit, QLabel *infoLabel)
{
    wav = WavFile();
    if (!wav.load(path)) {
        setStatus("Ошибка загрузки: " + wav.errorString(), true);
        pathEdit->clear();
        infoLabel->setText("—");
        infoLabel->setStyleSheet("color: gray;");
        return false;
    }
    pathEdit->setText(path);
    int sec = static_cast<int>(wav.durationSeconds());
    infoLabel->setText(QString("%1  |  %2:%3")
                           .arg(wav.formatDescription())
                           .arg(sec / 60)
                           .arg(sec % 60, 2, 10, QChar('0')));
    infoLabel->setStyleSheet("color: black;");
    return true;
}

void AnalysisWidget::loadFiles(const QString &originalPath, const QString &stegoPath)
{
    bool ok = tryLoadWav(originalPath, m_origWav, m_origPathEdit, m_origInfoLabel);
    ok &= tryLoadWav(stegoPath, m_stegoWav, m_stegoPathEdit, m_stegoInfoLabel);
    m_computeBtn->setEnabled(ok);
    if (ok) {
        setStatus({});
        onComputeClicked();
    }
}

void AnalysisWidget::browseOriginal()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Выберите оригинальный WAV", {},
        "WAV-файлы (*.wav);;Все файлы (*)");
    if (path.isEmpty()) return;

    if (tryLoadWav(path, m_origWav, m_origPathEdit, m_origInfoLabel))
        setStatus({});
    m_computeBtn->setEnabled(m_origWav.isValid() && m_stegoWav.isValid());
}

void AnalysisWidget::browseStego()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Выберите стего WAV", {},
        "WAV-файлы (*.wav);;Все файлы (*)");
    if (path.isEmpty()) return;

    if (tryLoadWav(path, m_stegoWav, m_stegoPathEdit, m_stegoInfoLabel))
        setStatus({});
    m_computeBtn->setEnabled(m_origWav.isValid() && m_stegoWav.isValid());
}

void AnalysisWidget::onComputeClicked()
{
    auto metrics = QualityAnalyzer::compute(m_origWav, m_stegoWav);

    if (!metrics.valid) {
        setStatus("Ошибка вычисления метрик: " + metrics.error, true);
        return;
    }

    m_mseValueLabel->setText(QString::number(metrics.mse, 'g', 8));
    m_nmseValueLabel->setText(QString::number(metrics.nmse, 'g', 8));

    if (!metrics.error.isEmpty())
        setStatus(metrics.error, false);  // non-fatal warning
    else
        setStatus("Метрики вычислены успешно.");

    updateCharts();
}

void AnalysisWidget::updateCharts()
{
    const auto &origS  = m_origWav.samples();
    const auto &stegoS = m_stegoWav.samples();
    const int numCh = m_origWav.header().numChannels;
    const int sr    = m_origWav.header().sampleRate;

    constexpr int MAX_PTS = 2000;
    size_t totalFrames = origS.size() / numCh;
    size_t step = std::max<size_t>(1, totalFrames / MAX_PTS);

    // Builds a series from sample array (channel 0 only)
    auto buildSeries = [&](const std::vector<int32_t> &s, QColor color) {
        auto *series = new QLineSeries();
        QPen pen(color);
        pen.setWidth(1);
        series->setPen(pen);
        for (size_t i = 0; i < totalFrames; i += step)
            series->append(static_cast<double>(i) / sr, s[i * numCh]);
        return series;
    };

    // Helper to assemble a chart from a single series
    auto makeChart = [](QLineSeries *series, const QString &title) {
        auto *chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(title);
        chart->legend()->hide();
        chart->createDefaultAxes();
        chart->axes(Qt::Horizontal).first()->setTitleText("Время (с)");
        chart->axes(Qt::Vertical).first()->setTitleText("Амплитуда");
        chart->setMargins(QMargins(4, 4, 4, 4));
        return chart;
    };

    m_origChartView->setChart(
        makeChart(buildSeries(origS, QColor(30, 120, 200)), "Оригинальный сигнал"));

    m_stegoChartView->setChart(
        makeChart(buildSeries(stegoS, QColor(50, 160, 80)), "Стего-сигнал"));

    // Difference signal
    auto *diffSeries = new QLineSeries();
    QPen diffPen(QColor(200, 60, 60));
    diffPen.setWidth(1);
    diffSeries->setPen(diffPen);
    size_t diffFrames = std::min(origS.size(), stegoS.size()) / numCh;
    size_t diffStep = std::max<size_t>(1, diffFrames / MAX_PTS);
    for (size_t i = 0; i < diffFrames; i += diffStep) {
        double t = static_cast<double>(i) / sr;
        int d = static_cast<int>(stegoS[i * numCh]) - static_cast<int>(origS[i * numCh]);
        diffSeries->append(t, d);
    }

    auto *diffChart = new QChart();
    diffChart->addSeries(diffSeries);
    diffChart->setTitle("Разностный сигнал (стего − оригинал)");
    diffChart->legend()->hide();
    diffChart->createDefaultAxes();
    diffChart->axes(Qt::Horizontal).first()->setTitleText("Время (с)");
    diffChart->axes(Qt::Vertical).first()->setTitleText("Δ Амплитуда");
    diffChart->setMargins(QMargins(4, 4, 4, 4));
    m_diffChartView->setChart(diffChart);
}

void AnalysisWidget::setStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? "color: red;" : "color: green;");
}
