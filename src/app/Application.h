#pragma once

#include <QIcon>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

#include "core/ClipboardItem.h"

class QImage;
class QSystemTrayIcon;
class QMenu;
class QAction;

class ClipboardMonitor;
class GlobalHotkey;
class MainWindow;
class SettingsWindow;
class SettingsStore;
class StorageManager;
class ThemeManager;

/*!
 * 把各个零件拼起来的那一层。
 *
 * 启动后不弹窗口，只留一个托盘图标 + 一个全局热键：
 *   热键 → 列表弹窗 → 选一条 → 内容进剪切板 → 模拟 Ctrl+V 送回光标处 → 弹窗收起。
 */
class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    /*! 初始化失败（比如数据库打不开）时返回 false。 */
    bool start();

private slots:
    void onHotkeyActivated();
    void onHotkeyRequested(const QString &sequence);
    void onHotkeyRegistrationFailed(const QString &sequence);

    void onTextCaptured(const QString &text);
    void onImageCaptured(const QImage &image);

    void onItemActivated(qint64 id);

    void showListWindow();
    void showSettingsWindow();
    void openDataDirectory();

    void onMaxItemsChanged(int value);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    bool initStorage();
    bool initHotkey();
    void initTray();
    void updateTrayMenu();

    void pasteToCursor(const ClipboardItem &item);
    static void sendPasteShortcut();

    QIcon buildTrayIcon() const;

    const ClipboardItem *findItem(qint64 id) const;

    SettingsStore *m_settings = nullptr;
    StorageManager *m_storage = nullptr;
    ThemeManager *m_theme = nullptr;
    ClipboardMonitor *m_monitor = nullptr;
    GlobalHotkey *m_hotkey = nullptr;

    MainWindow *m_listWindow = nullptr;
    SettingsWindow *m_settingsWindow = nullptr;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showListAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_dataDirAction = nullptr;
    QAction *m_quitAction = nullptr;

    qint64 m_lastPasteAt = 0;
    bool m_started = false;
};
