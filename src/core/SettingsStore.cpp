#include "core/SettingsStore.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

const char *kKeyTheme = "appearance/theme";
const char *kKeyMaxItems = "history/maxItems";
const char *kKeyHotkey = "hotkey/sequence";
const char *kKeyPasteOnClick = "interaction/pasteOnClick";
const char *kKeyThumbnailHeight = "appearance/thumbnailHeight";
const char *kKeyListWidth = "window/listWidth";
const char *kKeyListHeight = "window/listHeight";
const char *kKeyBackgroundOpacity = "window/backgroundOpacity";

const int kMinMaxItems = 20;
const int kMaxMaxItems = 1000;
const int kDefaultMaxItems = 200;

const int kMinThumbnail = 24;
const int kMaxThumbnail = 256;
const int kDefaultThumbnail = 48;

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
