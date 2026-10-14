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
 * 两种行长得完全不一样，所以不共用一套槽位：
 *  - **文本行**：只有一行摘要，左对齐、垂直居中。没有图标、不显示时间。
 *  - **图片行**：只有缩略图，水平垂直都居中，圆角带描边，行高随图片大小走。
 *
 * 行高不是固定值：图片预览高度 = 原图像素高度 × 1/8，夹在 [下限, 设置上限] 之间，
 * 所以图片越大、这一行占得越高。图片尺寸存在库里，算行高时不需要解码图片 ——
 * 否则列 200 条记录时光是算滚动范围就会把所有图片读一遍。
 *
 * 背景 / 选中 / hover 仍然交给样式去画，所以 QSS 里的配色照旧生效；
 * delegate 只负责在背景之上画选中竖线、内容、勾选框和行分隔线。
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
    /*! 图片预览高度的上限，来自设置。 */
    int maxImageHeight() const;

    /*! 按宽高比算出这张图该显示多大；availableWidth 是这一行能用的宽度。 */
    QSize fittedImageSize(const ClipboardItem &item, int availableWidth) const;

    /*! 勾选框在行内的位置（右侧）。 */
    QRect markBoxRect(const QRect &rowRect) const;

    /*! 内容区左侧到右侧保留区之间的可用宽度。 */
    int rightReserve() const;

    QColor textColor() const;
    QColor mutedColor() const;
    QColor pinColor() const;
    QColor frameColor() const;
    QColor accentColor() const;
    QColor separatorColor() const;

    void drawTextRow(QPainter *painter, const QRect &rowRect,
                     const ClipboardItem &item, const QFont &font) const;
    void drawImageRow(QPainter *painter, const QRect &rowRect, const ClipboardItem &item) const;
    void drawSelectionBar(QPainter *painter, const QRect &rowRect) const;
    void drawSeparator(QPainter *painter, const QRect &rowRect) const;
    void drawMarkBox(QPainter *painter, const QRect &rowRect, bool marked) const;

    StorageManager *m_storage = nullptr;
    SettingsStore *m_settings = nullptr;
    bool m_dark = false;
    bool m_marksVisible = false;
};
