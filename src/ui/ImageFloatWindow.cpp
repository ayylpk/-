#include "ui/ImageFloatWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>

namespace {

// 判定"按在边框上"的厚度
const int kEdgeMargin = 8;

// 窗口最小尺寸：无边框窗口能一路拉到 0，不给下限就会拉到找不回来
const int kMinWidth = 160;
const int kMinHeight = 120;

// 右上角关闭按钮
const int kCloseButtonSize = 22;
const int kCloseButtonMargin = 6;

// 拖完手多久之后补一次平滑缩放
const int kSmoothDelayMs = 200;

// 初始尺寸最多占屏幕可用区域的比例
const double kMaxScreenRatio = 0.9;

} // namespace

ImageFloatWindow::ImageFloatWindow(qint64 itemId, const QImage &image, bool darkTheme, QWidget *parent)
    : QWidget(parent)
    , m_itemId(itemId)
    , m_original(image)
    , m_dark(darkTheme)
{
    // Tool 不在任务栏留条目；无边框 + 置顶才能"钉在桌面上"
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setObjectName(QStringLiteral("FloatWindow"));
    setWindowTitle(QStringLiteral("图片预览"));
    setMinimumSize(kMinWidth, kMinHeight);
    // 不吃焦点的话收不到 Esc，但收焦点又会让用户原来的窗口失去焦点。
    // 折中：给 StrongFocus，Esc 能用；关闭主要靠右上角的 ×。
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_smoothTimer = new QTimer(this);
    m_smoothTimer->setSingleShot(true);
    m_smoothTimer->setInterval(kSmoothDelayMs);
    connect(m_smoothTimer, &QTimer::timeout, this, [this]() {
        rebuildScaledPixmap(true);
    });

    // 定位到鼠标所在屏幕，取它的可用区域和缩放比
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);

    const QSize limit(qMax(kMinWidth, int(available.width() * kMaxScreenRatio)),
                      qMax(kMinHeight, int(available.height() * kMaxScreenRatio)));

    resize(initialWindowSize(m_original, dpr, limit));
    rebuildScaledPixmap(true);
}

QSize ImageFloatWindow::initialWindowSize(const QImage &image, qreal devicePixelRatio,
                                          const QSize &screenLimit)
{
    if (image.isNull())
        return QSize(kMinWidth, kMinHeight);

    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;

    // 关键：1 图像像素 = 1 物理屏幕像素。高 DPI 下窗口的逻辑尺寸要除以 dpr，
    // 不然 1920 宽的截图在 200% 缩放的屏幕上会开出 3840 物理像素、直接超屏。
    QSize logical(qRound(image.width() / dpr), qRound(image.height() / dpr));

    // 比屏幕还大就先等比缩到屏幕可用范围内
    if (logical.width() > screenLimit.width() || logical.height() > screenLimit.height()) {
        logical.scale(screenLimit, Qt::KeepAspectRatio);
    }

    return QSize(qMax(kMinWidth, logical.width()), qMax(kMinHeight, logical.height()));
}

void ImageFloatWindow::setDarkTheme(bool dark)
{
    if (m_dark == dark)
        return;

    m_dark = dark;
    update();
}

void ImageFloatWindow::placeAt(const QPoint &topLeft)
{
    QScreen *screen = QGuiApplication::screenAt(topLeft);
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    if (!screen) {
        move(topLeft);
        return;
    }

    // 夹进屏幕：多张错开摆放时很容易把后开的顶到屏幕外面去
    const QRect area = screen->availableGeometry();
    int x = topLeft.x();
    int y = topLeft.y();

    if (x + width() > area.right())
        x = area.right() - width();
    if (y + height() > area.bottom())
        y = area.bottom() - height();

    move(qMax(area.left(), x), qMax(area.top(), y));
}

QSize ImageFloatWindow::sizeHint() const
{
    return size();
}

QRect ImageFloatWindow::closeButtonRect() const
{
    return QRect(width() - kCloseButtonMargin - kCloseButtonSize,
                 kCloseButtonMargin,
                 kCloseButtonSize,
                 kCloseButtonSize);
}

bool ImageFloatWindow::hitCloseButton(const QPoint &localPos) const
{
    return closeButtonRect().contains(localPos);
}

int ImageFloatWindow::edgeAt(const QPoint &localPos) const
{
    int edge = NoEdge;

    if (localPos.x() <= kEdgeMargin)
        edge |= LeftEdge;
    else if (localPos.x() >= width() - kEdgeMargin)
        edge |= RightEdge;

    if (localPos.y() <= kEdgeMargin)
        edge |= TopEdge;
    else if (localPos.y() >= height() - kEdgeMargin)
        edge |= BottomEdge;

    return edge;
}

void ImageFloatWindow::applyCursorFor(int edge)
{
    switch (edge) {
    case LeftEdge | TopEdge:
    case RightEdge | BottomEdge:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case RightEdge | TopEdge:
    case LeftEdge | BottomEdge:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case LeftEdge:
    case RightEdge:
        setCursor(Qt::SizeHorCursor);
        break;
    case TopEdge:
    case BottomEdge:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void ImageFloatWindow::rebuildScaledPixmap(bool smooth)
{
    if (m_original.isNull() || width() <= 0 || height() <= 0) {
        m_scaled = QPixmap();
        update();
        return;
    }

    const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;

    // 按物理像素去缩放，再给 pixmap 标上 dpr —— 这样高 DPI 屏上不会糊
    const QSize targetPhysical(qRound(width() * dpr), qRound(height() * dpr));

    const Qt::TransformationMode mode = smooth ? Qt::SmoothTransformation
                                               : Qt::FastTransformation;

    m_scaled = QPixmap::fromImage(m_original.scaled(targetPhysical, Qt::KeepAspectRatio, mode));
    m_scaled.setDevicePixelRatio(dpr);

    update();
}

void ImageFloatWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 拖动过程中先给一版快的，停手之后再补一版平滑的
    rebuildScaledPixmap(false);
    m_smoothTimer->start();
}

void ImageFloatWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const QColor fill = m_dark ? QColor(0x1c, 0x21, 0x28) : QColor(0xf9, 0xfa, 0xfb);
    const QColor stroke = m_dark ? QColor(0x3d, 0x44, 0x4d) : QColor(0xb9, 0xc4, 0xd0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.fillRect(rect(), fill);

    if (!m_scaled.isNull()) {
        // deviceIndependentSize 是逻辑尺寸，直接拿来居中；实际绘制由 dpr 还原到物理像素
        const QSize logical = m_scaled.deviceIndependentSize().toSize();
        const QPoint topLeft(rect().center().x() - logical.width() / 2,
                             rect().center().y() - logical.height() / 2);
        painter.drawPixmap(topLeft, m_scaled);
    }

    // 关闭按钮：底下垫一层半透明黑片，浅色图和深色图上都看得见
    const QRect close = closeButtonRect();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_closeHovered ? QColor(0, 0, 0, 190) : QColor(0, 0, 0, 120));
    painter.drawRoundedRect(QRectF(close), 5, 5);

    painter.setPen(QPen(QColor(0xff, 0xff, 0xff, 230), 1.5,
                        Qt::SolidLine, Qt::RoundCap));
    const int inset = 6;
    painter.drawLine(close.left() + inset, close.top() + inset,
                     close.right() - inset, close.bottom() - inset);
    painter.drawLine(close.right() - inset, close.top() + inset,
                     close.left() + inset, close.bottom() - inset);
    painter.restore();

    painter.setPen(stroke);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ImageFloatWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint local = event->position().toPoint();
    const QPoint global = event->globalPosition().toPoint();

    if (hitCloseButton(local)) {
        m_closePressed = true;
        event->accept();
        return;
    }

    m_pressGlobal = global;
    m_pressGeometry = geometry();
    m_activeEdge = edgeAt(local);
    m_interacting = true;

    event->accept();
}

void ImageFloatWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint local = event->position().toPoint();
    const QPoint global = event->globalPosition().toPoint();

    if (!m_interacting) {
        // 没在拖的时候只负责换光标和关闭按钮的悬停态
        const bool hoveringClose = hitCloseButton(local);
        if (hoveringClose != m_closeHovered) {
            m_closeHovered = hoveringClose;
            update();
        }

        if (!hoveringClose)
            applyCursorFor(edgeAt(local));

        QWidget::mouseMoveEvent(event);
        return;
    }

    if (m_activeEdge != NoEdge)
        applyResize(global);
    else
        applyDrag(global);

    event->accept();
}

void ImageFloatWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_closePressed) {
        m_closePressed = false;
        if (hitCloseButton(event->position().toPoint())) {
            event->accept();
            close();
            return;
        }
    }

    const bool wasInteracting = m_interacting;
    m_interacting = false;
    m_activeEdge = NoEdge;

    // 松手后立刻补一版平滑的，不用等定时器
    if (wasInteracting)
        rebuildScaledPixmap(true);

    event->accept();
}

void ImageFloatWindow::leaveEvent(QEvent *event)
{
    if (m_closeHovered) {
        m_closeHovered = false;
        update();
    }
    setCursor(Qt::ArrowCursor);

    QWidget::leaveEvent(event);
}

void ImageFloatWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        close();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ImageFloatWindow::closeEvent(QCloseEvent *event)
{
    emit closed(m_itemId);
    QWidget::closeEvent(event);
}

void ImageFloatWindow::applyDrag(const QPoint &globalPos)
{
    move(globalPos - (m_pressGlobal - m_pressGeometry.topLeft()));
}

void ImageFloatWindow::applyResize(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_pressGlobal;
    QRect target = m_pressGeometry;

    if (m_activeEdge & LeftEdge)
        target.setLeft(target.left() + delta.x());
    if (m_activeEdge & RightEdge)
        target.setRight(target.right() + delta.x());
    if (m_activeEdge & TopEdge)
        target.setTop(target.top() + delta.y());
    if (m_activeEdge & BottomEdge)
        target.setBottom(target.bottom() + delta.y());

    // 撞到最小尺寸时要把对应那条边顶回来，
    // 否则窗口"粘"在最小尺寸上、鼠标继续动也没反应，松手前会看着像卡住。
    if (target.width() < kMinWidth) {
        if (m_activeEdge & LeftEdge)
            target.setLeft(target.right() - kMinWidth + 1);
        else
            target.setRight(target.left() + kMinWidth - 1);
    }
    if (target.height() < kMinHeight) {
        if (m_activeEdge & TopEdge)
            target.setTop(target.bottom() - kMinHeight + 1);
        else
            target.setBottom(target.top() + kMinHeight - 1);
    }

    setGeometry(target);
}
