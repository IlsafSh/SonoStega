#include "ui/extract/ExtractWidget.h"
#include "core/steganography/LsbEmbedder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QSlider>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>

ExtractWidget::ExtractWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void ExtractWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // --- Stego WAV group ----------------------------------------------------
    auto *wavGroup = new QGroupBox("Стего-файл (WAV)");
    auto *wavLayout = new QVBoxLayout(wavGroup);

    auto *wavRow = new QHBoxLayout();
    m_wavPathEdit = new QLineEdit();
    m_wavPathEdit->setReadOnly(true);
    m_wavPathEdit->setPlaceholderText("Выберите стего WAV-файл...");
    auto *wavBrowseBtn = new QPushButton("Обзор...");
    wavBrowseBtn->setFixedWidth(90);
    wavRow->addWidget(m_wavPathEdit);
    wavRow->addWidget(wavBrowseBtn);

    m_wavInfoLabel = new QLabel("—");
    m_wavInfoLabel->setStyleSheet("color: gray;");

    wavLayout->addLayout(wavRow);
    wavLayout->addWidget(m_wavInfoLabel);

    // --- Parameters group ---------------------------------------------------
    auto *paramGroup = new QGroupBox("Параметры извлечения");
    auto *paramLayout = new QVBoxLayout(paramGroup);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto *sliderRow = new QHBoxLayout();
    m_nBitsSlider = new QSlider(Qt::Horizontal);
    m_nBitsSlider->setRange(1, 4);
    m_nBitsSlider->setValue(1);
    m_nBitsSlider->setTickPosition(QSlider::TicksBelow);
    m_nBitsSlider->setTickInterval(1);
    m_nBitsValueLabel = new QLabel("1");
    m_nBitsValueLabel->setFixedWidth(20);
    sliderRow->addWidget(m_nBitsSlider);
    sliderRow->addWidget(m_nBitsValueLabel);
    form->addRow("Количество LSB (N):", sliderRow);

    auto *modeRow = new QHBoxLayout();
    m_seqRadio = new QRadioButton("Последовательный");
    m_keyedRadio = new QRadioButton("С паролем");
    m_seqRadio->setChecked(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_seqRadio);
    modeGroup->addButton(m_keyedRadio);
    modeRow->addWidget(m_seqRadio);
    modeRow->addWidget(m_keyedRadio);
    modeRow->addStretch();
    form->addRow("Режим:", modeRow);

    paramLayout->addLayout(form);

    m_passwordRow = new QWidget();
    auto *pwLayout = new QHBoxLayout(m_passwordRow);
    pwLayout->setContentsMargins(0, 0, 0, 0);
    pwLayout->addWidget(new QLabel("Пароль:"));
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Введите пароль...");
    pwLayout->addWidget(m_passwordEdit);
    m_passwordRow->setVisible(false);
    paramLayout->addWidget(m_passwordRow);

    // --- Extract button ------------------------------------------------------
    m_extractBtn = new QPushButton("Извлечь");
    m_extractBtn->setFixedHeight(36);
    m_extractBtn->setEnabled(false);

    // --- Result group -------------------------------------------------------
    auto *resultGroup = new QGroupBox("Извлечённое сообщение");
    auto *resultLayout = new QVBoxLayout(resultGroup);

    m_previewEdit = new QTextEdit();
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setPlaceholderText("Здесь появится извлечённый текст...");
    m_previewEdit->setMinimumHeight(150);

    auto *resultFooter = new QHBoxLayout();
    m_msgInfoLabel = new QLabel("—");
    m_msgInfoLabel->setStyleSheet("color: gray;");
    m_saveBtn = new QPushButton("Сохранить как TXT...");
    m_saveBtn->setEnabled(false);
    resultFooter->addWidget(m_msgInfoLabel);
    resultFooter->addStretch();
    resultFooter->addWidget(m_saveBtn);

    resultLayout->addWidget(m_previewEdit);
    resultLayout->addLayout(resultFooter);

    // --- Status label -------------------------------------------------------
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // --- Assemble -----------------------------------------------------------
    mainLayout->addWidget(wavGroup);
    mainLayout->addWidget(paramGroup);
    mainLayout->addWidget(m_extractBtn);
    mainLayout->addWidget(resultGroup);
    mainLayout->addWidget(m_statusLabel);

    // --- Connections --------------------------------------------------------
    connect(wavBrowseBtn, &QPushButton::clicked, this, &ExtractWidget::browseWav);
    connect(m_nBitsSlider, &QSlider::valueChanged, this, &ExtractWidget::onNBitsChanged);
    connect(m_seqRadio, &QRadioButton::toggled, this, &ExtractWidget::onModeChanged);
    connect(m_keyedRadio, &QRadioButton::toggled, this, &ExtractWidget::onModeChanged);
    connect(m_extractBtn, &QPushButton::clicked, this, &ExtractWidget::onExtractClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &ExtractWidget::onSaveClicked);
}

void ExtractWidget::loadStegoFile(const QString &path)
{
    m_wav = WavFile();
    if (!m_wav.load(path)) {
        setStatus("Ошибка загрузки WAV: " + m_wav.errorString(), true);
        return;
    }
    m_wavPathEdit->setText(path);
    updateWavInfo();
    setStatus({});
}

void ExtractWidget::browseWav()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Выберите стего WAV-файл", {},
        "WAV-файлы (*.wav);;Все файлы (*)");

    if (!path.isEmpty())
        loadStegoFile(path);
}

void ExtractWidget::updateWavInfo()
{
    if (!m_wav.isValid())
        return;

    // Widen slider range for 24-bit (noise floor is ~8 bits deep)
    int maxBits = (m_wav.header().bitsPerSample == 24) ? 8 : 4;
    m_nBitsSlider->setMaximum(maxBits);
    if (m_nBitsSlider->value() > maxBits)
        m_nBitsSlider->setValue(maxBits);

    int totalSec = static_cast<int>(m_wav.durationSeconds());
    QString dur = QString("%1:%2").arg(totalSec / 60).arg(totalSec % 60, 2, 10, QChar('0'));

    m_wavInfoLabel->setText(QString("Формат: %1  |  Длительность: %2")
                                .arg(m_wav.formatDescription(), dur));
    m_wavInfoLabel->setStyleSheet("color: black;");
    m_extractBtn->setEnabled(true);
}

void ExtractWidget::onNBitsChanged(int value)
{
    m_nBitsValueLabel->setText(QString::number(value));
}

void ExtractWidget::onModeChanged()
{
    m_passwordRow->setVisible(m_keyedRadio->isChecked());
}

void ExtractWidget::onExtractClicked()
{
    if (!m_wav.isValid()) {
        setStatus("Сначала загрузите стего WAV-файл.", true);
        return;
    }
    if (m_keyedRadio->isChecked() && m_passwordEdit->text().isEmpty()) {
        setStatus("Введите пароль для режима 'С паролем'.", true);
        return;
    }

    int n = m_nBitsSlider->value();
    LsbEmbedder::Result res;

    if (m_keyedRadio->isChecked())
        res = LsbEmbedder::extractKeyed(m_wav, m_extractedPayload, n, m_passwordEdit->text());
    else
        res = LsbEmbedder::extract(m_wav, m_extractedPayload, n);

    if (!res.ok) {
        setStatus("Ошибка извлечения: " + res.error, true);
        m_previewEdit->clear();
        m_saveBtn->setEnabled(false);
        m_msgInfoLabel->setText("—");
        return;
    }

    // Display extracted text (try UTF-8, fall back to Latin-1)
    QString text = QString::fromUtf8(m_extractedPayload);
    m_previewEdit->setPlainText(text);

    qint64 size = m_extractedPayload.size();
    if (size < 1024)
        m_msgInfoLabel->setText(QString("Извлечено: %1 байт").arg(size));
    else
        m_msgInfoLabel->setText(QString("Извлечено: %1 КБ").arg(size / 1024.0, 0, 'f', 1));

    m_msgInfoLabel->setStyleSheet("color: black;");
    m_saveBtn->setEnabled(true);
    setStatus("Извлечение выполнено успешно.");
}

void ExtractWidget::onSaveClicked()
{
    if (m_extractedPayload.isEmpty())
        return;

    QString path = QFileDialog::getSaveFileName(
        this, "Сохранить извлечённый текст", {}, "Текстовые файлы (*.txt);;Все файлы (*)");

    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        setStatus("Не удалось сохранить файл: " + f.errorString(), true);
        return;
    }
    f.write(m_extractedPayload);
    f.close();

    setStatus(QString("Файл сохранён: %1").arg(QFileInfo(path).fileName()));
}

void ExtractWidget::setStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? "color: red;" : "color: green;");
}
