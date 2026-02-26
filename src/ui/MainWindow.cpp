#include "MainWindow.h"
#include "EmbedWidget.h"
#include "ExtractWidget.h"
#include "AnalysisWidget.h"

#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QApplication>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("SonoStega — стеганография аудио");
    setMinimumSize(900, 650);
    setupUi();
    setupMenuBar();
    statusBar()->showMessage("Готово");
}

void MainWindow::setupUi()
{
    m_tabs = new QTabWidget(this);

    m_embedWidget    = new EmbedWidget(m_tabs);
    m_extractWidget  = new ExtractWidget(m_tabs);
    m_analysisWidget = new AnalysisWidget(m_tabs);

    m_tabs->addTab(m_embedWidget,    "Встроить");
    m_tabs->addTab(m_extractWidget,  "Извлечь");
    m_tabs->addTab(m_analysisWidget, "Анализ качества");

    // After embedding: populate ExtractWidget and AnalysisWidget automatically
    connect(m_embedWidget, &EmbedWidget::embedded, this,
            [this](const QString &origPath, const QString &stegoPath) {
        statusBar()->showMessage("Встраивание завершено: " + stegoPath, 5000);
        m_extractWidget->loadStegoFile(stegoPath);
        m_analysisWidget->loadFiles(origPath, stegoPath);
    });

    setCentralWidget(m_tabs);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    QAction *exitAction = fileMenu->addAction("Выход");
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *helpMenu = menuBar()->addMenu("Справка");
    QAction *aboutAction = helpMenu->addAction("О программе");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "О программе",
            "<b>SonoStega</b> v1.0<br><br>"
            "Стеганографическое скрытие данных в аудиофайлах WAV<br>"
            "методом кодирования наименее значащих бит (LSB).<br><br>"
            "Метрики качества: MSE, NMSE.<br><br>"
            "Разработано в рамках курсовой работы по стеганографии.");
    });
}
