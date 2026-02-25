#include "CsvDataSource.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QTextCodec>
#include <algorithm>
#include <cmath>

CsvDataSource::CsvDataSource(const QString& filePath, bool hasHeader, char delimiter, int maxCacheSize)
    : m_filePath(filePath)
    , m_hasHeader(hasHeader)
    , m_delimiter(delimiter)
    , m_rowCount(-1) // -1表示行数未计算
    , m_columnCount(0)
    , m_isValid(false)
    , m_fileSize(0)
    , m_maxCacheSize(maxCacheSize)
{
    // 初始化数据源
    m_isValid = initialize();
}

CsvDataSource::~CsvDataSource()
{
    // 关闭文件
    if (m_file.isOpen()) {
        m_file.close();
    }
}

int CsvDataSource::rowCount() const
{
    // 如果行数未计算，尝试计算
    if (m_rowCount == -1) {
        // 创建一个非const的副本进行计算
        CsvDataSource* nonConstThis = const_cast<CsvDataSource*>(this);
        QMutexLocker locker(&nonConstThis->m_mutex);
        
        // 确保文件已打开
        if (!nonConstThis->m_file.isOpen()) {
            // 尝试重新打开文件
            nonConstThis->m_file.setFileName(nonConstThis->m_filePath);
            if (!nonConstThis->m_file.isOpen()) {
                return 0;
            }
        }
        
        // 计算行数
        nonConstThis->calculateRowCount();
        
        // 如果仍然未计算成功，返回0
        if (nonConstThis->m_rowCount == -1) {
            return 0;
        }
    }
    return m_rowCount;
}

int CsvDataSource::columnCount() const
{
    return m_columnCount;
}

QList<QList<QVariant>> CsvDataSource::loadData(int startRow, int count)
{
    QMutexLocker locker(&m_mutex);

    QList<QList<QVariant>> data;
    if (!m_isValid || startRow < 0) {
        return data;
    }

    // 确保文件已打开
    if (!m_file.isOpen()) {
        // 尝试重新打开文件
        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::ReadOnly)) {
            m_errorString = QString("无法打开文件: %1").arg(m_file.errorString());
            return data;
        }
    }

    // 确保行数已计算
    if (m_rowCount == -1) {
        calculateRowCount();
        // 再次检查行数，确保计算成功
        if (m_rowCount == -1) {
            // 如果计算失败，设置默认值
            m_rowCount = 0;
            return data;
        }
    }

    if (startRow >= m_rowCount) {
        return data;
    }

    // 计算实际需要加载的行数
    int endRow = std::min(startRow + count, m_rowCount);
    int actualCount = endRow - startRow;

    if (actualCount <= 0) {
        return data;
    }

    // 预计算需要的行偏移量
    ensureRowOffsetsCalculated(startRow, endRow);

    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        QList<QVariant> rowData;

        // 先尝试从缓存获取
        if (getFromCache(rowIndex, rowData)) {
            data.append(rowData);
            continue;
        }

        // 从内存映射读取
        QString line = getLineFromMappedData(rowIndex);
        if (line.isNull()) {
            // 读取失败
            break;
        }

        // 解析行数据
        rowData = parseLine(line);

        // 确保列数一致
        if (rowData.size() < m_columnCount) {
            // 如果列数不足，用空值填充
            while (rowData.size() < m_columnCount) {
                rowData.append(QVariant());
            }
        } else if (rowData.size() > m_columnCount) {
            // 如果列数过多，截断
            rowData = rowData.mid(0, m_columnCount);
        }

        // 缓存行数据
        cacheRow(rowIndex, rowData);

        // 添加到结果集
        data.append(rowData);
    }

    return data;
}

QList<QString> CsvDataSource::headerData() const
{
    return m_headers;
}

QString CsvDataSource::filePath() const
{
    return m_filePath;
}

bool CsvDataSource::isValid() const
{
    return m_isValid;
}

QString CsvDataSource::errorString() const
{
    return m_errorString;
}

bool CsvDataSource::initialize()
{
    QMutexLocker locker(&m_mutex);

    // 打开文件
    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_errorString = QString("无法打开文件: %1").arg(m_file.errorString());
        return false;
    }

    // 获取文件大小
    m_fileSize = m_file.size();
    if (m_fileSize == 0) {
        m_errorString = "文件为空";
        m_file.close();
        return false;
    }

    // 只映射文件头部用于读取表头
    const qint64 headerMapSize = qMin(m_fileSize, static_cast<qint64>(1024 * 1024)); // 最多映射1MB
    uchar* headerMappedData = m_file.map(0, headerMapSize);
    if (!headerMappedData) {
        m_errorString = QString("无法映射文件: %1").arg(m_file.errorString());
        m_file.close();
        return false;
    }

    // 读取表头
    qint64 headerEnd = 0;
    while (headerEnd < headerMapSize && headerMappedData[headerEnd] != '\n') {
        headerEnd++;
    }

    if (headerEnd >= headerMapSize) {
        m_errorString = "文件格式错误: 表头过长";
        m_file.unmap(headerMappedData);
        m_file.close();
        return false;
    }

    // 提取表头行
    QByteArray headerBytes(reinterpret_cast<const char*>(headerMappedData), headerEnd);
    QString headerLine = QString::fromUtf8(headerBytes);
    
    // 解析表头
    QList<QVariant> headerData = parseLine(headerLine);
    m_headers.clear();
    foreach (const QVariant& var, headerData) {
        m_headers.append(var.toString());
    }
    m_columnCount = m_headers.size();

    // 释放表头映射
    m_file.unmap(headerMappedData);

    // 初始化行偏移量
    m_rowOffsets.clear();
    if (m_hasHeader) {
        m_rowOffsets.push_back(0); // 表头行的偏移量
        m_rowOffsets.push_back(headerEnd + 1); // 第一行数据的偏移量
    } else {
        m_rowOffsets.push_back(0); // 第一行数据的偏移量
    }

    return m_columnCount > 0;
}

QList<QVariant> CsvDataSource::readRow(int rowIndex)
{
    QList<QVariant> rowData;

    // 先尝试从缓存获取
    if (getFromCache(rowIndex, rowData)) {
        return rowData;
    }

    // 从内存映射读取
    QString line = getLineFromMappedData(rowIndex);

    if (!line.isNull()) {
        rowData = parseLine(line);

        // 确保列数一致
        if (rowData.size() < m_columnCount) {
            // 如果列数不足，用空值填充
            while (rowData.size() < m_columnCount) {
                rowData.append(QVariant());
            }
        } else if (rowData.size() > m_columnCount) {
            rowData = rowData.mid(0, m_columnCount);
        }

        // 缓存行数据
        cacheRow(rowIndex, rowData);
    }

    return rowData;
}

QList<QVariant> CsvDataSource::parseLine(const QString& line)
{
    QList<QVariant> result;
    QString currentField;
    bool inQuotes = false;
    bool escaped = false;

    for (int i = 0; i < line.length(); ++i) {
        QChar c = line.at(i);

        if (escaped) {
            // 处理转义字符
            currentField.append(c);
            escaped = false;
        } else if (c == '\\') {
            // 转义字符
            escaped = true;
        } else if (c == '"') {
            // 引号处理
            inQuotes = !inQuotes;
        } else if (c == m_delimiter && !inQuotes) {
            // 分隔符，且不在引号内
            result.append(currentField.trimmed());
            currentField.clear();
        } else {
            // 普通字符
            currentField.append(c);
        }
    }

    // 添加最后一个字段
    result.append(currentField.trimmed());

    return result;
}

bool CsvDataSource::seekToRow(int rowIndex)
{
    // 检查参数有效性
    if (rowIndex < 0 || !m_file.isOpen()) {
        return false;
    }

    // 确保行数已计算
    if (m_rowCount == -1) {
        calculateRowCount();
    }

    if (rowIndex >= m_rowCount) {
        return false;
    }

    // 内存映射方式不需要文件指针操作，直接返回true
    return true;
}

QString CsvDataSource::getLineFromMappedData(int rowIndex)
{
    if (rowIndex < 0 || !m_file.isOpen()) {
        return QString();
    }

    // 计算实际行索引（考虑表头）
    int actualRowIndex = m_hasHeader ? rowIndex + 1 : rowIndex;
    
    // 确保偏移量已计算
    if (!ensureRowOffsetCalculated(actualRowIndex)) {
        return QString();
    }

    if (actualRowIndex >= m_rowOffsets.size()) {
        return QString();
    }

    qint64 startOffset = m_rowOffsets[actualRowIndex];
    if (startOffset >= m_fileSize) {
        return QString();
    }

    // 分段映射：只映射当前行所在的块
    const qint64 blockSize = 1024 * 1024; // 1MB块
    qint64 blockStart = startOffset - (startOffset % blockSize);
    qint64 mapSize = qMin(blockSize, m_fileSize - blockStart);
    
    uchar* blockMappedData = m_file.map(blockStart, mapSize);
    if (!blockMappedData) {
        // 尝试使用更小的块大小
        const qint64 smallBlockSize = 64 * 1024; // 64KB块
        blockStart = startOffset - (startOffset % smallBlockSize);
        mapSize = qMin(smallBlockSize, m_fileSize - blockStart);
        blockMappedData = m_file.map(blockStart, mapSize);
        if (!blockMappedData) {
            return QString();
        }
    }

    // 找到行结束位置
    qint64 relativeStart = startOffset - blockStart;
    qint64 endOffset = startOffset;
    while (endOffset < blockStart + mapSize && blockMappedData[endOffset - blockStart] != '\n') {
        endOffset++;
    }

    // 提取行数据
    QByteArray lineBytes(reinterpret_cast<const char*>(blockMappedData + relativeStart), endOffset - startOffset);
    QString line = QString::fromUtf8(lineBytes);

    // 释放块映射
    m_file.unmap(blockMappedData);

    return line;
}

void CsvDataSource::cacheRow(int rowIndex, const QList<QVariant>& data)
{
    // 动态调整缓存大小：根据可用内存和文件大小
    const int minCacheSize = 1000;
    const int maxCacheSize = 50000;
    
    // 根据系统内存和文件大小调整缓存大小
    // Qt 5.13 兼容版本
    // QSysInfo sysInfo;
    // qulonglong totalMemory = sysInfo.memorySize(); // 使用memorySize()代替totalVirtualMemory()
    // qulonglong optimalCacheSize = qMin(static_cast<qulonglong>(maxCacheSize),
    //                                   (totalMemory / 1024 / 1024) / 10); // 最多使用1/10的内存
    // m_maxCacheSize = qMax(minCacheSize, static_cast<int>(optimalCacheSize));
    m_maxCacheSize = maxCacheSize;

    // 如果缓存已满，清理最旧的缓存项
    if (m_rowCache.size() >= m_maxCacheSize) {
        cleanupCache();
    }

    // 添加到缓存
    m_rowCache[rowIndex] = data;

    // 更新缓存顺序
    m_cacheOrder.removeAll(rowIndex);
    m_cacheOrder.append(rowIndex);
}

bool CsvDataSource::getFromCache(int rowIndex, QList<QVariant>& data) const
{
    auto it = m_rowCache.find(rowIndex);
    if (it != m_rowCache.end()) {
        data = it.value();

        // 更新缓存顺序（需要const_cast，因为这个方法是const的）
        const_cast<CsvDataSource*>(this)->m_cacheOrder.removeAll(rowIndex);
        const_cast<CsvDataSource*>(this)->m_cacheOrder.append(rowIndex);

        return true;
    }

    return false;
}

void CsvDataSource::cleanupCache()
{
    if (m_cacheOrder.isEmpty()) {
        return;
    }

    // 移除最旧的缓存项（LRU策略）
    int oldestRow = m_cacheOrder.takeFirst();
    m_rowCache.remove(oldestRow);
}

bool CsvDataSource::ensureRowOffsetCalculated(int rowIndex)
{
    if (rowIndex < 0) {
        return false;
    }

    // 如果已经计算了足够的偏移量，直接返回
    if (rowIndex < m_rowOffsets.size()) {
        return true;
    }

    // 从上次计算的位置继续计算
    qint64 currentOffset = m_rowOffsets.empty() ? 0 : m_rowOffsets.back();
    
    // 确保至少有一个偏移量
    if (m_rowOffsets.empty()) {
        m_rowOffsets.push_back(0);
    }
    
    while (currentOffset < m_fileSize && m_rowOffsets.size() <= rowIndex) {
        qint64 lineEnd = currentOffset;
        
        // 分段映射计算行结束位置
        const qint64 blockSize = 1024 * 1024; // 1MB块
        qint64 blockStart = currentOffset - (currentOffset % blockSize);
        qint64 mapSize = qMin(blockSize, m_fileSize - blockStart);
        
        uchar* blockMappedData = m_file.map(blockStart, mapSize);
        if (!blockMappedData) {
            // 尝试使用更小的块大小
            const qint64 smallBlockSize = 64 * 1024; // 64KB块
            blockStart = currentOffset - (currentOffset % smallBlockSize);
            mapSize = qMin(smallBlockSize, m_fileSize - blockStart);
            blockMappedData = m_file.map(blockStart, mapSize);
            if (!blockMappedData) {
                // 如果仍然失败，跳过当前块
                currentOffset += smallBlockSize;
                continue;
            }
        }
        
        while (lineEnd < blockStart + mapSize && blockMappedData[lineEnd - blockStart] != '\n') {
            lineEnd++;
        }
        
        m_file.unmap(blockMappedData);
        
        // 跳过空行
        if (lineEnd > currentOffset) {
            m_rowOffsets.push_back(currentOffset);
        }
        
        currentOffset = lineEnd + 1;
    }
    
    return rowIndex < m_rowOffsets.size();
}

void CsvDataSource::ensureRowOffsetsCalculated(int startRow, int endRow)
{
    // 计算实际需要的行索引（考虑表头）
    int actualStartRow = m_hasHeader ? startRow + 1 : startRow;
    int actualEndRow = m_hasHeader ? endRow + 1 : endRow;
    
    for (int i = actualStartRow; i <= actualEndRow; ++i) {
        ensureRowOffsetCalculated(i);
    }
}

void CsvDataSource::calculateRowCount()
{
    if (m_rowCount != -1 || !m_file.isOpen()) {
        return;
    }

    // 从表头结束位置开始计算
    qint64 currentOffset = m_rowOffsets.size() > 1 ? m_rowOffsets[1] : 0;
    int count = 0;
    bool calculationSuccessful = false;
    
    // 确保currentOffset是有效的
    if (currentOffset >= m_fileSize) {
        m_rowCount = 0;
        return;
    }
    
    while (currentOffset < m_fileSize) {
        qint64 lineEnd = currentOffset;
        
        // 分段映射计算行结束位置
        const qint64 blockSize = 1024 * 1024; // 1MB块
        qint64 blockStart = currentOffset - (currentOffset % blockSize);
        qint64 mapSize = qMin(blockSize, m_fileSize - blockStart);
        
        uchar* blockMappedData = m_file.map(blockStart, mapSize);
        if (!blockMappedData) {
            // 尝试使用更小的块大小
            const qint64 smallBlockSize = 64 * 1024; // 64KB块
            blockStart = currentOffset - (currentOffset % smallBlockSize);
            mapSize = qMin(smallBlockSize, m_fileSize - blockStart);
            blockMappedData = m_file.map(blockStart, mapSize);
            if (!blockMappedData) {
                // 如果仍然失败，跳过当前块
                currentOffset += smallBlockSize;
                continue;
            }
        }
        
        while (lineEnd < blockStart + mapSize && blockMappedData[lineEnd - blockStart] != '\n') {
            lineEnd++;
        }
        
        m_file.unmap(blockMappedData);
        calculationSuccessful = true;
        
        // 跳过空行
        if (lineEnd > currentOffset) {
            count++;
        }
        
        currentOffset = lineEnd + 1;
    }
    
    // 只有在计算成功时才设置行数
    if (calculationSuccessful || count > 0) {
        m_rowCount = count;
    } else {
        // 如果计算失败，设置为0
        m_rowCount = 0;
    }
}