#include "ui/ClipboardItemDelegate.h"

#include <QApplication>
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

// 行高下限，太矮了难看
const int kMinRowHeight = 28;

// 右侧勾选框的边长，以及它跟文字之间的间隔
const int kMarkBoxSize = 16;
const int kSlotGap = 10;

// 选中行左侧那根竖线
const int kSelectionBarWidth = 3;

// 缩略图圆角
const int kImageCornerRadius = 6;

// 图片预览高度 = 原图像素高度 × 这个比例。
// 这是"图片大、占的行高就大"的来源；上限由设置里的"图片预览最大高度"决定。
const double kImageHeightRatio = 1.0 / 8.0;
const int kMinImageHeight = 40;

// 图片尺寸丢失时（老库解析失败）按一张普通截图兜底
const int kFallbackImageWidth = 1920;
const int kFallbackImageHeight = 1080;

// sizeHint 拿到 0 宽时的保守可用宽度
const int kFallbackRowWidth = 400;

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

int ClipboardItemDelegate::maxImageHeight() const
{
    return m_settings ? m_settings->thumbnailHeight() : 180;
}

int ClipboardItemDelegate::rightReserve() const
{
    return m_marksVisible ? (kPaddingH + kMarkBoxSize + kSlotGap) : kPaddingH;
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

QColor ClipboardItemDelegate::frameColor() const
{
    return m_dark ? QColor(0x30, 0x36, 0x3d) : QColor(0xd0, 0xd7, 0xde);
}

QColor ClipboardItemDelegate::accentColor() const
{
    return m_dark ? QColor(0x2f, 0x81, 0xf7) : QColor(0x35, 0x7a, 0xbd);
}

QColor ClipboardItemDelegate::separatorColor() const
{
    return m_dark ? QColor(0x26, 0x2c, 0x33) : QColor(0xec, 0xef, 0xf3);
}

QSize ClipboardItemDelegate::fittedImageSize(const ClipboardItem &item, int availableWidth) const
{
    int naturalWidth = item.imageWidth;
    int naturalHeight = item.imageHeight;
    if (naturalWidth <= 0 || naturalHeight <= 0) {
        naturalWidth = kFallbackImageWidth;
        naturalHeight = kFallbackImageHeight;
    }

    const int cap = qMax(kMinImageHeight, maxImageHeight());

    // 先按原图自身大小定高度，再夹在下限和上限之间
    int height = qBound(kMinImageHeight, qRound(naturalHeight * kImageHeightRatio), cap);
    int width = qMax(1, qRound(double(height) * naturalWidth / naturalHeight));

    // 太宽了就整体缩，保持比例
    if (availableWidth > 0 && width > availableWidth) {
        const double shrink = double(availableWidth) / double(width);
        width = availableWidth;
        height = qMax(1, qRound(height * shrink));
    }

    return QSize(width, height);
}

QRect ClipboardItemDelegate::markBoxRect(const QRect &rowRect) const
{
    return QRect(rowRect.right() - kPaddingH - kMarkBoxSize + 1,
                 rowRect.center().y() - kMarkBoxSize / 2,
                 kMarkBoxSize,
                 kMarkBoxSize);
}

void ClipboardItemDelegate::drawTextRow(QPainter *painter,
                                        const QRect &rowRect,
                                        const ClipboardItem &item,
                                        const QFont &font) const
{
    const QRect textArea(rowRect.left() + kPaddingH,
                         rowRect.top() + kPaddingV,
                         qMax(0, rowRect.width() - kPaddingH - rightReserve() + 1),
                         qMax(0, rowRect.height() - kPaddingV * 2));
    if (textArea.width() <= 0)
        return;

    painter->save();
    painter->setFont(font);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetrics metrics(font);
    const int baseline = textArea.top() + (textArea.height() - metrics.height()) / 2
            + metrics.ascent();

    int x = textArea.left();

    // 置顶标记留着 —— 它是唯一能一眼看出"这条不会过期"的信号
    if (item.pinned) {
        painter->setPen(pinColor());
        const QString star = QStringLiteral("★ ");
        painter->drawText(QPoint(x, baseline), star);
        x += metrics.horizontalAdvance(star);
    }

    const int remaining = qMax(0, textArea.right() - x + 1);
    const QString content = metrics.elidedText(item.preview, Qt::ElideRight, remaining);
    painter->setPen(textColor());
    painter->drawText(QPoint(x, baseline), content);

    painter->restore();
}

void ClipboardItemDelegate::drawImageRow(QPainter *painter,
                                        const QRect &rowRect,
                                        const ClipboardItem &item) const
{
    const int leftLimit = rowRect.left() + kPaddingH;
    const int rightLimit = rowRect.right() - rightReserve() + 1;
    const int available = qMax(40, rightLimit - leftLimit);

    const QSize size = fittedImageSize(item, available);
    if (size.isEmpty())
        return;

    // 在可用区域里居中：水平按区域中点，垂直按行中点
    const QRect target((leftLimit + rightLimit) / 2 - size.width() / 2,
                       rowRect.center().y() - size.height() / 2,
                       size.width(),
                       size.height());

    const QPixmap thumb = m_storage ? m_storage->thumbnail(item, size) : QPixmap();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF frame = QRectF(target).adjusted(0.5, 0.5, -0.5, -0.5);

    if (!thumb.isNull()) {
        // 圆角裁剪只包住贴图这一步，save/restore 会把外层的行裁剪还回来
        painter->save();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(target), kImageCornerRadius, kImageCornerRadius);
        painter->setClipPath(clip, Qt::IntersectClip);
        painter->drawPixmap(target, thumb);
        painter->restore();
    }

    // 描边：浅色截图铺在浅底上，不描边就看不出边界；图片文件丢了时它也是唯一的反馈
    painter->setPen(QPen(frameColor(), 0.8));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(frame, kImageCornerRadius, kImageCornerRadius);

    painter->restore();
}

void ClipboardItemDelegate::drawSelectionBar(QPainter *painter, const QRect &rowRect) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(accentColor());
    painter->drawRoundedRect(QRectF(rowRect.left() + 1.0,
                                   rowRect.top() + 3.0,
                                   kSelectionBarWidth,
                                   qMax(4, rowRect.height() - 6)),
                            1.5, 1.5);
    painter->restore();
}

void ClipboardItemDelegate::drawSeparator(QPainter *painter, const QRect &rowRect) const
{
    painter->save();
    painter->setPen(QPen(separatorColor(), 1));
    const int y = rowRect.bottom();
    painter->drawLine(rowRect.left() + kPaddingH, y, rowRect.right() - kPaddingH, y);
    painter->restore();
}

void ClipboardItemDelegate::drawMarkBox(QPainter *painter, const QRect &rowRect, bool marked) const
{
    const QRect box = markBoxRect(rowRect);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (marked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accentColor());
        painter->drawRoundedRect(QRectF(box), 4, 4);

        QPainterPath check;
        check.moveTo(box.left() + 4.0, box.center().y() + 0.5);
        check.lineTo(box.center().x() - 0.5, box.bottom() - 4.5);
        check.lineTo(box.right() - 3.5, box.top() + 4.5);

        painter->setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(check);
    } else {
        painter->setPen(QPen(frameColor(), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }

    painter->restore();
}

void ClipboardItemDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // 关键：只让样式画背景 / 选中 / hover，内容我们自己来。
    // 这样 QSS 里的 #HistoryList::item:selected 之类照旧生效。
    opt.text.clear();
    opt.icon = QIcon();

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const ClipboardItem item = index.data(ClipboardRoles::ItemRole).value<ClipboardItem>();
    const bool marked = index.data(ClipboardRoles::MarkedRole).toBool();
    const QRect rowRect = option.rect;

    painter->save();
    painter->setClipRect(rowRect);

    if (option.state & QStyle::State_Selected)
        drawSelectionBar(painter, rowRect);

    if (item.isImage())
        drawImageRow(painter, rowRect, item);
    else
        drawTextRow(painter, rowRect, item, option.font);

    if (m_marksVisible)
        drawMarkBox(painter, rowRect, marked);

    // hover / 选中那一行不画分隔线，否则会压在背景药丸上显得脏
    if (!(option.state & (QStyle::State_Selected | QStyle::State_MouseOver)))
        drawSeparator(painter, rowRect);

    painter->restore();
}

QSize ClipboardItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    const ClipboardItem item = index.data(ClipboardRoles::ItemRole).value<ClipboardItem>();
    const QFontMetrics metrics(option.font);

    int height = metrics.height() + kPaddingV * 2;

    if (item.isImage()) {
        // sizeHint 里 option.rect 常常是 0 宽，给个保守值：
        // 行高主要由"图片预览最大高度"决定，宽度只在窗口很窄时才会反过来压缩高度。
        const int available = option.rect.width() > 0
                ? qMax(40, option.rect.width() - kPaddingH - rightReserve())
                : kFallbackRowWidth;

        const QSize size = fittedImageSize(item, available);
        if (!size.isEmpty())
            height = qMax(height, size.height() + kPaddingV * 2);
    }

    // 宽度给 0：让视图自己按视口宽度铺满，不要按内容撑
    return QSize(0, qMax(height, kMinRowHeight));
}
