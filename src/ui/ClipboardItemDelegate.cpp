#include "ui/ClipboardItemDelegate.h"

#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "core/SettingsStore.h"
#include "core/StorageManager.h"

namespace {

// 行内留白
const int kPaddingH = 12;
const int kPaddingV = 7;

// 图标槽和文字之间的间隔
const int kSlotGap = 10;

// 文本行那个占位小方块的边长（不会超过缩略图高度）
const int kPlaceholderSize = 24;

// 图标槽的宽高比：缩略图大多不是正方形，留宽一点免得来回跳
const int kSlotAspectNum = 4;
const int kSlotAspectDen = 3;

// 行高下限，太矮了难看
const int kMinRowHeight = 30;

// 右侧勾选框的边长
const int kMarkBoxSize = 16;

// 勾选状态下左侧那条竖线的宽度
const int kMarkBarWidth = 3;

} // namespace

ClipboardItemDelegate::ClipboardItemDelegate(StorageManager *storage,
                                             SettingsStore *settings,
                                             QObject *parent)
    : QStyledItemDelegate(parent)
    , m_storage(storage)
    , m_settings(settings)
{
}

void ClipboardItemDelegate::setDarkTheme(bool dark)
{
    m_dark = dark;
}

void ClipboardItemDelegate::setMarksVisible(bool visible)
{
    m_marksVisible = visible;
}

int ClipboardItemDelegate::thumbnailHeight() const
{
    return m_settings ? m_settings->thumbnailHeight() : 48;
}

QColor ClipboardItemDelegate::textColor() const
{
    return m_dark ? QColor(0xe6, 0xed, 0xf3) : QColor(0x1f, 0x23, 0x28);
}

QColor ClipboardItemDelegate::mutedColor() const
{
    return m_dark ? QColor(0x8b, 0x94, 0x9e) : QColor(0x57, 0x60, 0x6a);
}

QColor ClipboardItemDelegate::pinColor() const
{
    return m_dark ? QColor(0xef, 0x9f, 0x27) : QColor(0xba, 0x75, 0x17);
}

QColor ClipboardItemDelegate::slotFillColor() const
{
    return m_dark ? QColor(0x21, 0x26, 0x2d) : QColor(0xf3, 0xf5, 0xf8);
}

QColor ClipboardItemDelegate::slotStrokeColor() const
{
    return m_dark ? QColor(0x30, 0x36, 0x3d) : QColor(0xd0, 0xd7, 0xde);
}

QColor ClipboardItemDelegate::markColor() const
{
    return m_dark ? QColor(0x2f, 0x81, 0xf7) : QColor(0x35, 0x7a, 0xbd);
}

QRect ClipboardItemDelegate::gutterRect(const QRect &rowRect) const
{
    const int height = thumbnailHeight();
    const int width = qMax(kPlaceholderSize + 4, height * kSlotAspectNum / kSlotAspectDen);

    // 高度不够的时候（比如缩略图设得很小）按行高收缩
    const int effectiveHeight = qMin(height, qMax(kPlaceholderSize, rowRect.height() - kPaddingV * 2));

    return QRect(rowRect.left() + kPaddingH,
                 rowRect.center().y() - effectiveHeight / 2,
                 width,
                 effectiveHeight);
}

QRect ClipboardItemDelegate::textRect(const QRect &rowRect) const
{
    const QRect slot = gutterRect(rowRect);

    // 勾选框显示时，右边得给它留位置，不然长文本会钻到框底下
    const int rightPad = m_marksVisible ? (kPaddingH + kMarkBoxSize + kSlotGap) : kPaddingH;
    const int left = slot.right() + 1 + kSlotGap;

    return QRect(left,
                 rowRect.top() + kPaddingV,
                 qMax(0, rowRect.right() - rightPad - left + 1),
                 qMax(0, rowRect.height() - kPaddingV * 2));
}

QRect ClipboardItemDelegate::markBoxRect(const QRect &rowRect) const
{
    return QRect(rowRect.right() - kPaddingH - kMarkBoxSize + 1,
                 rowRect.center().y() - kMarkBoxSize / 2,
                 kMarkBoxSize,
                 kMarkBoxSize);
}

void ClipboardItemDelegate::drawMarkBox(QPainter *painter, const QRect &rowRect, bool marked) const
{
    const QRect box = markBoxRect(rowRect);
    const QColor accent = markColor();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (marked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawRoundedRect(QRectF(box), 4, 4);

        // 一个对勾
        QPainterPath check;
        check.moveTo(box.left() + 4.0, box.center().y() + 0.5);
        check.lineTo(box.center().x() - 0.5, box.bottom() - 4.5);
        check.lineTo(box.right() - 3.5, box.top() + 4.5);

        QPen pen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(check);
    } else {
        painter->setPen(QPen(slotStrokeColor(), 1.0));
        painter->setBrush(slotFillColor());
        painter->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }

    painter->restore();
}

void ClipboardItemDelegate::drawPlaceholderSlot(QPainter *painter, const QRect &slot) const
{
    const int size = qMin(kPlaceholderSize, qMin(slot.width(), slot.height()));
    if (size <= 0)
        return;

    // 靠左、垂直居中：这样它和缩略图的左边缘、中线都在同一条线上
    const QRect box(slot.left(), slot.center().y() - size / 2, size, size);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(slotStrokeColor(), 0.5));
    painter->setBrush(slotFillColor());
    painter->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

    // 三条横线，暗示"这是一段文本"
    const QColor line = mutedColor();
    painter->setPen(QPen(line, 1.2, Qt::SolidLine, Qt::RoundCap));
    const int left = box.left() + 5;
    const int right = box.right() - 5;
    const int first = box.top() + size / 3;
    const int step = qMax(3, size / 8);
    for (int i = 0; i < 3; ++i) {
        const int y = first + i * step;
        if (y > box.bottom() - 4)
            break;
        // 最后一行短一点，像段落末尾
        const int lineRight = (i == 2) ? left + (right - left) * 2 / 3 : right;
        painter->drawLine(left, y, lineRight, y);
    }
    painter->restore();
}

void ClipboardItemDelegate::drawRowText(QPainter *painter,
                                        const QRect &textArea,
                                        const ClipboardItem &item,
                                        const QFont &font) const
{
    if (textArea.width() <= 0)
        return;

    painter->save();
    painter->setFont(font);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetrics metrics(font);
    const int baseline = textArea.top() + (textArea.height() - metrics.height()) / 2 + metrics.ascent();

    int x = textArea.left();

    // 置顶标记
    if (item.pinned) {
        painter->setPen(pinColor());
        const QString star = QStringLiteral("★ ");
        painter->drawText(QPoint(x, baseline), star);
        x += metrics.horizontalAdvance(star);
    }

    // 时间：弱化一点，别抢内容的注意力
    const QString timeText = QDateTime::fromMSecsSinceEpoch(item.createdAt)
                                     .toString(QStringLiteral("HH:mm"))
            + QStringLiteral("   ");
    painter->setPen(mutedColor());
    painter->drawText(QPoint(x, baseline), timeText);
    x += metrics.horizontalAdvance(timeText);

    // 内容：剩下的宽度里做省略号，绝不越界
    const int remaining = textArea.right() - x + 1;
    const QString content = metrics.elidedText(item.preview, Qt::ElideRight, qMax(0, remaining));
    painter->setPen(textColor());
    painter->drawText(QPoint(x, baseline), content);

    painter->restore();
}

void ClipboardItemDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // 关键：只让样式画背景 / 选中 / hover，文字图标我们自己来。
    // 这样 QSS 里的 #HistoryList::item:selected 之类照旧生效。
    opt.text.clear();
    opt.icon = QIcon();

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const ClipboardItem item = index.data(ClipboardRoles::ItemRole).value<ClipboardItem>();
    const bool marked = index.data(ClipboardRoles::MarkedRole).toBool();

    const QRect rowRect = option.rect;
    const QRect slot = gutterRect(rowRect);
    const QRect text = textRect(rowRect);

    painter->save();
    painter->setClipRect(rowRect);

    // 勾选态在左边加一条竖线，比整行变色克制，也不会跟选中/hover 的背景打架
    if (marked) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(markColor());
        painter->drawRoundedRect(QRectF(rowRect.left(), rowRect.top() + 3.0,
                                       kMarkBarWidth, rowRect.height() - 6.0),
                                 1.5, 1.5);
        painter->restore();
    }

    if (item.type == ClipboardItem::Image && m_storage) {
        const QPixmap thumb = m_storage->thumbnail(item, thumbnailHeight());
        if (!thumb.isNull()) {
            // 缩略图等比，放在槽位左对齐、垂直居中；不让它撑满，避免被拉变形
            const int maxW = slot.width();
            const int maxH = slot.height();
            QPixmap scaled = thumb;
            if (thumb.width() > maxW || thumb.height() > maxH)
                scaled = thumb.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            const QRect target(slot.left(),
                               slot.center().y() - scaled.height() / 2,
                               scaled.width(),
                               scaled.height());
            painter->drawPixmap(target, scaled);

            // 一像素描边，浅色截图铺在白底上才看得出边界
            painter->setPen(QPen(slotStrokeColor(), 0.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(target).adjusted(-0.5, -0.5, 0.5, 0.5));
        } else {
            // 图片文件被删了 / 读不出来，退化成一个占位块
            drawPlaceholderSlot(painter, slot);
        }
    } else {
        drawPlaceholderSlot(painter, slot);
    }

    drawRowText(painter, text, item, option.font);

    if (m_marksVisible)
        drawMarkBox(painter, rowRect, marked);

    painter->restore();
}

QSize ClipboardItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    const ClipboardItem item = index.data(ClipboardRoles::ItemRole).value<ClipboardItem>();
    const QFontMetrics metrics(option.font);

    int height = metrics.height() + kPaddingV * 2;

    if (item.type == ClipboardItem::Image)
        height = qMax(height, thumbnailHeight() + kPaddingV * 2);

    height = qMax(height, kMinRowHeight);

    // 宽度给 0：让视图自己按视口宽度铺满，不要按内容撑
    return QSize(0, height);
}
