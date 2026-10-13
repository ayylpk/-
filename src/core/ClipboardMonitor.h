#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class QClipboard;

/*!
 * 监听系统剪切板。
 *
 * 唯一要注意的坑：程序自己为了让用户"粘贴回去"也会去写剪切板，
 * 那次改动必须被忽略，否则会把刚粘贴的内容又存一遍。
 * 所以写入前调 suppress()，够时间之后再 release()。
 */
class ClipboardMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(QObject *parent = nullptr);

    /*! 忽略接下来的剪切板变化。 */
    void suppress();
    /*! 恢复正常监听。 */
    void release();
    bool isSuppressed() const { return m_suppressed; }

signals:
    void textCaptured(const QString &text);
    void imageCaptured(const QImage &image);

private slots:
    void onClipboardChanged();

private:
    QClipboard *m_clipboard = nullptr;
    bool m_suppressed = false;
    QString m_lastText;
};
