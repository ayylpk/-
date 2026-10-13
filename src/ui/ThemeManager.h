#pragma once

#include <QObject>
#include <QString>

#include "core/SettingsStore.h"

/*!
 * 主题 = 一份 QSS，直接 qApp->setStyleSheet()，
 * 所有已存在和之后新建的窗口都会跟着变，不用挨个通知。
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(SettingsStore *settings, QObject *parent = nullptr);

    void apply(SettingsStore::Theme theme);
    SettingsStore::Theme currentTheme() const { return m_currentTheme; }

    static QString loadStyleSheet(SettingsStore::Theme theme);
    static QString resourcePath(SettingsStore::Theme theme);

private slots:
    void onThemeChanged(SettingsStore::Theme theme);

private:
    SettingsStore *m_settings = nullptr;
    SettingsStore::Theme m_currentTheme = SettingsStore::Light;
};
