#pragma once

#include <QVector>
#include <QWidget>

#include "core/ClipboardItem.h"
#include "core/SettingsStore.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class ClipboardItemDelegate;
class StorageManager;

/*!
 * 剪切板历史列表。
 *
 * 无边框 + 置顶的小弹窗：热键唤出 → 鼠标在哪弹在哪 → 点一下就把内容送回光标处。
 * 失去焦点自动收起，符合"随手一唤、随手一关"的用法。
 *
 * 批量删除：Ctrl+点击（或空格）把记录勾进 m_marked，底部出现删除按钮。
 * 普通单击仍然是"直接用"，所以勾选不会挤掉原来的主操作。
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(StorageManager *storage, SettingsStore *settings, QWidget *parent = nullptr);

    /*! 在鼠标附近弹出，并选中第一条。 */
    void popupAtCursor();
    /*! 整表重建（主题、缩略图尺寸变化时用）。 */
    void refreshAll();

signals:
    /*! 用户选中了某条，交给 Application 去写剪切板 + 模拟粘贴。 */
    void itemActivated(qint64 id);
    void settingsRequested();
    void dataDirRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onItemInserted(int index, const ClipboardItem &item);
    void onItemChanged(int index, const ClipboardItem &item);
    void onItemRemoved(qint64 id);
    void onResetPerformed();
    void onRowPressed(QListWidgetItem *row);
    void onRowClicked(QListWidgetItem *row);
    void onRowDoubleClicked(QListWidgetItem *row);
    void onContextMenu(const QPoint &pos);
    void onThumbnailHeightChanged(int height);
    void onThemeChanged(SettingsStore::Theme theme);
    void onListSizeChanged(int value);
    void onBackgroundOpacityChanged(int value);
    void onDeleteMarked();
    void onClearMarks();

private:
    void buildUi();
    void rebuildList();
    QListWidgetItem *createRow(const ClipboardItem &item) const;
    void applyRowContent(QListWidgetItem *row, const ClipboardItem &item) const;
    void updateHeader();
    void clampToScreen(const QPoint &anchor);
    void activateCurrentRow();
    void togglePinCurrentRow();
    void removeCurrentRow();
    int rowIndexById(qint64 id) const;

    void toggleMark(qint64 id);
    void updateMarkBar();

    /*! 无边框窗口的拖动：按下时记住抓取点，移动超过系统阈值才算拖，避免和点击冲突。 */
    void beginDrag(const QPoint &globalPos);
    bool updateDrag(const QPoint &globalPos);
    void endDrag();

    /*! 背景不透明度 < 100 时切到半透明窗口，== 100 时切回不透明。 */
    void applyBackgroundOpacity();

    StorageManager *m_storage = nullptr;
    SettingsStore *m_settings = nullptr;
    ClipboardItemDelegate *m_delegate = nullptr;

    QLabel *m_header = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QListWidget *m_list = nullptr;
    QWidget *m_markBar = nullptr;
    QLabel *m_markLabel = nullptr;
    QPushButton *m_clearMarksButton = nullptr;
    QPushButton *m_deleteMarkedButton = nullptr;
    QLabel *m_hint = nullptr;

    /*! 勾选中、等着被批量删掉的 id。删完即清空。 */
    QVector<qint64> m_marked;

    /*! Ctrl+点击时按下和抬起都会来一次，靠它吃掉多余的那次 click。 */
    bool m_swallowNextClick = false;

    /*! 拖动状态 */
    bool m_dragging = false;
    QPoint m_dragPressGlobal;
    QPoint m_dragAnchor;   // 按下瞬间，鼠标相对窗口左上角的偏移

    /*! delegate 自己画的颜色要跟着主题走，这里记一份当前是不是深色。 */
    bool m_darkTheme = false;
};
