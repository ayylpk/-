#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

/*!
 * 全局热键。
 *
 * Qt 本身没有全局快捷键，这里直接用 Win32 的 RegisterHotKey：
 * 注册到当前线程的消息队列，再通过 nativeEventFilter 截 WM_HOTKEY。
 * 好处是不用引 QHotkey 之类的第三方库。
 *
 * 注意：被别的程序占用的组合键会注册失败，isRegistered() 会是 false。
 */
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    /*! 接受 "Ctrl+Alt+V" 这类写法（QKeySequence 的字符串形式）。 */
    bool registerSequence(const QString &sequence);
    void unregister();

    bool isRegistered() const { return m_registered; }
    QString sequence() const { return m_sequence; }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void activated();
    /*! 注册失败时带出原因，界面拿去提示用户。 */
    void registrationFailed(const QString &sequence);

private:
    static const int kHotkeyId = 0x4A51; // "JQ"

    int m_hotkeyId = kHotkeyId;
    bool m_registered = false;
    bool m_filterInstalled = false;
    QString m_sequence;
};
