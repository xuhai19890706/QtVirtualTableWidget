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
    , m_rowCount(0)
    , m_columnCount(0)
    , m_isValid(false)
    , m_mappedData(nullptr)
    , m_fileSize(0)
    , m_maxCacheSize(maxCacheSize)
{
    // 初始化数据源
    m_isValid = initialize();
}

CsvDataSource::~CsvDataSource()
{
    // 释放内存映射
    if (m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
    // 关闭文件
    if (m_file.isOpen()) {
        m_file.close();
    }
}

int CsvDataSource::rowCount() const
{
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
    if (!m_isValid || startRow < 0 || startRow >= m_rowCount || !m_mappedData) {
        return data;
    }

    // 计算实际需要加载的行数
    int endRow = std::min(startRow + count, m_rowCount);
    int actualCount = endRow - startRow;

    if (actualCount <= 0) {
        return data;
    }

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

    // 内存映射文件
    m_mappedData = m_file.map(0, m_fileSize);
    if (!m_mappedData) {
        m_errorString = QString("无法映射文件: %1").arg(m_file.errorString());
        m_file.close();
        return false;
    }

    // 计算行偏移量并读取表头
    m_rowOffsets.clear();
    m_rowOffsets.push_back(0); // 第一行的偏移量

    // 读取表头
    qint64 headerEnd = 0;
    while (headerEnd < m_fileSize && m_mappedData[headerEnd] != '\n') {
        headerEnd++;
    }

    if (headerEnd >= m_fileSize) {
        m_errorString = "文件格式错误";
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
        m_file.close();
        return false;
    }

    // 提取表头行
    QByteArray headerBytes(reinterpret_cast<const char*>(m_mappedData), headerEnd);
    QString headerLine = QString::fromUtf8(headerBytes);
    
    // 解析表头
    QList<QVariant> headerData = parseLine(headerLine);
    m_headers.clear();
    foreach (const QVariant& var, headerData) {
        m_headers.append(var.toString());
    }
    m_columnCount = m_headers.size();

    // 计算总行数和行偏移量
    if (m_hasHeader) {
        m_rowCount = 0;
    } else {
        m_rowCount = 1;
    }

    qint64 currentOffset = headerEnd + 1; // 跳过表头行
    while (currentOffset < m_fileSize) {
        qint64 lineEnd = currentOffset;
        while (lineEnd < m_fileSize && m_mappedData[lineEnd] != '\n') {
            lineEnd++;
        }
        
        // 跳过空行
        if (lineEnd > currentOffset) {
            m_rowCount++;
            m_rowOffsets.push_back(currentOffset);
        }
        
        currentOffset = lineEnd + 1;
    }

    return m_rowCount > 0 && m_columnCount > 0;
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
    if (rowIndex < 0 || rowIndex >= m_rowCount || !m_mappedData) {
        return false;
    }

    // 内存映射方式不需要文件指针操作，直接返回true
    return true;
}

QString CsvDataSource::getLineFromMappedData(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= m_rowCount || !m_mappedData) {
        return QString();
    }

    // 计算实际行索引（考虑表头）
    int actualRowIndex = m_hasHeader ? rowIndex + 1 : rowIndex;
    if (actualRowIndex >= m_rowOffsets.size()) {
        return QString();
    }

    qint64 startOffset = m_rowOffsets[actualRowIndex];
    if (startOffset >= m_fileSize) {
        return QString();
    }

    // 找到行结束位置
    qint64 endOffset = startOffset;
    while (endOffset < m_fileSize && m_mappedData[endOffset] != '\n') {
        endOffset++;
    }

    // 提取行数据
    QByteArray lineBytes(reinterpret_cast<const char*>(m_mappedData + startOffset), endOffset - startOffset);
    return QString::fromUtf8(lineBytes);
}

void CsvDataSource::cacheRow(int rowIndex, const QList<QVariant>& data)
{
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