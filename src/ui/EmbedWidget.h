#pragma once

#include <QWidget>
#include "core/WavFile.h"

class QLineEdit;
class QLabel;
class QSlider;
class QRadioButton;
class QPushButton;

class EmbedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EmbedWidget(QWidget *parent = nullptr);

signals:
    // Emitted after successful embedding so AnalysisWidget can auto-load both files
    void embedded(const QString &originalPath, const QString &stegoPath);

private slots:
    void browseWav();
    void browseMsg();
    void onNBitsChanged(int value);
    void onModeChanged();
    void onEmbedClicked();

private:
    void setupUi();
    void updateWavInfo();
    void updateCapacityLabel();
    void setStatus(const QString &text, bool isError = false);

    QLineEdit *m_wavPathEdit;
    QLineEdit *m_msgPathEdit;
    QLabel *m_wavInfoLabel;
    QLabel *m_msgInfoLabel;
    QLabel *m_capacityLabel;
    QSlider *m_nBitsSlider;
    QLabel *m_nBitsValueLabel;
    QRadioButton *m_seqRadio;
    QRadioButton *m_keyedRadio;
    QWidget *m_passwordRow;
    QLineEdit *m_passwordEdit;
    QPushButton *m_embedBtn;
    QLabel *m_statusLabel;

    WavFile m_wav;
    qint64 m_msgSize = 0;
};
