#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QSize>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "core/ClipboardItem.h"

/*!
 * 持久化 + 内存缓存。
 *
 * 原则：
 *  - 硬盘是唯一真相，内存只是缓存。
 *  - 文本进 SQLite；图片存成文件，库里只记相对路径。
 *  - 写入时先落盘再更新内存；删除时连带把图片文件删掉。
 *  - 启动时把所有元数据读进内存，之后运行时不再读库。
 *
 * m_items 的排序始终是：置顶的在前，同组内按时间倒序，也就是 index 0 永远是最该显示的那条。
 */
class StorageManager : public QObject
{
    Q_OBJECT

public:
    explicit StorageManager(QObject *parent = nullptr);
    ~StorageManager() override;

    bool open(const QString &dataDir, int maxItems);
    void setMaxItems(int maxItems);

    const QVector<ClipboardItem> &items() const { return m_items; }
    int count() const { return m_items.size(); }
    int pinnedCount() const;
    bool isEmpty() const { return m_items.isEmpty(); }

    /*! 返回新记录的 id，被忽略时返回 -1。 */
    qint64 addText(const QString &text);
    qint64 addImage(const QImage &image);

    bool setPinned(qint64 id, bool pinned);
    bool removeItem(qint64 id);
    /*! 批量删除：一次事务、一次刷新，返回真正删掉的条数。 */
    int removeItems(const QVector<qint64> &ids);
    void clearAll(bool keepPinned);

    /*!
     * 缩略图，按 (id + 目标尺寸) 缓存。
     * size 是调用方按宽高比算好的目标尺寸，第一次调用会去读图片文件。
     */
    QPixmap thumbnail(const ClipboardItem &item, const QSize &size);

    QString imageAbsolutePath(const ClipboardItem &item) const;

    QString dataDir() const { return m_dataDir; }
    QString imagesDir() const { return m_imagesDir; }
    QString databasePath() const { return m_databasePath; }

signals:
    /*! 新记录插入到 index，收到后直接往列表里插一行即可。 */
    void itemInserted(int index, const ClipboardItem &item);
    /*! 某条记录内容变了（目前只有置顶状态会触发）。 */
    void itemChanged(int index, const ClipboardItem &item);
    void itemRemoved(qint64 id);
    /*! 需要整个重建列表（排序变了、批量清理、去重挪位）。 */
    void resetPerformed();

private:
    bool ensureSchema();
    bool loadAll();
    void enforceCapacity();
    /*! 老库升级用：表里有没有这一列。 */
    bool hasColumn(const QString &table, const QString &column) const;

    /*! 把某条记录的所有尺寸缩略图缓存都清掉（记录被删时用）。 */
    void forgetThumbnails(qint64 id);

    /*! 返回最后一个可以淘汰的下标，全被置顶时返回 -1。 */
    int lastEvictableIndex() const;
    int insertionIndexFor(const ClipboardItem &item) const;
    int indexOfId(qint64 id) const;
    int indexOfHash(const QString &hash) const;

    /*! 已有同样内容时把它提到最前面并返回 id；没有则返回 -1。 */
    qint64 bumpExisting(const QString &hash);

    /*! 把图片写到 images/ 下，成功返回相对路径，失败返回空串。 */
    QString writeImageFile(const QImage &image) const;
    bool removeImageFile(const QString &relativePath);
    static QString buildPreview(const QString &text);
    static int countPinned(const QVector<ClipboardItem> &items);

    bool dropItemAt(int index, bool deleteImage);

    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_dataDir;
    QString m_imagesDir;
    QString m_databasePath;

    QVector<ClipboardItem> m_items;
    /*! key 是 "id@宽x高"，因为同一张图在不同设置下会有不同尺寸的缩略图。 */
    QHash<QString, QPixmap> m_thumbnailCache;
    int m_maxItems = 200;
    bool m_opened = false;
};
