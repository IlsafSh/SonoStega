#include "MainWindow.h"

#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QApplication>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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
    m_tabs->addTab(new QLabel("  Модуль встраивания — в разработке", m_tabs), "Встроить");
    m_tabs->addTab(new QLabel("  Модуль извлечения — в разработке", m_tabs), "Извлечь");
    m_tabs->addTab(new QLabel("  Модуль анализа качества — в разработке", m_tabs), "Анализ качества");
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
