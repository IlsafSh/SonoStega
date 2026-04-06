#pragma once

#include <QWidget>
#include "core/io/WavFile.h"

class QLineEdit;
class QLabel;
class QSlider;
class QRadioButton;
class QPushButton;
class QTextEdit;
class QComboBox;

class ExtractWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExtractWidget(QWidget *parent = nullptr);

    // Called by MainWindow when EmbedWidget finishes — auto-fills the stego path
    void loadStegoFile(const QString &path);

private slots:
    void browseWav();
    void onNBitsChanged(int value);
    void onModeChanged();
    void onExtractClicked();
    void onSaveClicked();

private:
    void setupUi();
    void updateWavInfo();
    void setStatus(const QString &text, bool isError = false);

    QLineEdit *m_wavPathEdit;
    QLabel *m_wavInfoLabel;
    QSlider *m_nBitsSlider;
    QLabel *m_nBitsValueLabel;
    QRadioButton *m_seqRadio;
    QRadioButton *m_keyedRadio;
    QComboBox *m_encodingCombo;
    QWidget *m_passwordRow;
    QLineEdit *m_passwordEdit;
    QPushButton *m_extractBtn;
    QTextEdit *m_previewEdit;
    QLabel *m_msgInfoLabel;
    QPushButton *m_saveBtn;
    QLabel *m_statusLabel;

    WavFile m_wav;
    QByteArray m_extractedPayload;
};
