#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = Q_NULLPTR);
    ~Widget();

private Q_SLOTS:
    void crash();
};

#endif // WIDGET_H
