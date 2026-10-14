#include "core/StorageManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>
#include <QVariant>

namespace {

const char *kImagesDirName = "images";
const int kPreviewLength = 120;

QString hashOf(const QByteArray &bytes)
{
    return QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

/*! 按内容算图哈希，用来判断"同一张图又被复制了一次"。 */
QString hashOfImage(const QImage &image)
{
    const QImage normalized = image.convertToFormat(QImage::Format_ARGB32);
    const QByteArray raw(reinterpret_cast<const char *>(normalized.constBits()),
                         static_cast<int>(normalized.sizeInBytes()));
    return hashOf(raw);
}

/*!
 * 从预览文案 "图片  1920 × 1080" 里把尺寸抠出来。
 * 只给"补列之前存进去的老图片记录"兜底用 —— 那些行 img_w/img_h 是 0。
 */
void parsePreviewSize(const QString &preview, int *width, int *height)
{
    static const QRegularExpression pattern(QStringLiteral("(\\d+)\\s*[x×*]\\s*(\\d+)"));

    const QRegularExpressionMatch match = pattern.match(preview);
    if (!match.hasMatch())
        return;

    const int w = match.captured(1).toInt();
    const int h = match.captured(2).toInt();
    if (w <= 0 || h <= 0)
        return;

    if (width)
        *width = w;
    if (height)
        *height = h;
}

} // namespace

StorageManager::StorageManager(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("jianqie_storage"))
{
}

StorageManager::~StorageManager()
{
    if (m_database.isOpen())
        m_database.close();

    m_database = QSqlDatabase();
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool StorageManager::open(const QString &dataDir, int maxItems)
{
    m_maxItems = qMax(1, maxItems);
    m_dataDir = QDir::cleanPath(dataDir);

    QDir root(m_dataDir);
    if (!root.exists() && !root.mkpath(QStringLiteral(".")))
        return false;

    m_imagesDir = root.filePath(QString::fromLatin1(kImagesDirName));
    if (!QDir().mkpath(m_imagesDir))
        return false;

    m_databasePath = root.filePath(QStringLiteral("history.db"));

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open())
        return false;

    if (!ensureSchema())
        return false;

    m_opened = true;

    if (!loadAll())
        return false;

    enforceCapacity();
    return true;
}

bool StorageManager::hasColumn(const QString &table, const QString &column) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;

    while (query.next()) {
        if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool StorageManager::ensureSchema()
{
    QSqlQuery query(m_database);
    const QString createTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS items ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  type       INTEGER NOT NULL DEFAULT 0,"
            "  content    TEXT    NOT NULL,"
            "  preview    TEXT    NOT NULL DEFAULT '',"
            "  hash       TEXT    NOT NULL DEFAULT '',"
            "  pinned     INTEGER NOT NULL DEFAULT 0,"
            "  created_at INTEGER NOT NULL,"
            "  img_w      INTEGER NOT NULL DEFAULT 0,"
            "  img_h      INTEGER NOT NULL DEFAULT 0"
            ")");
    if (!query.exec(createTable))
        return false;

    // 老库升级：img_w / img_h 是后加的列，得给已经存在的库补上。
    // 补成 0 不影响功能 —— 读的时候会从 preview 里退回去解析，解析不出来就按默认比例算。
    if (!hasColumn(QStringLiteral("items"), QStringLiteral("img_w"))) {
        if (!query.exec(QStringLiteral(
                "ALTER TABLE items ADD COLUMN img_w INTEGER NOT NULL DEFAULT 0")))
            return false;
    }
    if (!hasColumn(QStringLiteral("items"), QStringLiteral("img_h"))) {
        if (!query.exec(QStringLiteral(
                "ALTER TABLE items ADD COLUMN img_h INTEGER NOT NULL DEFAULT 0")))
            return false;
    }

    // 去重和排序都要用
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_items_created ON items(created_at DESC)")))
        return false;

    return query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_items_hash ON items(hash)"));
}

bool StorageManager::loadAll()
{
    m_items.clear();
    m_thumbnailCache.clear();

    QSqlQuery query(m_database);
    // 置顶的排前面，其余按时间倒序
    if (!query.exec(QStringLiteral(
            "SELECT id, type, content, preview, hash, pinned, created_at, img_w, img_h "
            "FROM items ORDER BY pinned DESC, created_at DESC, id DESC"))) {
        return false;
    }

    while (query.next()) {
        ClipboardItem item;
        item.id = query.value(0).toLongLong();
        item.type = query.value(1).toInt() == ClipboardItem::Image ? ClipboardItem::Image
                                                                   : ClipboardItem::Text;
        item.content = query.value(2).toString();
        item.preview = query.value(3).toString();
        item.hash = query.value(4).toString();
        item.pinned = query.value(5).toInt() != 0;
        item.createdAt = query.value(6).toLongLong();
        item.imageWidth = query.value(7).toInt();
        item.imageHeight = query.value(8).toInt();

        // 老库（补列之前存的图片）没有尺寸，从 preview 里退回去解析一次，
        // 免得这些行按默认比例算出来跟实际差太多。
        if (item.isImage() && (item.imageWidth <= 0 || item.imageHeight <= 0))
            parsePreviewSize(item.preview, &item.imageWidth, &item.imageHeight);

        m_items.append(item);
    }

    return true;
}

int StorageManager::indexOfHash(const QString &hash) const
{
    if (hash.isEmpty())
        return -1;

    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).hash.isEmpty() && m_items.at(i).hash == hash)
            return i;
    }
    return -1;
}

/*! 已存在同样的内容时，只把它提到最前面，返回它的 id；插入新记录时返回 -1。 */
qint64 StorageManager::bumpExisting(const QString &hash)
{
    const int index = indexOfHash(hash);
    if (index < 0)
        return -1;

    ClipboardItem item = m_items.at(index);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QSqlQuery update(m_database);
    update.prepare(QStringLiteral("UPDATE items SET created_at = :created WHERE id = :id"));
    update.bindValue(QStringLiteral(":created"), now);
    update.bindValue(QStringLiteral(":id"), item.id);
    if (!update.exec())
        return -1;

    item.createdAt = now;
    m_items.removeAt(index);
    m_items.insert(insertionIndexFor(item), item);
    emit resetPerformed();
    return item.id;
}

int StorageManager::indexOfId(qint64 id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

int StorageManager::countPinned(const QVector<ClipboardItem> &items)
{
    int n = 0;
    for (const ClipboardItem &item : items) {
        if (item.pinned)
            ++n;
    }
    return n;
}

int StorageManager::pinnedCount() const
{
    return countPinned(m_items);
}

int StorageManager::insertionIndexFor(const ClipboardItem &item) const
{
    if (!item.pinned)
        return countPinned(m_items);

    // 置顶区内部也是按时间倒序，找到第一个比它旧的位置
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).pinned)
            return i;
        if (m_items.at(i).createdAt < item.createdAt)
            return i;
    }
    return m_items.size();
}

QString StorageManager::buildPreview(const QString &text)
{
    QString flat = text;
    flat.replace(QLatin1Char('\r'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\t'), QLatin1Char(' '));
    flat = flat.simplified();

    if (flat.size() > kPreviewLength)
        flat = flat.left(kPreviewLength) + QStringLiteral("…");

    return flat.isEmpty() ? QStringLiteral("(空白文本)") : flat;
}

QString StorageManager::writeImageFile(const QImage &image) const
{
    if (image.isNull())
        return QString();

    const QString fileName = QUuid::createUuid().toString(QUuid::WithoutBraces)
            + QStringLiteral(".png");
    const QString absolutePath = QDir(m_imagesDir).filePath(fileName);

    if (!image.save(absolutePath, "PNG"))
        return QString();

    return QString::fromLatin1(kImagesDirName) + QLatin1Char('/') + fileName;
}

bool StorageManager::removeImageFile(const QString &relativePath)
{
    if (relativePath.isEmpty())
        return false;

    return QFile::remove(QDir(m_dataDir).filePath(relativePath));
}

QString StorageManager::imageAbsolutePath(const ClipboardItem &item) const
{
    if (item.type != ClipboardItem::Image)
        return QString();

    return QDir(m_dataDir).filePath(item.content);
}

qint64 StorageManager::addText(const QString &text)
{
    if (!m_opened || text.isEmpty())
        return -1;

    const QString hash = hashOf(text.toUtf8());

    // 同一条内容反复复制时，只把原来的挪到最前面，不新增记录
    const qint64 bumped = bumpExisting(hash);
    if (bumped >= 0)
        return bumped;

    ClipboardItem item;
    item.type = ClipboardItem::Text;
    item.content = text;
    item.preview = buildPreview(text);
    item.hash = hash;
    item.createdAt = QDateTime::currentMSecsSinceEpoch();

    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
            "INSERT INTO items (type, content, preview, hash, pinned, created_at) "
            "VALUES (:type, :content, :preview, :hash, 0, :created)"));
    insert.bindValue(QStringLiteral(":type"), static_cast<int>(ClipboardItem::Text));
    insert.bindValue(QStringLiteral(":content"), item.content);
    insert.bindValue(QStringLiteral(":preview"), item.preview);
    insert.bindValue(QStringLiteral(":hash"), item.hash);
    insert.bindValue(QStringLiteral(":created"), item.createdAt);
    if (!insert.exec())
        return -1;

    item.id = insert.lastInsertId().toLongLong();

    // 先落盘成功，再更新内存
    const int index = insertionIndexFor(item);
    m_items.insert(index, item);
    emit itemInserted(index, item);

    enforceCapacity();
    return item.id;
}

qint64 StorageManager::addImage(const QImage &image)
{
    if (!m_opened || image.isNull())
        return -1;

    const QString hash = hashOfImage(image);

    // 同一张图又复制了一次 —— 提上来就行，不重复占硬盘
    const qint64 bumped = bumpExisting(hash);
    if (bumped >= 0)
        return bumped;

    const QString relativePath = writeImageFile(image);
    if (relativePath.isEmpty())
        return -1;

    ClipboardItem item;
    item.type = ClipboardItem::Image;
    item.content = relativePath;
    item.preview = QStringLiteral("图片  %1 × %2").arg(image.width()).arg(image.height());
    item.hash = hash;
    item.createdAt = QDateTime::currentMSecsSinceEpoch();
    // 尺寸存下来，列表算行高就不用去解码图片了
    item.imageWidth = image.width();
    item.imageHeight = image.height();

    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
            "INSERT INTO items (type, content, preview, hash, pinned, created_at, img_w, img_h) "
            "VALUES (:type, :content, :preview, :hash, 0, :created, :imgW, :imgH)"));
    insert.bindValue(QStringLiteral(":type"), static_cast<int>(ClipboardItem::Image));
    insert.bindValue(QStringLiteral(":content"), item.content);
    insert.bindValue(QStringLiteral(":preview"), item.preview);
    insert.bindValue(QStringLiteral(":hash"), item.hash);
    insert.bindValue(QStringLiteral(":created"), item.createdAt);
    insert.bindValue(QStringLiteral(":imgW"), item.imageWidth);
    insert.bindValue(QStringLiteral(":imgH"), item.imageHeight);
    if (!insert.exec()) {
        removeImageFile(relativePath);
        return -1;
    }

    item.id = insert.lastInsertId().toLongLong();

    const int index = insertionIndexFor(item);
    m_items.insert(index, item);
    emit itemInserted(index, item);

    enforceCapacity();
    return item.id;
}

bool StorageManager::setPinned(qint64 id, bool pinned)
{
    const int index = indexOfId(id);
    if (index < 0)
        return false;

    if (m_items.at(index).pinned == pinned)
        return true;

    QSqlQuery update(m_database);
    update.prepare(QStringLiteral("UPDATE items SET pinned = :pinned WHERE id = :id"));
    update.bindValue(QStringLiteral(":pinned"), pinned ? 1 : 0);
    update.bindValue(QStringLiteral(":id"), id);
    if (!update.exec())
        return false;

    ClipboardItem item = m_items.takeAt(index);
    item.pinned = pinned;

    const int target = insertionIndexFor(item);
    m_items.insert(target, item);

    if (target != index)
        emit resetPerformed();
    else
        emit itemChanged(target, item);

    return true;
}

void StorageManager::forgetThumbnails(qint64 id)
{
    const QString prefix = QString::number(id) + QLatin1Char('@');

    for (auto it = m_thumbnailCache.begin(); it != m_thumbnailCache.end();) {
        if (it.key().startsWith(prefix))
            it = m_thumbnailCache.erase(it);
        else
            ++it;
    }
}

bool StorageManager::dropItemAt(int index, bool deleteImage)
{
    if (index < 0 || index >= m_items.size())
        return false;

    const ClipboardItem item = m_items.at(index);

    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM items WHERE id = :id"));
    remove.bindValue(QStringLiteral(":id"), item.id);
    if (!remove.exec())
        return false;

    if (deleteImage)
        removeImageFile(item.content);

    forgetThumbnails(item.id);
    m_items.removeAt(index);
    emit itemRemoved(item.id);
    return true;
}

bool StorageManager::removeItem(qint64 id)
{
    const int index = indexOfId(id);
    return dropItemAt(index, true);
}

int StorageManager::removeItems(const QVector<qint64> &ids)
{
    if (!m_opened || ids.isEmpty())
        return 0;

    QStringList idTexts;
    for (qint64 id : ids) {
        if (id > 0)
            idTexts.append(QString::number(id));
    }
    if (idTexts.isEmpty())
        return 0;

    // 先落盘：一条 DELETE 把选中的都删掉。
    // id 都是我们自己从库里读出来的整数，不存在注入问题。
    QSqlQuery query(m_database);
    const QString sql = QStringLiteral("DELETE FROM items WHERE id IN (%1)")
                                .arg(idTexts.join(QLatin1Char(',')));
    if (!query.exec(sql))
        return 0;

    // 落盘成功后再更新内存，顺手把图片文件清掉
    int removed = 0;
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const ClipboardItem &item = m_items.at(i);
        if (!ids.contains(item.id))
            continue;

        if (item.type == ClipboardItem::Image)
            removeImageFile(item.content);

        forgetThumbnails(item.id);
        m_items.removeAt(i);
        ++removed;
    }

    emit resetPerformed();
    return removed;
}

int StorageManager::lastEvictableIndex() const
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!m_items.at(i).pinned)
            return i;
    }
    return -1;
}

void StorageManager::enforceCapacity()
{
    // FIFO 淘汰：从最旧的一条开始丢，但置顶的永远不动
    while (m_items.size() > m_maxItems) {
        const int index = lastEvictableIndex();
        if (index < 0)
            break;
        if (!dropItemAt(index, true))
            break;
    }
}

void StorageManager::setMaxItems(int maxItems)
{
    m_maxItems = qMax(1, maxItems);
    enforceCapacity();
}

void StorageManager::clearAll(bool keepPinned)
{
    if (keepPinned) {
        for (int i = m_items.size() - 1; i >= 0; --i) {
            if (!m_items.at(i).pinned)
                dropItemAt(i, true);
        }
    } else {
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("DELETE FROM items")))
            return;

        for (const ClipboardItem &item : m_items) {
            if (item.type == ClipboardItem::Image)
                removeImageFile(item.content);
        }

        m_items.clear();
        m_thumbnailCache.clear();
    }

    emit resetPerformed();
}

QPixmap StorageManager::thumbnail(const ClipboardItem &item, const QSize &size)
{
    if (!item.isImage() || size.isEmpty() || size.width() <= 0 || size.height() <= 0)
        return QPixmap();

    // 同一张图在不同设置/不同窗口宽度下会有不同尺寸的缩略图，所以 key 得带上尺寸
    const QString key = QStringLiteral("%1@%2x%3")
                                .arg(item.id)
                                .arg(size.width())
                                .arg(size.height());

    const auto cached = m_thumbnailCache.constFind(key);
    if (cached != m_thumbnailCache.constEnd())
        return cached.value();

    QPixmap original(imageAbsolutePath(item));
    if (original.isNull())
        return QPixmap();

    // 不放大：算出的是显示尺寸，比原图大就直接用原图，免得糊
    QPixmap scaled = original;
    if (original.width() > size.width() || original.height() > size.height())
        scaled = original.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 尺寸设置改了会不断产生新 key，加个上限免得越攒越多
    if (m_thumbnailCache.size() > 400)
        m_thumbnailCache.clear();

    m_thumbnailCache.insert(key, scaled);
    return scaled;
}
