#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("VirtualTableExample");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Qt Virtual Table Example");
    
    // 设置样式
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    // 运行应用程序
    return app.exec();
}