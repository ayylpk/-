#pragma once

#include <QMetaType>
#include <QString>

/*!
 * 一条剪切板记录。
 *
 * 文本记录：content 就是文本本身。
 * 图片记录：content 是相对 dataDir 的图片文件路径（形如 "images/xxx.png"），
 *           绝不往数据库里塞 BLOB。
 */
struct ClipboardItem
{
    enum Type {
        Text = 0,
        Image = 1
    };

    qint64 id = -1;
    Type type = Text;
    QString content;
    QString preview;      // 列表里显示的一行摘要
    QString hash;         // 去重用
    bool pinned = false;
    qint64 createdAt = 0; // 毫秒时间戳

    bool isText() const { return type == Text; }
    bool isImage() const { return type == Image; }
};

Q_DECLARE_METATYPE(ClipboardItem)
Q_DECLARE_METATYPE(ClipboardItem::Type)

/*!
 * 列表行上挂的数据角色。
 * 集中放在这里，免得 MainWindow 和 delegate 各写一套 UserRole + 1 / + 2。
 */
namespace ClipboardRoles {
enum {
    IdRole = Qt::UserRole,      // qint64
    TypeRole,                   // int（ClipboardItem::Type）
    ItemRole,                   // ClipboardItem 本体
    MarkedRole                  // bool —— 勾选中、等着被批量删除
};
} // namespace ClipboardRoles
