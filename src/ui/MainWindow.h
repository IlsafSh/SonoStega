#pragma once

#include <QMainWindow>

class QTabWidget;
class EmbedWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private:
    void setupUi();
    void setupMenuBar();

    QTabWidget *m_tabs;
    EmbedWidget *m_embedWidget;
};
