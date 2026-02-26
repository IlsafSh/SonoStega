#include "EmbedWidget.h"
#include "core/LsbEmbedder.h"

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
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

EmbedWidget::EmbedWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void EmbedWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // --- WAV container group ------------------------------------------------
    auto *wavGroup = new QGroupBox("Контейнер (WAV-файл)");
    auto *wavLayout = new QVBoxLayout(wavGroup);

    auto *wavRow = new QHBoxLayout();
    m_wavPathEdit = new QLineEdit();
    m_wavPathEdit->setReadOnly(true);
    m_wavPathEdit->setPlaceholderText("Выберите WAV-файл...");
    auto *wavBrowseBtn = new QPushButton("Обзор...");
    wavBrowseBtn->setFixedWidth(90);
    wavRow->addWidget(m_wavPathEdit);
    wavRow->addWidget(wavBrowseBtn);

    m_wavInfoLabel = new QLabel("—");
    m_wavInfoLabel->setStyleSheet("color: gray;");
    m_capacityLabel = new QLabel("—");
    m_capacityLabel->setStyleSheet("color: gray;");

    wavLayout->addLayout(wavRow);
    wavLayout->addWidget(m_wavInfoLabel);
    wavLayout->addWidget(m_capacityLabel);

    // --- Message group -------------------------------------------------------
    auto *msgGroup = new QGroupBox("Сообщение (TXT-файл)");
    auto *msgLayout = new QVBoxLayout(msgGroup);

    auto *msgRow = new QHBoxLayout();
    m_msgPathEdit = new QLineEdit();
    m_msgPathEdit->setReadOnly(true);
    m_msgPathEdit->setPlaceholderText("Выберите TXT-файл...");
    auto *msgBrowseBtn = new QPushButton("Обзор...");
    msgBrowseBtn->setFixedWidth(90);
    msgRow->addWidget(m_msgPathEdit);
    msgRow->addWidget(msgBrowseBtn);

    m_msgInfoLabel = new QLabel("—");
    m_msgInfoLabel->setStyleSheet("color: gray;");

    msgLayout->addLayout(msgRow);
    msgLayout->addWidget(m_msgInfoLabel);

    // --- Parameters group ----------------------------------------------------
    auto *paramGroup = new QGroupBox("Параметры встраивания");
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

    // Password row (hidden by default)
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

    // --- Embed button --------------------------------------------------------
    m_embedBtn = new QPushButton("Встроить и сохранить...");
    m_embedBtn->setFixedHeight(36);
    m_embedBtn->setEnabled(false);

    // --- Status label --------------------------------------------------------
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // --- Assemble ------------------------------------------------------------
    mainLayout->addWidget(wavGroup);
    mainLayout->addWidget(msgGroup);
    mainLayout->addWidget(paramGroup);
    mainLayout->addWidget(m_embedBtn);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch();

    // --- Connections ---------------------------------------------------------
    connect(wavBrowseBtn, &QPushButton::clicked, this, &EmbedWidget::browseWav);
    connect(msgBrowseBtn, &QPushButton::clicked, this, &EmbedWidget::browseMsg);
    connect(m_nBitsSlider, &QSlider::valueChanged, this, &EmbedWidget::onNBitsChanged);
    connect(m_seqRadio, &QRadioButton::toggled, this, &EmbedWidget::onModeChanged);
    connect(m_keyedRadio, &QRadioButton::toggled, this, &EmbedWidget::onModeChanged);
    connect(m_embedBtn, &QPushButton::clicked, this, &EmbedWidget::onEmbedClicked);
}

void EmbedWidget::browseWav()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Выберите WAV-файл контейнера", {},
        "WAV-файлы (*.wav);;Все файлы (*)");

    if (path.isEmpty())
        return;

    m_wav = WavFile();
    if (!m_wav.load(path)) {
        setStatus("Ошибка загрузки WAV: " + m_wav.errorString(), true);
        m_wavPathEdit->clear();
        m_wavInfoLabel->setText("—");
        m_capacityLabel->setText("—");
        m_embedBtn->setEnabled(false);
        return;
    }

    m_wavPathEdit->setText(path);
    updateWavInfo();
    setStatus({});
}

void EmbedWidget::browseMsg()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Выберите текстовый файл", {},
        "Текстовые файлы (*.txt);;Все файлы (*)");

    if (path.isEmpty())
        return;

    QFileInfo fi(path);
    m_msgSize = fi.size();
    m_msgPathEdit->setText(path);

    if (m_msgSize < 1024)
        m_msgInfoLabel->setText(QString("Размер: %1 байт").arg(m_msgSize));
    else
        m_msgInfoLabel->setText(QString("Размер: %1 КБ").arg(m_msgSize / 1024.0, 0, 'f', 1));

    m_msgInfoLabel->setStyleSheet("color: black;");
    updateCapacityLabel();
    setStatus({});
}

void EmbedWidget::updateWavInfo()
{
    if (!m_wav.isValid())
        return;

    const auto &h = m_wav.header();
    int totalSec = static_cast<int>(m_wav.durationSeconds());
    QString dur = QString("%1:%2").arg(totalSec / 60).arg(totalSec % 60, 2, 10, QChar('0'));

    m_wavInfoLabel->setText(QString("Формат: %1  |  Длительность: %2")
                                .arg(m_wav.formatDescription(), dur));
    m_wavInfoLabel->setStyleSheet("color: black;");
    updateCapacityLabel();

    m_embedBtn->setEnabled(m_wav.isValid() && !m_msgPathEdit->text().isEmpty());
}

void EmbedWidget::updateCapacityLabel()
{
    if (!m_wav.isValid()) {
        m_capacityLabel->setText("—");
        return;
    }

    int n = m_nBitsSlider->value();
    uint64_t cap = m_wav.capacityBytes(n);

    QString capStr;
    if (cap < 1024)
        capStr = QString("%1 байт").arg(cap);
    else
        capStr = QString("%1 КБ").arg(cap / 1024.0, 0, 'f', 1);

    QString utilStr;
    if (m_msgSize > 0 && cap > 0) {
        double util = 100.0 * m_msgSize / static_cast<double>(cap);
        utilStr = QString("  |  Сообщение: %1%  заполнения").arg(util, 0, 'f', 1);
    }

    m_capacityLabel->setText(QString("Ёмкость при N=%1: %2%3").arg(n).arg(capStr, utilStr));
    m_capacityLabel->setStyleSheet(
        (m_msgSize > 0 && m_msgSize > static_cast<qint64>(cap)) ? "color: red;" : "color: gray;");

    m_embedBtn->setEnabled(m_wav.isValid() && !m_msgPathEdit->text().isEmpty());
}

void EmbedWidget::onNBitsChanged(int value)
{
    m_nBitsValueLabel->setText(QString::number(value));
    updateCapacityLabel();
}

void EmbedWidget::onModeChanged()
{
    m_passwordRow->setVisible(m_keyedRadio->isChecked());
}

void EmbedWidget::onEmbedClicked()
{
    if (!m_wav.isValid()) {
        setStatus("Сначала загрузите WAV-файл.", true);
        return;
    }
    if (m_msgPathEdit->text().isEmpty()) {
        setStatus("Сначала выберите TXT-файл.", true);
        return;
    }
    if (m_keyedRadio->isChecked() && m_passwordEdit->text().isEmpty()) {
        setStatus("Введите пароль для режима 'С паролем'.", true);
        return;
    }

    // Read message
    QFile f(m_msgPathEdit->text());
    if (!f.open(QIODevice::ReadOnly)) {
        setStatus("Не удалось открыть файл сообщения: " + f.errorString(), true);
        return;
    }
    QByteArray payload = f.readAll();
    f.close();

    // Choose output path
    QString savePath = QFileDialog::getSaveFileName(
        this, "Сохранить стего-файл", {}, "WAV-файлы (*.wav)");
    if (savePath.isEmpty())
        return;

    // Make a copy so the original stays intact
    WavFile stegoWav = m_wav;

    int n = m_nBitsSlider->value();
    LsbEmbedder::Result res;
    if (m_keyedRadio->isChecked())
        res = LsbEmbedder::embedKeyed(stegoWav, payload, n, m_passwordEdit->text());
    else
        res = LsbEmbedder::embed(stegoWav, payload, n);

    if (!res.ok) {
        setStatus("Ошибка встраивания: " + res.error, true);
        return;
    }

    if (!stegoWav.save(savePath)) {
        setStatus("Не удалось сохранить файл: " + savePath, true);
        return;
    }

    setStatus(QString("Готово! Стего-файл сохранён: %1").arg(QFileInfo(savePath).fileName()));
    emit embedded(m_wavPathEdit->text(), savePath);
}

void EmbedWidget::setStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? "color: red;" : "color: green;");
}
