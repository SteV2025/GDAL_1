#include "mainwindow.h"
#include <QApplication>
#include "gdal_priv.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GDALAllRegister(); // 注册所有 GDAL 驱动，包括 HDF4
    MainWindow w;
    w.show();
    return a.exec();
}
