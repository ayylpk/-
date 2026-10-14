#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include "core/SettingsStore.h"
#include "core/StorageManager.h"
#include "ui/ClipboardItemDelegate.h"

namespace {

// 拖动判定：超过系统拖拽阈值才算"拖"，否则当成点击
// （阈值取 QApplication::startDragDistance()）

// 弹窗和屏幕边缘保持的距离
const int kScreenMargin = 8;

// 窗口内容四周的留白（边框画在最外圈，内容缩进来一点）
const int kWindowPadding = 6;

} // namespace

MainWindow::MainWindow(StorageManager *storage, SettingsStore *settings, QWidget *parent)
    : QWidget(parent)
    , m_storage(storage)
    , m_settings(settings)
{
    // Tool 不会在任务栏留条目；Frameless + 置顶 是小弹窗的标配。
    // 不做半透明：一是没必要，二是省得在不同显卡驱动上出黑角。
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    // QWidget 的子类必须显式打开这个，QSS 里的 background / border 才会被绘制。
    // 窗口自己的底色和边框不走 QSS，直接由 paintEvent 画（见下），这里留着是为了子控件。
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("ListWindow"));
    setWindowTitle(QStringLiteral("剪切板历史"));

    m_darkTheme = m_settings && m_settings->theme() == SettingsStore::Dark;

    resize(m_settings ? m_settings->listWidth() : 600,
           m_settings ? m_settings->listHeight() : 520);

    buildUi();

    if (m_storage) {
        connect(m_storage, &StorageManager::itemInserted, this, &MainWindow::onItemInserted);
        connect(m_storage, &StorageManager::itemChanged, this, &MainWindow::onItemChanged);
        connect(m_storage, &StorageManager::itemRemoved, this, &MainWindow::onItemRemoved);
        connect(m_storage, &StorageManager::resetPerformed, this, &MainWindow::onResetPerformed);
    }

    if (m_settings) {
        connect(m_settings, &SettingsStore::thumbnailHeightChanged,
                this, &MainWindow::onThumbnailHeightChanged);
        connect(m_settings, &SettingsStore::themeChanged,
                this, &MainWindow::onThemeChanged);
        connect(m_settings, &SettingsStore::listWidthChanged,
                this, &MainWindow::onListSizeChanged);
        connect(m_settings, &SettingsStore::listHeightChanged,
                this, &MainWindow::onListSizeChanged);
        connect(m_settings, &SettingsStore::backgroundOpacityChanged,
                this, &MainWindow::onBackgroundOpacityChanged);
    }

    // 放在最后：切半透明窗口要求窗口还没显示过
    applyBackgroundOpacity();

    rebuildList();
}

void MainWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(kWindowPadding, kWindowPadding, kWindowPadding, kWindowPadding);
    root->setSpacing(0);

    auto *topBar = new QHBoxLayout;
    topBar->setContentsMargins(kWindowPadding, 2, kWindowPadding, 0);
    topBar->setSpacing(6);

    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("ListHeader"));

    m_settingsButton = new QPushButton(QStringLiteral("设置"), this);
    m_settingsButton->setFlat(true);
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setFocusPolicy(Qt::NoFocus);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::settingsRequested);

    topBar->addWidget(m_header, 1);
    topBar->addWidget(m_settingsButton, 0);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("HistoryList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setWordWrap(false);
    m_list->setUniformItemSizes(false);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->installEventFilter(this);

    // 文本和图片混排的对齐问题交给 delegate 处理
    m_delegate = new ClipboardItemDelegate(m_storage, m_settings, this);
    m_delegate->setDarkTheme(m_darkTheme);
    m_list->setItemDelegate(m_delegate);

    connect(m_list, &QListWidget::itemPressed, this, &MainWindow::onRowPressed);
    connect(m_list, &QListWidget::itemClicked, this, &MainWindow::onRowClicked);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onRowDoubleClicked);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);

    // 勾选栏：平时藏着，有勾选项才出现
    m_markBar = new QWidget(this);
    m_markBar->setObjectName(QStringLiteral("MarkBar"));
    auto *markLayout = new QHBoxLayout(m_markBar);
    markLayout->setContentsMargins(10, 6, 10, 6);
    markLayout->setSpacing(8);

    m_markLabel = new QLabel(m_markBar);
    m_markLabel->setObjectName(QStringLiteral("MarkLabel"));

    m_clearMarksButton = new QPushButton(QStringLiteral("取消勾选"), m_markBar);
    m_clearMarksButton->setFocusPolicy(Qt::NoFocus);
    connect(m_clearMarksButton, &QPushButton::clicked, this, &MainWindow::onClearMarks);

    m_deleteMarkedButton = new QPushButton(QStringLiteral("删除选中"), m_markBar);
    m_deleteMarkedButton->setObjectName(QStringLiteral("DangerButton"));
    m_deleteMarkedButton->setFocusPolicy(Qt::NoFocus);
    connect(m_deleteMarkedButton, &QPushButton::clicked, this, &MainWindow::onDeleteMarked);

    markLayout->addWidget(m_markLabel, 1);
    markLayout->addWidget(m_clearMarksButton, 0);
    markLayout->addWidget(m_deleteMarkedButton, 0);
    m_markBar->setVisible(false);

    // 列表为空时铺在整个列表区上的一句引导（跟着 viewport 一起改大小，见 eventFilter）
    m_emptyLabel = new QLabel(m_list->viewport());
    m_emptyLabel->setObjectName(QStringLiteral("EmptyHint"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setText(QStringLiteral("还没有记录\n复制点什么就会出现在这里"));
    // 别把鼠标事件吃掉：那一块还要能用来拖动窗口
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_emptyLabel->setGeometry(m_list->viewport()->rect());
    m_emptyLabel->hide();

    // 底部提示做成键帽，扫一眼就知道按什么键
    auto *hintBar = new QWidget(this);
    hintBar->setObjectName(QStringLiteral("HintBar"));
    auto *hintLayout = new QHBoxLayout(hintBar);
    hintLayout->setContentsMargins(kWindowPadding, 2, kWindowPadding, 8);
    hintLayout->setSpacing(6);

    auto addKey = [hintBar, hintLayout](const QString &key, const QString &desc,
                                        const QString &tooltip) {
        auto *cap = new QLabel(key, hintBar);
        cap->setObjectName(QStringLiteral("KeyCap"));
        auto *text = new QLabel(desc, hintBar);
        text->setObjectName(QStringLiteral("KeyDesc"));

        if (!tooltip.isEmpty()) {
            cap->setToolTip(tooltip);
            text->setToolTip(tooltip);
        }

        hintLayout->addWidget(cap, 0);
        hintLayout->addWidget(text, 0);
    };

    addKey(QStringLiteral("Enter"), QStringLiteral("使用"),
           QStringLiteral("空格同义：都是把这条送回光标处，用完自动收窗"));
    hintLayout->addSpacing(10);
    addKey(QStringLiteral("Ctrl+点击"), QStringLiteral("勾选"),
           QStringLiteral("也可以按 Ctrl+空格，或右键菜单里选「勾选」"));
    hintLayout->addSpacing(10);
    addKey(QStringLiteral("P"), QStringLiteral("置顶"), QString());
    hintLayout->addSpacing(10);
    addKey(QStringLiteral("Del"), QStringLiteral("删除"), QString());
    hintLayout->addSpacing(10);
    addKey(QStringLiteral("Esc"), QStringLiteral("关闭"), QString());
    hintLayout->addStretch(1);

    root->addLayout(topBar);
    root->addWidget(m_list, 1);
    root->addWidget(m_markBar, 0);
    root->addWidget(hintBar, 0);
}

void MainWindow::onThumbnailHeightChanged(int height)
{
    Q_UNUSED(height);

    // 行高和槽位宽度都从 SettingsStore 现取，这里只要重新算一遍布局
    rebuildList();
}

void MainWindow::onThemeChanged(SettingsStore::Theme theme)
{
    m_darkTheme = (theme == SettingsStore::Dark);

    if (m_delegate)
        m_delegate->setDarkTheme(m_darkTheme);

    // 窗口自己画的底色和边框也要跟着换
    update();
}

void MainWindow::onListSizeChanged(int value)
{
    Q_UNUSED(value);

    if (!m_settings)
        return;

    resize(m_settings->listWidth(), m_settings->listHeight());
}

void MainWindow::onBackgroundOpacityChanged(int value)
{
    Q_UNUSED(value);

    applyBackgroundOpacity();
}

void MainWindow::toggleMark(qint64 id)
{
    if (id <= 0)
        return;

    const int index = m_marked.indexOf(id);
    if (index >= 0)
        m_marked.removeAt(index);
    else
        m_marked.append(id);

    // 行数据里的勾选标记跟着更新
    if (m_list) {
        const int rowIndex = rowIndexById(id);
        if (rowIndex >= 0)
            m_list->item(rowIndex)->setData(ClipboardRoles::MarkedRole, index < 0);
    }

    updateMarkBar();
}

void MainWindow::updateMarkBar()
{
    const int markedCount = m_marked.size();
    const bool hasMarks = markedCount > 0;

    if (m_markBar)
        m_markBar->setVisible(hasMarks);

    if (m_delegate)
        m_delegate->setMarksVisible(hasMarks);

    if (m_markLabel) {
        m_markLabel->setText(QStringLiteral("已勾选 %1 条").arg(markedCount));
    }

    if (m_deleteMarkedButton) {
        m_deleteMarkedButton->setText(hasMarks
                                              ? QStringLiteral("删除选中 (%1)").arg(markedCount)
                                              : QStringLiteral("删除选中"));
        m_deleteMarkedButton->setEnabled(hasMarks);
    }

    // 勾选框显示/隐藏会改变文字可用宽度，得重绘
    if (m_list)
        m_list->viewport()->update();
}

void MainWindow::onClearMarks()
{
    m_marked.clear();

    // 行数据里的标记也要一起清掉（要在下面那次重绘之前清完）
    if (m_list) {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setData(ClipboardRoles::MarkedRole, false);
    }

    updateMarkBar();
}

void MainWindow::onDeleteMarked()
{
    if (!m_storage || m_marked.isEmpty())
        return;

    const int markedCount = m_marked.size();

    const auto answer = QMessageBox::question(
            this,
            QStringLiteral("批量删除"),
            QStringLiteral("要删掉勾选的 %1 条记录吗？\n图片文件会一起删除，删掉之后无法恢复。")
                    .arg(markedCount),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    const int removed = m_storage->removeItems(m_marked);
    m_marked.clear();
    updateMarkBar();

    if (removed == 0 && m_header)
        m_header->setText(QStringLiteral("删除失败，记录还在（数据库没能写入）"));
}

QListWidgetItem *MainWindow::createRow(const ClipboardItem &item) const
{
    auto *row = new QListWidgetItem;
    applyRowContent(row, item);
    return row;
}

void MainWindow::applyRowContent(QListWidgetItem *row, const ClipboardItem &item) const
{
    if (!row)
        return;

    // 整行怎么画由 ClipboardItemDelegate 决定，这里只把数据挂上去
    row->setData(ClipboardRoles::IdRole, item.id);
    row->setData(ClipboardRoles::TypeRole, static_cast<int>(item.type));
    row->setData(ClipboardRoles::ItemRole, QVariant::fromValue(item));
    row->setData(ClipboardRoles::MarkedRole, m_marked.contains(item.id));

    // 文本本身不显示（delegate 自己画时间和摘要），
    // 留在这里是为了 tooltip、无障碍和以后加搜索的时候有内容可用
    row->setText(item.preview);
    row->setToolTip(item.isImage() ? item.preview : item.content.left(4000));
}

void MainWindow::rebuildList()
{
    if (!m_list || !m_storage)
        return;

    m_list->clear();

    const QVector<ClipboardItem> &items = m_storage->items();
    for (const ClipboardItem &item : items)
        m_list->addItem(createRow(item));

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    updateHeader();
}

void MainWindow::refreshAll()
{
    rebuildList();
}

void MainWindow::updateHeader()
{
    if (!m_header || !m_storage)
        return;

    const bool empty = m_storage->isEmpty();

    // 空列表不再只留一行小字，铺一句居中的引导
    if (m_emptyLabel)
        m_emptyLabel->setVisible(empty);

    if (empty) {
        m_header->setText(QStringLiteral("剪切板历史"));
        return;
    }

    m_header->setText(QStringLiteral("剪切板历史 · %1 条 · 置顶 %2 条")
                              .arg(m_storage->count())
                              .arg(m_storage->pinnedCount()));
}

int MainWindow::rowIndexById(qint64 id) const
{
    if (!m_list)
        return -1;

    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(ClipboardRoles::IdRole).toLongLong() == id)
            return i;
    }
    return -1;
}

void MainWindow::onItemInserted(int index, const ClipboardItem &item)
{
    if (!m_list)
        return;

    const int safeIndex = qBound(0, index, m_list->count());
    m_list->insertItem(safeIndex, createRow(item));

    if (!isVisible())
        m_list->setCurrentRow(safeIndex);

    updateHeader();
}

void MainWindow::onItemChanged(int index, const ClipboardItem &item)
{
    if (!m_list || index < 0 || index >= m_list->count())
        return;

    applyRowContent(m_list->item(index), item);
    updateHeader();
}

void MainWindow::onItemRemoved(qint64 id)
{
    if (!m_list)
        return;

    // 被删的记录如果正在勾选列表里，顺手摘掉，免得留下无效 id
    const int markedIndex = m_marked.indexOf(id);
    if (markedIndex >= 0) {
        m_marked.removeAt(markedIndex);
        updateMarkBar();
    }

    const int index = rowIndexById(id);
    if (index >= 0)
        delete m_list->takeItem(index);

    if (m_list->count() > 0 && !m_list->currentItem())
        m_list->setCurrentRow(qMin(index, m_list->count() - 1));

    updateHeader();
}

void MainWindow::onResetPerformed()
{
    rebuildList();
}

void MainWindow::onRowPressed(QListWidgetItem *row)
{
    if (!row)
        return;

    // Ctrl+点击 = 勾选待删，不当成"使用"。
    // 触发在按下时，所以要吃掉随之而来的那次 click。
    if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        m_swallowNextClick = true;
        toggleMark(row->data(ClipboardRoles::IdRole).toLongLong());
    } else {
        // 上一次 Ctrl+点击如果松手时跑到了行外面，这里把残留的标记清掉，
        // 免得它把下一次正常点击吃掉
        m_swallowNextClick = false;
    }
}

void MainWindow::onRowClicked(QListWidgetItem *row)
{
    if (!row)
        return;

    // Ctrl+点击那一下已经在 onRowPressed 里处理掉了
    if (m_swallowNextClick) {
        m_swallowNextClick = false;
        return;
    }

    if (!m_settings || m_settings->pasteOnClick())
        emit itemActivated(row->data(ClipboardRoles::IdRole).toLongLong());
}

void MainWindow::onRowDoubleClicked(QListWidgetItem *row)
{
    if (!row)
        return;

    // 双击也走"使用"，但 Ctrl+双击仍然是勾选语义，别误粘贴
    if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
        return;

    emit itemActivated(row->data(ClipboardRoles::IdRole).toLongLong());
}

void MainWindow::activateCurrentRow()
{
    QListWidgetItem *row = m_list ? m_list->currentItem() : nullptr;
    if (!row)
        return;

    emit itemActivated(row->data(ClipboardRoles::IdRole).toLongLong());
}

void MainWindow::togglePinCurrentRow()
{
    QListWidgetItem *row = m_list ? m_list->currentItem() : nullptr;
    if (!row || !m_storage)
        return;

    const qint64 id = row->data(ClipboardRoles::IdRole).toLongLong();

    bool pinned = false;
    bool found = false;
    const QVector<ClipboardItem> &items = m_storage->items();
    for (const ClipboardItem &item : items) {
        if (item.id == id) {
            pinned = item.pinned;
            found = true;
            break;
        }
    }

    if (!found)
        return;

    m_storage->setPinned(id, !pinned);
}

void MainWindow::removeCurrentRow()
{
    QListWidgetItem *row = m_list ? m_list->currentItem() : nullptr;
    if (!row || !m_storage)
        return;

    m_storage->removeItem(row->data(ClipboardRoles::IdRole).toLongLong());
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *row = m_list ? m_list->itemAt(pos) : nullptr;
    if (!row || !m_storage)
        return;

    if (m_list)
        m_list->setCurrentItem(row);

    const qint64 id = row->data(ClipboardRoles::IdRole).toLongLong();

    bool pinned = false;
    const QVector<ClipboardItem> &items = m_storage->items();
    for (const ClipboardItem &item : items) {
        if (item.id == id) {
            pinned = item.pinned;
            break;
        }
    }

    const bool marked = m_marked.contains(id);
    const bool isImage = row->data(ClipboardRoles::TypeRole).toInt() == ClipboardItem::Image;

    QMenu menu(this);
    // QSS 的 item padding 对菜单宽度的影响不总是可靠，这里再兜一个最小宽度
    menu.setMinimumWidth(170);
    QAction *floatAction = nullptr;
    if (isImage)
        floatAction = menu.addAction(QStringLiteral("浮动到桌面"));
    QAction *markAction = menu.addAction(marked ? QStringLiteral("取消勾选")
                                                : QStringLiteral("勾选（用于批量删除）"));
    QAction *pinAction = menu.addAction(pinned ? QStringLiteral("取消置顶")
                                               : QStringLiteral("置顶"));
    QAction *removeAction = menu.addAction(QStringLiteral("删除这条"));
    menu.addSeparator();
    QAction *dataDirAction = menu.addAction(QStringLiteral("打开数据目录"));
    QAction *settingsAction = menu.addAction(QStringLiteral("设置…"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (floatAction && chosen == floatAction)
        emit imageFloatRequested(id);
    else if (chosen == markAction)
        toggleMark(id);
    else if (chosen == pinAction)
        togglePinCurrentRow();
    else if (chosen == removeAction)
        m_storage->removeItem(id);
    else if (chosen == dataDirAction)
        emit dataDirRequested();
    else if (chosen == settingsAction)
        emit settingsRequested();
}

void MainWindow::clampToScreen(const QPoint &anchor)
{
    QScreen *screen = QGuiApplication::screenAt(anchor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();

    // 比屏幕还大的窗口是没法用的，先收进来（只在超出时才动，不覆盖用户设的尺寸）
    const int maxW = area.width() - kScreenMargin * 2;
    const int maxH = area.height() - kScreenMargin * 2;
    int w = width();
    int h = height();
    if (w > maxW)
        w = maxW;
    if (h > maxH)
        h = maxH;
    if (w != width() || h != height())
        resize(w, h);

    int x = anchor.x() + 14;
    int y = anchor.y() + 14;

    if (x + w > area.right() - kScreenMargin)
        x = area.right() - kScreenMargin - w;
    if (y + h > area.bottom() - kScreenMargin)
        y = area.bottom() - kScreenMargin - h;

    x = qMax(area.left() + kScreenMargin, x);
    y = qMax(area.top() + kScreenMargin, y);

    move(x, y);
}

void MainWindow::popupAtCursor()
{
    rebuildList();
    clampToScreen(QCursor::pos());

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);

    if (m_list) {
        m_list->setFocus(Qt::OtherFocusReason);
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateHeader();
}

/*!
 * 窗口的底色和边框在这里自己画，不走 QSS。
 *
 * 原因是之前把 WA_TranslucentBackground 和 QSS 的 #ListWindow 背景搭在一起时，
 * 背景压根没画出来，整个窗口是透的、连边框都看不见。
 * 自己画最省心，也不依赖 QSS 有没有被正确解析。
 *
 * 不透明度就是在这里加 alpha：100% 时窗口本身是不透明的（不透桌面），
 * 低于 100% 时 windowFlags 那边开了 WA_TranslucentBackground，alpha 才有意义。
 */
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const int opacity = m_settings ? m_settings->backgroundOpacity() : 100;
    const int alpha = qBound(0, opacity * 255 / 100, 255);

    QColor fill = m_darkTheme ? QColor(0x1c, 0x21, 0x28) : QColor(0xf9, 0xfa, 0xfb);
    // 描边比内部用的分隔线深一档：不透明的无边框窗口没有阴影，
    // 边框不够重的话整块会糊在桌面里看不出边界。
    QColor stroke = m_darkTheme ? QColor(0x3d, 0x44, 0x4d) : QColor(0xb9, 0xc4, 0xd0);
    fill.setAlpha(alpha);
    stroke.setAlpha(alpha);

    QPainter painter(this);
    painter.fillRect(rect(), fill);

    painter.setPen(stroke);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MainWindow::applyBackgroundOpacity()
{
    if (!m_settings)
        return;

    const bool translucent = m_settings->backgroundOpacity() < 100;

    // WA_TranslucentBackground 必须在窗口显示之前设好，
    // 运行中要改就先藏起来、设完再显示回来。
    const bool wasVisible = isVisible();
    if (wasVisible)
        hide();

    setAttribute(Qt::WA_TranslucentBackground, translucent);

    if (wasVisible)
        show();

    update();
}

void MainWindow::beginDrag(const QPoint &globalPos)
{
    m_dragPressGlobal = globalPos;
    m_dragAnchor = globalPos - pos();
    m_dragging = false;
}

bool MainWindow::updateDrag(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_dragPressGlobal;

    // 没超过系统阈值就还不算拖 —— 否则轻微抖动会让点击失效
    if (!m_dragging && delta.manhattanLength() < QApplication::startDragDistance())
        return false;

    m_dragging = true;
    move(globalPos - m_dragAnchor);
    return true;
}

void MainWindow::endDrag()
{
    m_dragging = false;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        beginDrag(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton) && updateDrag(event->globalPosition().toPoint())) {
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    endDrag();
    QWidget::mouseReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 在列表上也能拖窗口：拖动超过阈值就移动，否则照旧算点击
    if (m_list && watched == m_list->viewport()) {
        switch (event->type()) {
        case QEvent::Resize:
            // 空列表的引导要一直铺满整个列表区，所以得跟着 viewport 改大小
            if (m_emptyLabel)
                m_emptyLabel->setGeometry(m_list->viewport()->rect());
            break;
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
                beginDrag(mouseEvent->globalPosition().toPoint());
            break;
        }
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->buttons().testFlag(Qt::LeftButton) && updateDrag(mouseEvent->globalPosition().toPoint())) {
                // 已经在拖了，这一串鼠标事件不能再让列表当成点击处理
                m_swallowNextClick = true;
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            endDrag();
            break;
        default:
            break;
        }
    }

    if (watched == m_list && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateCurrentRow();
            return true;
        case Qt::Key_Space:
            // 空格和回车同义：用当前这条，用完自动收窗。
            // 想用键盘勾选就按 Ctrl+空格。
            if (keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
                if (QListWidgetItem *current = m_list->currentItem()) {
                    toggleMark(current->data(ClipboardRoles::IdRole).toLongLong());
                    return true;
                }
                break;
            }
            activateCurrentRow();
            return true;
        case Qt::Key_Delete:
            removeCurrentRow();
            return true;
        case Qt::Key_P:
            if (keyEvent->modifiers() == Qt::NoModifier || keyEvent->modifiers() == Qt::ControlModifier) {
                togglePinCurrentRow();
                return true;
            }
            break;
        case Qt::Key_F5:
            refreshAll();
            return true;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}
