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
    QString preview;      // 摘要。文本行直接显示；图片行现在只用它做 tooltip
    QString hash;         // 去重用
    bool pinned = false;
    qint64 createdAt = 0; // 毫秒时间戳

    /*!
     * 图片的原始像素尺寸。存下来是为了让列表**不加载图片**就能算出行高 ——
     * 否则列 200 条记录时 QListView 为了算滚动范围会把所有图片全解码一遍，卡死。
     * 文本记录恒为 0。
     */
    int imageWidth = 0;
    int imageHeight = 0;

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
