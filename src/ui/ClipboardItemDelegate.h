#pragma once

#include <QColor>
#include <QRect>
#include <QSize>
#include <QStyledItemDelegate>

#include "core/ClipboardItem.h"

class QFont;
class SettingsStore;
class StorageManager;

/*!
 * 整行自己画的列表项绘制器。
 *
 * 为什么不用 QListWidget 默认那套 setIcon + setText：
 * 默认绘制里，带图标的行文字会被图标顶到右边，不带图标的行文字贴左边，
 * 文本和图片混排的时候左边缘就是参差的。
 *
 * 这里改成：**所有行都留出同样宽度的左侧图标槽**（图片行放缩略图，
 * 文本行放一个轻量的占位小方块），文字统一从同一个 x 开始，垂直居中。
 *
 * 背景 / 选中 / hover 仍然交给样式去画，所以 QSS 里的配色照旧生效，
 * 不会出现"delegate 里硬编码的颜色和主题对不上"的问题。
 */
class ClipboardItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ClipboardItemDelegate(StorageManager *storage, SettingsStore *settings, QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /*! 主题切换时由 MainWindow 通知，这里只用它决定自己画的那几个颜色。 */
    void setDarkTheme(bool dark);
    bool isDarkTheme() const { return m_dark; }

    /*!
     * 只有列表里存在勾选项时，才把所有行的勾选框显示出来。
     * 平时不显示，免得每行都挂个空框显得吵。
     */
    void setMarksVisible(bool visible);
    bool marksVisible() const { return m_marksVisible; }

private:
    int thumbnailHeight() const;
    QRect gutterRect(const QRect &rowRect) const;
    QRect textRect(const QRect &rowRect) const;
    QRect markBoxRect(const QRect &rowRect) const;

    QColor textColor() const;
    QColor mutedColor() const;
    QColor pinColor() const;
    QColor slotFillColor() const;
    QColor slotStrokeColor() const;
    QColor markColor() const;

    void drawPlaceholderSlot(QPainter *painter, const QRect &slot) const;
    void drawRowText(QPainter *painter, const QRect &textArea,
                     const ClipboardItem &item, const QFont &font) const;
    void drawMarkBox(QPainter *painter, const QRect &rowRect, bool marked) const;

    StorageManager *m_storage = nullptr;
    SettingsStore *m_settings = nullptr;
    bool m_dark = false;
    bool m_marksVisible = false;
};
