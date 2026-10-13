#include <QApplication>

#include "app/Application.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 这三个名字决定了 QSettings 和 QStandardPaths 的落点，
    // 必须在任何用到它们的东西创建之前设好。
    QApplication::setOrganizationName(QStringLiteral("jianqie"));
    QApplication::setOrganizationDomain(QStringLiteral("jianqie.local"));
    QApplication::setApplicationName(QStringLiteral("jianqie"));
    QApplication::setApplicationDisplayName(QStringLiteral("剪切板历史"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // 这程序靠托盘和热键活着，关了窗口（列表/设置）不该退出
    QApplication::setQuitOnLastWindowClosed(false);

    Application controller;
    if (!controller.start())
        return 1;

    return QApplication::exec();
}
