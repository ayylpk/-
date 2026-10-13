#include "core/ClipboardMonitor.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

ClipboardMonitor::ClipboardMonitor(QObject *parent)
    : QObject(parent)
    , m_clipboard(QApplication::clipboard())
{
    if (m_clipboard)
        connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardMonitor::onClipboardChanged);
}

void ClipboardMonitor::suppress()
{
    m_suppressed = true;
}

void ClipboardMonitor::release()
{
    m_suppressed = false;
}

void ClipboardMonitor::onClipboardChanged()
{
    if (m_suppressed || !m_clipboard)
        return;

    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime)
        return;

    // 先看图片：很多截图工具会同时塞图片和文本，图片优先
    if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            m_lastText.clear();
            emit imageCaptured(image);
            return;
        }
    }

    if (mime->hasText()) {
        const QString text = mime->text();
        if (text.isEmpty())
            return;

        // 有些程序会连着触发好几次同样的内容，这里先挡一道
        if (text == m_lastText)
            return;

        m_lastText = text;
        emit textCaptured(text);
    }
}
