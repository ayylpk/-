#include "app/Application.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#include "core/ClipboardMonitor.h"
#include "core/GlobalHotkey.h"
#include "core/SettingsStore.h"
#include "core/StorageManager.h"
#include "ui/MainWindow.h"
#include "ui/SettingsWindow.h"
#include "ui/ThemeManager.h"

#ifdef Q_OS_WIN
// MinGW 的 os_defines.h 自己会定义 NOMINMAX，这里必须判一下，
// 否则会撞 "NOMINMAX redefined" 警告。
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace {

// 让目标窗口先拿到焦点，再送 Ctrl+V
const int kFocusSettleMs = 120;
// 粘贴期间屏蔽监听的时间
const int kSuppressWindowMs = 400;
// 刚粘贴完的一小段时间内忽略热键，避免自己唤醒自己
const qint64 kHotkeyPasteGuardMs = 350;

const int kTrayIconSize = 64;

} // namespace

Application::Application(QObject *parent)
    : QObject(parent)
{
}

Application::~Application()
{
    // 托盘图标不显式隐藏的话，退出后可能留一个幽灵图标在托盘里。
    // 顺序要紧：先摘掉菜单引用，再销毁菜单，最后销毁图标。
    if (m_tray) {
        m_tray->hide();
        m_tray->setContextMenu(nullptr);
        delete m_tray;
        m_tray = nullptr;
    }

    delete m_trayMenu;
    m_trayMenu = nullptr;

    delete m_settingsWindow;
    m_settingsWindow = nullptr;

    delete m_listWindow;
    m_listWindow = nullptr;
}

bool Application::start()
{
    if (m_started)
        return true;

    m_settings = new SettingsStore(this);

    if (!initStorage())
        return false;

    m_theme = new ThemeManager(m_settings, this);
    m_theme->apply(m_settings->theme());

    m_listWindow = new MainWindow(m_storage, m_settings);
    connect(m_listWindow, &MainWindow::itemActivated, this, &Application::onItemActivated);
    connect(m_listWindow, &MainWindow::settingsRequested, this, &Application::showSettingsWindow);
    connect(m_listWindow, &MainWindow::dataDirRequested, this, &Application::openDataDirectory);

    m_settingsWindow = new SettingsWindow(m_settings, m_storage);
    connect(m_settingsWindow, &SettingsWindow::openDataDirRequested,
            this, &Application::openDataDirectory);
    connect(m_settingsWindow, &SettingsWindow::hotkeyChanged,
            this, &Application::onHotkeyRequested);

    m_monitor = new ClipboardMonitor(this);
    connect(m_monitor, &ClipboardMonitor::textCaptured, this, &Application::onTextCaptured);
    connect(m_monitor, &ClipboardMonitor::imageCaptured, this, &Application::onImageCaptured);

    connect(m_settings, &SettingsStore::maxItemsChanged, this, &Application::onMaxItemsChanged);

    initHotkey();
    initTray();

    m_started = true;
    return true;
}

bool Application::initStorage()
{
    m_storage = new StorageManager(this);

    if (m_storage->open(m_settings->dataDir(), m_settings->maxItems())) {
        return true;
    }

    // 这时候托盘还没建起来，只能弹个框
    QMessageBox::critical(nullptr,
                          QStringLiteral("剪切板历史"),
                          QStringLiteral("数据目录打不开：\n%1\n\n程序无法继续运行。")
                                  .arg(QDir::toNativeSeparators(m_settings->dataDir())));
    return false;
}

bool Application::initHotkey()
{
    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::activated, this, &Application::onHotkeyActivated);
    connect(m_hotkey, &GlobalHotkey::registrationFailed,
            this, &Application::onHotkeyRegistrationFailed);

    const QString sequence = m_settings->hotkey();
    if (!m_hotkey->registerSequence(sequence)) {
        // 注册失败不算致命：托盘菜单仍然能唤出列表
        return false;
    }

    return true;
}

QIcon Application::buildTrayIcon() const
{
    // 直接画一个，省掉一个图标资源文件
    QPixmap pixmap(kTrayIconSize, kTrayIconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x35, 0x7a, 0xbd));
    painter.drawRoundedRect(QRectF(4, 4, kTrayIconSize - 8, kTrayIconSize - 8), 14, 14);

    QFont font = painter.font();
    font.setPixelSize(34);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(0xff, 0xff, 0xff));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("剪"));
    painter.end();

    return QIcon(pixmap);
}

void Application::initTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(buildTrayIcon(), this);
    m_tray->setToolTip(QStringLiteral("剪切板历史"));

    m_trayMenu = new QMenu;

    m_showListAction = m_trayMenu->addAction(QStringLiteral("显示列表"));
    connect(m_showListAction, &QAction::triggered, this, &Application::showListWindow);

    m_settingsAction = m_trayMenu->addAction(QStringLiteral("设置…"));
    connect(m_settingsAction, &QAction::triggered, this, &Application::showSettingsWindow);

    m_dataDirAction = m_trayMenu->addAction(QStringLiteral("打开数据目录"));
    connect(m_dataDirAction, &QAction::triggered, this, &Application::openDataDirectory);

    m_trayMenu->addSeparator();

    m_quitAction = m_trayMenu->addAction(QStringLiteral("退出"));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &Application::onTrayActivated);

    m_tray->show();
    updateTrayMenu();

    // 第一次跑的时候提示一下热键，不然用户不知道该按什么
    if (m_hotkey && m_hotkey->isRegistered()) {
        m_tray->showMessage(QStringLiteral("剪切板历史已在后台运行"),
                            QStringLiteral("按 %1 唤出历史列表。")
                                    .arg(m_settings->hotkey()),
                            QSystemTrayIcon::Information,
                            4000);
    }
}

void Application::updateTrayMenu()
{
    if (!m_showListAction)
        return;

    const QString hotkey = m_settings->hotkey();
    m_showListAction->setText(hotkey.isEmpty() ? QStringLiteral("显示列表")
                                               : QStringLiteral("显示列表 (%1)").arg(hotkey));

    if (m_tray) {
        m_tray->setToolTip(m_hotkey && m_hotkey->isRegistered()
                                   ? QStringLiteral("剪切板历史 · %1").arg(hotkey)
                                   : QStringLiteral("剪切板历史 · 热键未生效"));
    }
}

void Application::onHotkeyActivated()
{
    // 刚粘贴完的那一下不算
    if (QDateTime::currentMSecsSinceEpoch() - m_lastPasteAt < kHotkeyPasteGuardMs)
        return;

    // 热键只负责"唤出"和"提到最前"，不再做开关切换。
    // 关掉列表只剩两种方式：用了一条（回车/空格/点击）或者按 Esc。
    if (m_listWindow->isVisible()) {
        m_listWindow->raise();
        m_listWindow->activateWindow();
        return;
    }

    showListWindow();
}

void Application::onHotkeyRequested(const QString &sequence)
{
    if (!m_hotkey || !m_settings)
        return;

    const QString previous = m_settings->hotkey();

    if (m_hotkey->registerSequence(sequence)) {
        m_settings->setHotkey(sequence);
        updateTrayMenu();
        if (m_settingsWindow)
            m_settingsWindow->clearHotkeyWarning();
        return;
    }

    // 失败：把生效中的热键抢回来，并让界面回到真实状态
    m_hotkey->registerSequence(previous);

    if (m_settingsWindow) {
        m_settingsWindow->showHotkeyWarning(
                QStringLiteral("「%1」注册失败，可能被别的程序占用了，换一个组合试试。")
                        .arg(sequence));
        m_settingsWindow->reloadFromSettings();
    }

    updateTrayMenu();
}

void Application::onHotkeyRegistrationFailed(const QString &sequence)
{
    if (!m_settingsWindow)
        return;

    m_settingsWindow->showHotkeyWarning(
            QStringLiteral("「%1」注册失败，可能被别的程序占用了。\n"
                           "在设置里换一个组合，或者从托盘菜单唤出列表。")
                    .arg(sequence));
}

void Application::onTextCaptured(const QString &text)
{
    if (m_storage)
        m_storage->addText(text);
}

void Application::onImageCaptured(const QImage &image)
{
    if (m_storage)
        m_storage->addImage(image);
}

const ClipboardItem *Application::findItem(qint64 id) const
{
    if (!m_storage)
        return nullptr;

    const QVector<ClipboardItem> &items = m_storage->items();
    for (const ClipboardItem &item : items) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

void Application::onItemActivated(qint64 id)
{
    const ClipboardItem *item = findItem(id);
    if (!item)
        return;

    pasteToCursor(*item);
}

void Application::pasteToCursor(const ClipboardItem &item)
{
    m_lastPasteAt = QDateTime::currentMSecsSinceEpoch();

    // 这一步很关键：接下来写剪切板的是我们自己，监听必须闭眼
    m_monitor->suppress();

    if (item.isImage()) {
        const QImage image(m_storage->imageAbsolutePath(item));
        if (image.isNull()) {
            m_monitor->release();
            return;
        }
        QApplication::clipboard()->setImage(image);
    } else {
        QApplication::clipboard()->setText(item.content);
    }

    // 先把自己藏起来，焦点才会还回原来那个窗口
    m_listWindow->hide();

    QTimer::singleShot(kFocusSettleMs, this, []() {
        sendPasteShortcut();
    });

    QTimer::singleShot(kSuppressWindowMs, this, [this]() {
        m_monitor->release();
    });
}

void Application::sendPasteShortcut()
{
#ifdef Q_OS_WIN
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
#endif
}

void Application::showListWindow()
{
    if (!m_listWindow)
        return;

    m_listWindow->popupAtCursor();
}

void Application::showSettingsWindow()
{
    if (!m_settingsWindow)
        return;

    if (m_listWindow && m_listWindow->isVisible())
        m_listWindow->hide();

    m_settingsWindow->reloadFromSettings();
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void Application::openDataDirectory()
{
    if (!m_storage)
        return;

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_storage->dataDir()));
}

void Application::onMaxItemsChanged(int value)
{
    if (m_storage)
        m_storage->setMaxItems(value);
}

void Application::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        showListWindow();
}
