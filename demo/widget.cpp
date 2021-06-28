#include "widget.h"
#include <QPushButton>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    QPushButton *button = new QPushButton("crash", this);

    connect(button, &QPushButton::clicked, this, &Widget::crash);
}

Widget::~Widget()
{

}

void Widget::crash()
{
    int* a = nullptr;
    *a = 1;
}
