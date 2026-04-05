#pragma once

#include <QWidget>
#include "core/io/WavFile.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QChartView;
class QChart;

class AnalysisWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisWidget(QWidget *parent = nullptr);

    // Called by MainWindow after embedding to auto-populate both file fields
    void loadFiles(const QString &originalPath, const QString &stegoPath);

private slots:
    void browseOriginal();
    void browseStego();
    void onComputeClicked();
    void saveOrigChart();
    void saveStegoChart();
    void saveDiffChart();

private:
    void setupUi();
    bool tryLoadWav(const QString &path, WavFile &wav, QLineEdit *pathEdit, QLabel *infoLabel);
    void updateCharts();
    void setStatus(const QString &text, bool isError = false);
    void saveChartView(QChartView *view, const QString &title);

    QLineEdit *m_origPathEdit;
    QLineEdit *m_stegoPathEdit;
    QLabel *m_origInfoLabel;
    QLabel *m_stegoInfoLabel;
    QPushButton *m_computeBtn;

    QLabel *m_mseValueLabel;
    QLabel *m_nmseValueLabel;
    QLabel *m_statusLabel;

    QPushButton *m_saveOrigBtn;
    QPushButton *m_saveStegoBtn;
    QPushButton *m_saveDiffBtn;

    QChartView *m_origChartView;
    QChartView *m_stegoChartView;
    QChartView *m_diffChartView;

    WavFile m_origWav;
    WavFile m_stegoWav;
};
