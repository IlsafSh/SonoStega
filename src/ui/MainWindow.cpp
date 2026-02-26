#include "MainWindow.h"
#include "EmbedWidget.h"

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

    m_embedWidget = new EmbedWidget(m_tabs);
    m_tabs->addTab(m_embedWidget, "Встроить");
    m_tabs->addTab(new QLabel("  Модуль извлечения — в разработке", m_tabs), "Извлечь");
    m_tabs->addTab(new QLabel("  Модуль анализа качества — в разработке", m_tabs), "Анализ качества");

    // When embedding is done, show a status bar message
    connect(m_embedWidget, &EmbedWidget::embedded, this, [this](const QString &, const QString &stego) {
        statusBar()->showMessage("Встраивание завершено: " + stego, 5000);
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
