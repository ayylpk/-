#include "ui/SettingsWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "core/SettingsStore.h"
#include "core/StorageManager.h"

namespace {

const int kMinThumbnail = 40;
const int kMaxThumbnail = 400;
const int kMinMaxItems = 20;
const int kMaxMaxItems = 1000;
const int kMinListWidth = 420;
const int kMaxListWidth = 1600;
const int kMinListHeight = 260;
const int kMaxListHeight = 1200;

} // namespace

SettingsWindow::SettingsWindow(SettingsStore *settings, StorageManager *storage, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_storage(storage)
{
    setObjectName(QStringLiteral("SettingsWindow"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(QStringLiteral("设置 - 剪切板历史"));
    // 横屏：两列并排，整体做得宽而扁，不用一直往下滚
    setMinimumSize(720, 400);
    resize(820, 470);

    buildUi();
    reloadFromSettings();

    if (m_storage) {
        connect(m_storage, &StorageManager::itemInserted, this, [this](int, const ClipboardItem &) {
            updateStorageSummary();
        });
        connect(m_storage, &StorageManager::itemRemoved, this, [this](qint64) {
            updateStorageSummary();
        });
        connect(m_storage, &StorageManager::resetPerformed, this, [this]() {
            updateStorageSummary();
        });
    }
}

void SettingsWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 16);
    root->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("设置"), this);
    title->setObjectName(QStringLiteral("SettingsTitle"));
    root->addWidget(title);

    /* ---------------- 外观 ---------------- */
    auto *appearanceBox = new QGroupBox(QStringLiteral("外观"), this);
    auto *appearanceForm = new QFormLayout(appearanceBox);
    appearanceForm->setLabelAlignment(Qt::AlignLeft);
    appearanceForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    appearanceForm->setHorizontalSpacing(14);
    appearanceForm->setVerticalSpacing(10);

    m_themeCombo = new QComboBox(appearanceBox);
    m_themeCombo->addItem(QStringLiteral("浅色"), static_cast<int>(SettingsStore::Light));
    m_themeCombo->addItem(QStringLiteral("深色"), static_cast<int>(SettingsStore::Dark));
    connect(m_themeCombo, &QComboBox::currentIndexChanged,
            this, &SettingsWindow::onThemeComboChanged);

    m_thumbnailSpin = new QSpinBox(appearanceBox);
    m_thumbnailSpin->setRange(kMinThumbnail, kMaxThumbnail);
    m_thumbnailSpin->setSingleStep(10);
    m_thumbnailSpin->setSuffix(QStringLiteral(" px"));
    m_thumbnailSpin->setToolTip(
            QStringLiteral("图片行的预览高度上限。行高按图片自身尺寸的 1/8 算，再夹在这个上限以内。"));
    connect(m_thumbnailSpin, &QSpinBox::valueChanged,
            this, &SettingsWindow::onThumbnailHeightChanged);

    m_listWidthSpin = new QSpinBox(appearanceBox);
    m_listWidthSpin->setRange(kMinListWidth, kMaxListWidth);
    m_listWidthSpin->setSingleStep(20);
    m_listWidthSpin->setSuffix(QStringLiteral(" px"));
    connect(m_listWidthSpin, &QSpinBox::valueChanged,
            this, &SettingsWindow::onListWidthChanged);

    m_listHeightSpin = new QSpinBox(appearanceBox);
    m_listHeightSpin->setRange(kMinListHeight, kMaxListHeight);
    m_listHeightSpin->setSingleStep(20);
    m_listHeightSpin->setSuffix(QStringLiteral(" px"));
    connect(m_listHeightSpin, &QSpinBox::valueChanged,
            this, &SettingsWindow::onListHeightChanged);

    m_opacitySpin = new QSpinBox(appearanceBox);
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSingleStep(5);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(QStringLiteral("100 = 完全不透明；低于 100 则窗口会透出后面的内容"));
    connect(m_opacitySpin, &QSpinBox::valueChanged,
            this, &SettingsWindow::onBackgroundOpacityChanged);

    // 横屏之后每一列都不宽，输入控件别拉满整列，不然一列里控件宽度参差不齐
    const int inputWidth = 150;
    m_themeCombo->setMaximumWidth(inputWidth);
    m_thumbnailSpin->setMaximumWidth(inputWidth);
    m_listWidthSpin->setMaximumWidth(inputWidth);
    m_listHeightSpin->setMaximumWidth(inputWidth);
    m_opacitySpin->setMaximumWidth(inputWidth);

    appearanceForm->addRow(QStringLiteral("主题"), m_themeCombo);
    appearanceForm->addRow(QStringLiteral("图片预览最大高度"), m_thumbnailSpin);
    appearanceForm->addRow(QStringLiteral("列表窗口宽度"), m_listWidthSpin);
    appearanceForm->addRow(QStringLiteral("列表窗口高度"), m_listHeightSpin);
    appearanceForm->addRow(QStringLiteral("背景不透明度"), m_opacitySpin);

    auto *appearanceHint = new QLabel(
            QStringLiteral("主题是一份全局 QSS，改完所有窗口立刻跟着变。\n"
                           "列表窗口尺寸改完，下次热键唤出时生效。\n"
                           "背景不透明度调到 100 以下才会变成半透明窗口（拖到 0 就只剩文字）。"),
            appearanceBox);
    appearanceHint->setObjectName(QStringLiteral("SectionHint"));
    appearanceHint->setWordWrap(true);
    appearanceForm->addRow(QString(), appearanceHint);

    /* ---------------- 行为 ---------------- */
    auto *behaviorBox = new QGroupBox(QStringLiteral("行为"), this);
    auto *behaviorForm = new QFormLayout(behaviorBox);
    behaviorForm->setLabelAlignment(Qt::AlignLeft);
    behaviorForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    behaviorForm->setHorizontalSpacing(14);
    behaviorForm->setVerticalSpacing(10);

    m_hotkeyEdit = new QKeySequenceEdit(behaviorBox);
    m_hotkeyEdit->setMaximumSequenceLength(1);
    m_hotkeyEdit->setMaximumWidth(240);
    connect(m_hotkeyEdit, &QKeySequenceEdit::editingFinished,
            this, &SettingsWindow::onHotkeyEditingFinished);

    m_pasteOnClickCheck = new QCheckBox(QStringLiteral("单击列表项立刻粘贴"), behaviorBox);
    connect(m_pasteOnClickCheck, &QCheckBox::toggled,
            this, &SettingsWindow::onPasteOnClickToggled);

    behaviorForm->addRow(QStringLiteral("唤出热键"), m_hotkeyEdit);
    behaviorForm->addRow(QString(), m_pasteOnClickCheck);

    m_hotkeyWarning = new QLabel(behaviorBox);
    m_hotkeyWarning->setObjectName(QStringLiteral("SectionHint"));
    m_hotkeyWarning->setWordWrap(true);
    m_hotkeyWarning->setVisible(false);
    behaviorForm->addRow(QString(), m_hotkeyWarning);

    auto *behaviorHint = new QLabel(
            QStringLiteral("热键被别的软件占用时注册会失败，换一个组合即可。"),
            behaviorBox);
    behaviorHint->setObjectName(QStringLiteral("SectionHint"));
    behaviorHint->setWordWrap(true);
    behaviorForm->addRow(QString(), behaviorHint);

    /* ---------------- 存储 ---------------- */
    auto *storageBox = new QGroupBox(QStringLiteral("存储"), this);
    auto *storageLayout = new QVBoxLayout(storageBox);
    storageLayout->setSpacing(10);

    auto *storageForm = new QFormLayout;
    storageForm->setLabelAlignment(Qt::AlignLeft);
    storageForm->setHorizontalSpacing(14);
    storageForm->setVerticalSpacing(10);

    m_maxItemsSpin = new QSpinBox(storageBox);
    m_maxItemsSpin->setRange(kMinMaxItems, kMaxMaxItems);
    m_maxItemsSpin->setSingleStep(50);
    m_maxItemsSpin->setSuffix(QStringLiteral(" 条"));
    m_maxItemsSpin->setMaximumWidth(150);
    connect(m_maxItemsSpin, &QSpinBox::valueChanged,
            this, &SettingsWindow::onMaxItemsChanged);

    storageForm->addRow(QStringLiteral("最多保留"), m_maxItemsSpin);
    storageLayout->addLayout(storageForm);

    m_storageSummary = new QLabel(storageBox);
    m_storageSummary->setObjectName(QStringLiteral("SectionHint"));
    m_storageSummary->setWordWrap(true);
    // 数据目录路径挺长的，允许选中复制
    m_storageSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    storageLayout->addWidget(m_storageSummary);

    auto *storageHint = new QLabel(
            QStringLiteral("超出上限时按时间淘汰最旧的，置顶的记录不会被删。\n"
                           "文本存在 SQLite 里，图片存在 images/ 目录下，数据库只记路径。"),
            storageBox);
    storageHint->setObjectName(QStringLiteral("SectionHint"));
    storageHint->setWordWrap(true);
    storageLayout->addWidget(storageHint);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);

    auto *openDirButton = new QPushButton(QStringLiteral("打开数据目录"), storageBox);
    connect(openDirButton, &QPushButton::clicked, this, &SettingsWindow::onOpenDataDirClicked);

    auto *clearButton = new QPushButton(QStringLiteral("清空历史（保留置顶）"), storageBox);
    clearButton->setObjectName(QStringLiteral("DangerButton"));
    connect(clearButton, &QPushButton::clicked, this, &SettingsWindow::onClearHistoryClicked);

    buttonRow->addWidget(openDirButton);
    buttonRow->addWidget(clearButton);
    buttonRow->addStretch(1);
    storageLayout->addLayout(buttonRow);

    /* ---------------- 组装成横屏两列 ----------------
     * 左列放「外观」（5 行，最高）；
     * 右列把「行为」和「存储」上下叠起来，两边高度正好差不多，
     * 不会出现一边空一大片。
     */
    auto *rightColumn = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);
    rightLayout->addWidget(behaviorBox, 0);
    rightLayout->addWidget(storageBox, 0);
    rightLayout->addStretch(1);

    // 给两列一个相同的最小宽度，免得某一列被自己的内容撑窄了
    appearanceBox->setMinimumWidth(330);
    rightColumn->setMinimumWidth(330);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(12);
    columns->addWidget(appearanceBox, 1);
    columns->addWidget(rightColumn, 1);

    root->addLayout(columns, 0);
    root->addStretch(1);
}

void SettingsWindow::reloadFromSettings()
{
    if (!m_settings)
        return;

    const auto theme = m_settings->theme();
    const int themeIndex = m_themeCombo->findData(static_cast<int>(theme));
    if (themeIndex >= 0) {
        // 这里会触发 currentIndexChanged，但值相同，SettingsStore 会自己忽略
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->setCurrentIndex(themeIndex);
    }

    {
        QSignalBlocker blocker(m_maxItemsSpin);
        m_maxItemsSpin->setValue(m_settings->maxItems());
    }
    {
        QSignalBlocker blocker(m_thumbnailSpin);
        m_thumbnailSpin->setValue(m_settings->thumbnailHeight());
    }
    {
        QSignalBlocker blocker(m_listWidthSpin);
        m_listWidthSpin->setValue(m_settings->listWidth());
    }
    {
        QSignalBlocker blocker(m_listHeightSpin);
        m_listHeightSpin->setValue(m_settings->listHeight());
    }
    {
        QSignalBlocker blocker(m_opacitySpin);
        m_opacitySpin->setValue(m_settings->backgroundOpacity());
    }
    {
        QSignalBlocker blocker(m_pasteOnClickCheck);
        m_pasteOnClickCheck->setChecked(m_settings->pasteOnClick());
    }
    {
        QSignalBlocker blocker(m_hotkeyEdit);
        m_hotkeyEdit->setKeySequence(
                QKeySequence(m_settings->hotkey(), QKeySequence::PortableText));
    }

    updateStorageSummary();
}

void SettingsWindow::updateStorageSummary()
{
    if (!m_storage || !m_storageSummary)
        return;

    m_storageSummary->setText(QStringLiteral("当前 %1 条（置顶 %2 条）\n数据目录：%3")
                                      .arg(m_storage->count())
                                      .arg(m_storage->pinnedCount())
                                      .arg(QDir::toNativeSeparators(m_storage->dataDir())));
}

void SettingsWindow::showHotkeyWarning(const QString &message)
{
    if (!m_hotkeyWarning)
        return;

    m_hotkeyWarning->setText(message);
    m_hotkeyWarning->setVisible(!message.isEmpty());
}

void SettingsWindow::clearHotkeyWarning()
{
    showHotkeyWarning(QString());
}

void SettingsWindow::onThemeComboChanged(int index)
{
    if (!m_settings || index < 0)
        return;

    const int value = m_themeCombo->itemData(index).toInt();
    m_settings->setTheme(value == static_cast<int>(SettingsStore::Dark) ? SettingsStore::Dark
                                                                        : SettingsStore::Light);
}

void SettingsWindow::onMaxItemsChanged(int value)
{
    if (m_settings)
        m_settings->setMaxItems(value);
}

void SettingsWindow::onThumbnailHeightChanged(int value)
{
    if (m_settings)
        m_settings->setThumbnailHeight(value);
}

void SettingsWindow::onListWidthChanged(int value)
{
    if (m_settings)
        m_settings->setListWidth(value);
}

void SettingsWindow::onListHeightChanged(int value)
{
    if (m_settings)
        m_settings->setListHeight(value);
}

void SettingsWindow::onBackgroundOpacityChanged(int value)
{
    if (m_settings)
        m_settings->setBackgroundOpacity(value);
}

void SettingsWindow::onPasteOnClickToggled(bool checked)
{
    if (m_settings)
        m_settings->setPasteOnClick(checked);
}

void SettingsWindow::onHotkeyEditingFinished()
{
    if (!m_settings || !m_hotkeyEdit)
        return;

    // 录制中途会经过只有修饰键的半成品状态，这里直接不要
    const QKeySequence sequence = m_hotkeyEdit->keySequence();
    if (sequence.isEmpty() || sequence.count() == 0)
        return;

    const int key = sequence[0].key();
    if (key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Shift
        || key == Qt::Key_Meta) {
        return;
    }

    if (sequence[0].keyboardModifiers() == Qt::NoModifier) {
        // 不带修饰键的裸按键做全局热键会抢掉正常输入，不接受
        showHotkeyWarning(QStringLiteral("全局热键至少要带一个修饰键（Ctrl / Alt / Shift / Win）。"));
        return;
    }

    const QString text = sequence.toString(QKeySequence::PortableText).trimmed();
    if (text.isEmpty() || text == m_settings->hotkey())
        return;

    emit hotkeyChanged(text);
}

void SettingsWindow::onOpenDataDirClicked()
{
    emit openDataDirRequested();
}

void SettingsWindow::onClearHistoryClicked()
{
    if (!m_storage)
        return;

    const int removable = m_storage->count() - m_storage->pinnedCount();
    if (removable <= 0) {
        QMessageBox::information(this, QStringLiteral("清空历史"),
                                 QStringLiteral("没有可清理的记录。"));
        return;
    }

    const auto answer = QMessageBox::question(
            this,
            QStringLiteral("清空历史"),
            QStringLiteral("会删掉 %1 条记录（置顶的会保留），图片文件也会一起删除。要继续吗？")
                    .arg(removable),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (answer == QMessageBox::Yes)
        m_storage->clearAll(true);
}
