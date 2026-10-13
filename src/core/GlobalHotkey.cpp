#include "core/GlobalHotkey.h"

#include <QApplication>
#include <QKeySequence>

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

#ifdef Q_OS_WIN

quint32 toNativeModifiers(Qt::KeyboardModifiers modifiers)
{
    quint32 native = 0;

    if (modifiers & Qt::ControlModifier)
        native |= MOD_CONTROL;
    if (modifiers & Qt::AltModifier)
        native |= MOD_ALT;
    if (modifiers & Qt::ShiftModifier)
        native |= MOD_SHIFT;
    if (modifiers & Qt::MetaModifier)
        native |= MOD_WIN;

    return native;
}

/*! Qt 的 Qt::Key 翻译成 Win32 的虚拟键码，认不出来返回 0。 */
quint32 toVirtualKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<quint32>('A' + (key - Qt::Key_A));

    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<quint32>('0' + (key - Qt::Key_0));

    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return static_cast<quint32>(VK_F1 + (key - Qt::Key_F1));

    switch (key) {
    case Qt::Key_Space:      return VK_SPACE;
    case Qt::Key_Tab:        return VK_TAB;
    case Qt::Key_Return:     return VK_RETURN;
    case Qt::Key_Enter:      return VK_RETURN;
    case Qt::Key_Escape:     return VK_ESCAPE;
    case Qt::Key_Backspace:  return VK_BACK;
    case Qt::Key_Delete:     return VK_DELETE;
    case Qt::Key_Insert:     return VK_INSERT;
    case Qt::Key_Home:       return VK_HOME;
    case Qt::Key_End:        return VK_END;
    case Qt::Key_PageUp:     return VK_PRIOR;
    case Qt::Key_PageDown:   return VK_NEXT;
    case Qt::Key_Up:         return VK_UP;
    case Qt::Key_Down:       return VK_DOWN;
    case Qt::Key_Left:       return VK_LEFT;
    case Qt::Key_Right:      return VK_RIGHT;
    case Qt::Key_QuoteLeft:  return VK_OEM_3;
    case Qt::Key_Minus:      return VK_OEM_MINUS;
    case Qt::Key_Equal:      return VK_OEM_PLUS;
    case Qt::Key_BracketLeft:  return VK_OEM_4;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Backslash:    return VK_OEM_5;
    case Qt::Key_Semicolon:    return VK_OEM_1;
    case Qt::Key_Apostrophe:   return VK_OEM_7;
    case Qt::Key_Comma:        return VK_OEM_COMMA;
    case Qt::Key_Period:       return VK_OEM_PERIOD;
    case Qt::Key_Slash:        return VK_OEM_2;
    default:
        break;
    }

    return 0;
}

#endif // Q_OS_WIN

} // namespace

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregister();
}

bool GlobalHotkey::registerSequence(const QString &sequence)
{
    unregister();

#ifdef Q_OS_WIN
    const QKeySequence keySequence(sequence, QKeySequence::PortableText);
    if (keySequence.isEmpty()) {
        emit registrationFailed(sequence);
        return false;
    }

    const QKeyCombination combination = keySequence[0];
    const quint32 modifiers = toNativeModifiers(combination.keyboardModifiers());
    const quint32 virtualKey = toVirtualKey(combination.key());

    if (virtualKey == 0 || modifiers == 0) {
        emit registrationFailed(sequence);
        return false;
    }

    // MOD_NOREPEAT：按住不放不要疯狂触发
    if (!RegisterHotKey(nullptr, m_hotkeyId, modifiers | MOD_NOREPEAT, virtualKey)) {
        emit registrationFailed(sequence);
        return false;
    }

    if (!m_filterInstalled) {
        qApp->installNativeEventFilter(this);
        m_filterInstalled = true;
    }

    m_registered = true;
    m_sequence = sequence;
    return true;
#else
    Q_UNUSED(sequence);
    emit registrationFailed(sequence);
    return false;
#endif
}

void GlobalHotkey::unregister()
{
#ifdef Q_OS_WIN
    if (m_registered)
        UnregisterHotKey(nullptr, m_hotkeyId);
#endif

    m_registered = false;
    m_sequence.clear();
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    if (!m_registered || !message)
        return false;

    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_hotkeyId) {
        emit activated();
        return true;
    }
#endif

    return false;
}
