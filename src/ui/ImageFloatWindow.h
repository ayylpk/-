#pragma once

#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

class QTimer;

/*!
 * 把一张剪切板图片钉在桌面最上层的浮窗。
 *
 * 交互：
 *  - 拖中间：移动窗口
 *  - 拖边框/四角：八个方向都能缩放（无边框窗口没有原生边框，命中判定是自己算的）
 *  - 右上角 × ：关闭（无边框窗口不能只靠 Esc —— 没焦点时收不到按键）
 *  - 悬停时鼠标会变成对应的缩放箭头
 *
 * 几个刻意的选择：
 *  - **初始尺寸 = 图片的 1:1 大小**（1 图像像素 = 1 物理屏幕像素，所以高 DPI 下要除以 dpr），
 *    比屏幕还大就先缩到屏幕的可用范围，否则一打开就有半边在屏幕外没法用。
 *  - **图片持有一份内存副本**：FIFO 淘汰会把文件删掉，浮窗不能因此变空。
 *  - 底色/边框跟随主题，但**不跟随"背景不透明度"** —— 看图的时候窗口透过去没意义。
 *  - 缩放过程用 Fast 变换，停手一小段时间后再用 Smooth 重算一遍，兼顾流畅和画质。
 */
class ImageFloatWindow : public QWidget
{
    Q_OBJECT

public:
    ImageFloatWindow(qint64 itemId, const QImage &image, bool darkTheme, QWidget *parent = nullptr);

    qint64 itemId() const { return m_itemId; }
    void setDarkTheme(bool dark);

    /*! 让 Application 把多张图错开摆放，免得全叠在同一个位置。 */
    void placeAt(const QPoint &topLeft);

    QSize sizeHint() const override;

signals:
    void closed(qint64 itemId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    /*! 命中的是哪条边／哪个角，按位组合。用裸 int 而不是 QFlags，
     *  因为这个枚举是私有的，Q_DECLARE_OPERATORS_FOR_FLAGS 在命名空间作用域下取不到。 */
    enum EdgeFlag {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8
    };

    int edgeAt(const QPoint &localPos) const;
    void applyCursorFor(int edge);
    QRect closeButtonRect() const;
    bool hitCloseButton(const QPoint &localPos) const;

    void applyDrag(const QPoint &globalPos);
    void applyResize(const QPoint &globalPos);
    void rebuildScaledPixmap(bool smooth);

    /*! 按 1:1（物理像素）算出窗口该多大，再夹进最小尺寸和屏幕可用范围之间。 */
    static QSize initialWindowSize(const QImage &image, qreal devicePixelRatio, const QSize &screenLimit);

    qint64 m_itemId = -1;
    QImage m_original;
    QPixmap m_scaled;
    bool m_dark = false;

    QTimer *m_smoothTimer = nullptr;

    // 拖动 / 缩放状态
    bool m_interacting = false;
    int m_activeEdge = NoEdge;
    QPoint m_pressGlobal;
    QRect m_pressGeometry;

    // 关闭按钮的悬停 / 按下状态
    bool m_closeHovered = false;
    bool m_closePressed = false;
};
