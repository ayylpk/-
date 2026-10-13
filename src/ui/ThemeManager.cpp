#include "ui/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

ThemeManager::ThemeManager(SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (m_settings) {
        m_currentTheme = m_settings->theme();
        connect(m_settings, &SettingsStore::themeChanged, this, &ThemeManager::onThemeChanged);
    }
}

QString ThemeManager::resourcePath(SettingsStore::Theme theme)
{
    return theme == SettingsStore::Dark ? QStringLiteral(":/themes/dark.qss")
                                        : QStringLiteral(":/themes/light.qss");
}

QString ThemeManager::loadStyleSheet(SettingsStore::Theme theme)
{
    QFile file(resourcePath(theme));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream stream(&file);
    return stream.readAll();
}

void ThemeManager::apply(SettingsStore::Theme theme)
{
    m_currentTheme = theme;

    // 空字符串也算有效结果：资源没编进去时不至于把界面搞成花的
    qApp->setStyleSheet(loadStyleSheet(theme));
}

void ThemeManager::onThemeChanged(SettingsStore::Theme theme)
{
    apply(theme);
}
