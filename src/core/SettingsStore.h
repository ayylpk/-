#pragma once

#include <QObject>
#include <QString>

/*!
 * 偏好设置。底层用 QSettings（Windows 上落到注册表 HKCU\Software\jianqie\jianqie），
 * 每次改动都会发信号出去，窗口和主题都靠这些信号实时联动，不需要重启。
 */
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        Light = 0,
        Dark = 1
    };
    Q_ENUM(Theme)

    explicit SettingsStore(QObject *parent = nullptr);

    Theme theme() const;
    void setTheme(Theme theme);

    int maxItems() const;
    void setMaxItems(int value);

    QString hotkey() const;
    void setHotkey(const QString &sequence);

    bool pasteOnClick() const;
    void setPasteOnClick(bool value);

    int thumbnailHeight() const;
    void setThumbnailHeight(int value);

    int listWidth() const;
    void setListWidth(int value);

    int listHeight() const;
    void setListHeight(int value);

    /*! 窗口背景不透明度，0~100（%）。100 = 完全不透明；低于 100 才启用半透明窗口。 */
    int backgroundOpacity() const;
    void setBackgroundOpacity(int value);

    /*! 数据目录，形如 %APPDATA%/jianqie/jianqie，不存在时会创建。 */
    QString dataDir() const;

signals:
    void themeChanged(SettingsStore::Theme theme);
    void maxItemsChanged(int value);
    void hotkeyChanged(const QString &sequence);
    void pasteOnClickChanged(bool value);
    void thumbnailHeightChanged(int value);
    void listWidthChanged(int value);
    void listHeightChanged(int value);
    void backgroundOpacityChanged(int value);

private:
    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString &text);

    mutable QString m_dataDirCache;
};
