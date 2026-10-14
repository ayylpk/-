#include "core/SettingsStore.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

const char *kKeyTheme = "appearance/theme";
const char *kKeyMaxItems = "history/maxItems";
const char *kKeyHotkey = "hotkey/sequence";
const char *kKeyPasteOnClick = "interaction/pasteOnClick";
// 注意：键名从 appearance/thumbnailHeight 改成了 appearance/imageMaxHeight。
// 语义变了（从"固定高度"变成"最大高度上限"），换个 key 让新默认值一定生效，
// 不然已经跑过的机器上会留着旧的 48，看起来像没改。
const char *kKeyThumbnailHeight = "appearance/imageMaxHeight";
const char *kKeyListWidth = "window/listWidth";
const char *kKeyListHeight = "window/listHeight";
const char *kKeyBackgroundOpacity = "window/backgroundOpacity";

const int kMinMaxItems = 20;
const int kMaxMaxItems = 1000;
const int kDefaultMaxItems = 200;

// 图片预览的最大高度。行高按图片自身尺寸的 1/8 算，再夹在这个上限之内 ——
// 所以这个值不是"固定高度"，而是"最大能到多高"。
const int kMinThumbnail = 40;
const int kMaxThumbnail = 400;
const int kDefaultThumbnail = 180;

const int kMinListWidth = 420;
const int kMaxListWidth = 1600;
const int kDefaultListWidth = 600;

const int kMinListHeight = 260;
const int kMaxListHeight = 1200;
const int kDefaultListHeight = 520;

} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
}

SettingsStore::Theme SettingsStore::theme() const
{
    QSettings s;
    return themeFromString(s.value(QString::fromLatin1(kKeyTheme)).toString());
}

void SettingsStore::setTheme(Theme theme)
{
    if (theme == this->theme())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyTheme), themeToString(theme));
    emit themeChanged(theme);
}

int SettingsStore::maxItems() const
{
    QSettings s;
    const int value = s.value(QString::fromLatin1(kKeyMaxItems), kDefaultMaxItems).toInt();
    return qBound(kMinMaxItems, value, kMaxMaxItems);
}

void SettingsStore::setMaxItems(int value)
{
    const int clamped = qBound(kMinMaxItems, value, kMaxMaxItems);
    if (clamped == maxItems())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyMaxItems), clamped);
    emit maxItemsChanged(clamped);
}

QString SettingsStore::hotkey() const
{
    QSettings s;
    const QString value = s.value(QString::fromLatin1(kKeyHotkey)).toString().trimmed();
    return value.isEmpty() ? QStringLiteral("Ctrl+Alt+V") : value;
}

void SettingsStore::setHotkey(const QString &sequence)
{
    const QString normalized = sequence.trimmed();
    if (normalized.isEmpty() || normalized == hotkey())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyHotkey), normalized);
    emit hotkeyChanged(normalized);
}

bool SettingsStore::pasteOnClick() const
{
    QSettings s;
    return s.value(QString::fromLatin1(kKeyPasteOnClick), true).toBool();
}

void SettingsStore::setPasteOnClick(bool value)
{
    if (value == pasteOnClick())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyPasteOnClick), value);
    emit pasteOnClickChanged(value);
}

int SettingsStore::thumbnailHeight() const
{
    QSettings s;
    const int value = s.value(QString::fromLatin1(kKeyThumbnailHeight), kDefaultThumbnail).toInt();
    return qBound(kMinThumbnail, value, kMaxThumbnail);
}

int SettingsStore::listWidth() const
{
    QSettings s;
    const int value = s.value(QString::fromLatin1(kKeyListWidth), kDefaultListWidth).toInt();
    return qBound(kMinListWidth, value, kMaxListWidth);
}

void SettingsStore::setListWidth(int value)
{
    const int clamped = qBound(kMinListWidth, value, kMaxListWidth);
    if (clamped == listWidth())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyListWidth), clamped);
    emit listWidthChanged(clamped);
}

int SettingsStore::listHeight() const
{
    QSettings s;
    const int value = s.value(QString::fromLatin1(kKeyListHeight), kDefaultListHeight).toInt();
    return qBound(kMinListHeight, value, kMaxListHeight);
}

void SettingsStore::setListHeight(int value)
{
    const int clamped = qBound(kMinListHeight, value, kMaxListHeight);
    if (clamped == listHeight())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyListHeight), clamped);
    emit listHeightChanged(clamped);
}

int SettingsStore::backgroundOpacity() const
{
    QSettings s;
    const int value = s.value(QString::fromLatin1(kKeyBackgroundOpacity), 100).toInt();
    return qBound(0, value, 100);
}

void SettingsStore::setBackgroundOpacity(int value)
{
    const int clamped = qBound(0, value, 100);
    if (clamped == backgroundOpacity())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyBackgroundOpacity), clamped);
    emit backgroundOpacityChanged(clamped);
}

void SettingsStore::setThumbnailHeight(int value)
{
    const int clamped = qBound(kMinThumbnail, value, kMaxThumbnail);
    if (clamped == thumbnailHeight())
        return;

    QSettings s;
    s.setValue(QString::fromLatin1(kKeyThumbnailHeight), clamped);
    emit thumbnailHeightChanged(clamped);
}

QString SettingsStore::dataDir() const{
    if (!m_dataDirCache.isEmpty())
        return m_dataDirCache;

    // AppDataLocation 已经是 %APPDATA%/jianqie/jianqie 了，
    // 别再自己补一层同名目录，不然会出现 jianqie/jianqie/jianqie。
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/.jianqie");

    QDir().mkpath(dir);
    m_dataDirCache = QDir::cleanPath(dir);
    return m_dataDirCache;
}

QString SettingsStore::themeToString(Theme theme)
{
    return theme == Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

SettingsStore::Theme SettingsStore::themeFromString(const QString &text)
{
    return text.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0 ? Dark : Light;
}
