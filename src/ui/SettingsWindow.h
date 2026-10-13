#pragma once

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QKeySequence;
class QKeySequenceEdit;
class QLabel;
class QSpinBox;
class StorageManager;
class SettingsStore;

/*!
 * 设置窗口：主题 + 各种偏好。
 *
 * 刻意做成"改完立刻生效"——不设保存按钮，
 * 少一步操作，也少一个"忘了点保存"的坑。
 *
 * 内容现在不多，直接一个表单就够；以后要加页面再换 QStackedWidget。
 */
class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    SettingsWindow(SettingsStore *settings, StorageManager *storage, QWidget *parent = nullptr);

    /*! 每次显示时把界面上的值跟设置同步一次（比如热键被外部改过）。 */
    void reloadFromSettings();

    /*! 热键注册失败时由 Application 调过来，在界面上提示。 */
    void showHotkeyWarning(const QString &message);
    void clearHotkeyWarning();

signals:
    void openDataDirRequested();
    /*! 用户改了热键，Application 负责真的去注册。 */
    void hotkeyChanged(const QString &sequence);

private slots:
    void onThemeComboChanged(int index);
    void onMaxItemsChanged(int value);
    /*! 等录制结束再处理：中途会经过 "Ctrl+" 这种半成品，注册必然失败。 */
    void onHotkeyEditingFinished();
    void onPasteOnClickToggled(bool checked);
    void onThumbnailHeightChanged(int value);
    void onListWidthChanged(int value);
    void onListHeightChanged(int value);
    void onBackgroundOpacityChanged(int value);
    void onClearHistoryClicked();
    void onOpenDataDirClicked();

private:
    void buildUi();
    void updateStorageSummary();

    SettingsStore *m_settings = nullptr;
    StorageManager *m_storage = nullptr;

    QComboBox *m_themeCombo = nullptr;
    QSpinBox *m_maxItemsSpin = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QCheckBox *m_pasteOnClickCheck = nullptr;
    QSpinBox *m_thumbnailSpin = nullptr;
    QSpinBox *m_listWidthSpin = nullptr;
    QSpinBox *m_listHeightSpin = nullptr;
    QSpinBox *m_opacitySpin = nullptr;
    QLabel *m_hotkeyWarning = nullptr;
    QLabel *m_storageSummary = nullptr;
};
