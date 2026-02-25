#include "VirtualTableView.h"
#include <QDebug>
#include <QHeaderView>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

VirtualTableView::VirtualTableView(QWidget* parent)
    : QTableView(parent)
    , m_virtualModel(nullptr)
    , m_bufferSize(50)
    , m_fixedRowHeight(0)
    , m_visibleStartRow(0)
    , m_visibleEndRow(0)
    , m_lastScrollPos(0)
    , m_currentScrollSpeed(0.0)
    , m_isInitializing(true)
{
    // 设置表格属性
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setSortingEnabled(false); // 禁用排序，由模型处理

    // 启用交替行颜色
    setAlternatingRowColors(true);

    // 配置更新定时器
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(50); // 20fps更新频率
    connect(&m_updateTimer, &QTimer::timeout, this, &VirtualTableView::updateVisibleData);

    // 配置滚动速度定时器
    m_scrollSpeedTimer.setSingleShot(true);
    m_scrollSpeedTimer.setInterval(200); // 200ms后重置滚动速度
    connect(&m_scrollSpeedTimer, &QTimer::timeout, this, &VirtualTableView::handleScrollSpeedTimeout);

    // 连接滚动条信号
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        // 计算滚动速度
        if (m_scrollTimer.isValid()) {
            qint64 elapsed = m_scrollTimer.elapsed();
            if (elapsed > 0) {
                int delta = value - m_lastScrollPos;
                m_currentScrollSpeed = static_cast<double>(delta) / (elapsed / 1000.0);

                // 更新滚动速度到模型
                if (m_virtualModel) {
                    m_virtualModel->setScrollSpeed(std::abs(m_currentScrollSpeed));
                }

                // 重启滚动速度定时器
                m_scrollSpeedTimer.start();
            }
        }

        m_lastScrollPos = value;
        m_scrollTimer.restart();

        // 延迟更新可见数据
        if (!m_updateTimer.isActive()) {
            m_updateTimer.start();
        }
    });
}

VirtualTableView::~VirtualTableView()
{
    m_updateTimer.stop();
    m_scrollSpeedTimer.stop();
}

void VirtualTableView::setVirtualModel(VirtualTableModel* model)
{
    // 防止重复设置相同的模型
    if (m_virtualModel == model)
        return;

    // 设置新模型
    if (model) {
        m_virtualModel = model;
        setModel(model);
        // 如果已经显示，更新可见数据
        if (isVisible()) {
            // 延迟更新，确保视图已经完全设置好
            QTimer::singleShot(10, this, &VirtualTableView::updateVisibleData);
        }
    }
}

void VirtualTableView::setBufferSize(int bufferSize)
{
    if (bufferSize > 0 && bufferSize != m_bufferSize) {
        m_bufferSize = bufferSize;

        // 如果已经显示，更新可见数据
        if (isVisible() && m_virtualModel) {
            updateVisibleData();
        }
    }
}

void VirtualTableView::setFixedRowHeight(int rowHeight)
{
    if (rowHeight >= 0 && rowHeight != m_fixedRowHeight) {
        m_fixedRowHeight = rowHeight;

        if (m_fixedRowHeight > 0) {
            // 设置所有行的固定高度
            verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
            verticalHeader()->setDefaultSectionSize(m_fixedRowHeight);
        } else {
            // 恢复默认行高
            verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            verticalHeader()->setDefaultSectionSize(verticalHeader()->minimumSectionSize());
        }

        // 如果已经显示，更新可见数据
        if (isVisible() && m_virtualModel) {
            updateVisibleData();
        }
    }
}

void VirtualTableView::jumpToRow(int rowIndex, bool scrollToVisible)
{
    if (!m_virtualModel || rowIndex < 0)
        return;

    // 跳转到指定行
    m_virtualModel->jumpToRow(rowIndex);

    // 如果需要滚动到可见区域
    if (scrollToVisible) {
        scrollTo(m_virtualModel->index(rowIndex, 0), QAbstractItemView::PositionAtCenter);
    }
}

int VirtualTableView::visibleStartRow() const
{
    return m_visibleStartRow;
}

int VirtualTableView::visibleEndRow() const
{
    return m_visibleEndRow;
}

void VirtualTableView::wheelEvent(QWheelEvent* event)
{
    // 处理滚轮事件
    QTableView::wheelEvent(event);

    // 延迟更新可见数据
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

void VirtualTableView::scrollContentsBy(int dx, int dy)
{
    // 处理滚动内容事件
    QTableView::scrollContentsBy(dx, dy);

    // 更新滚动速度
    if (dy != 0) {
        updateScrollSpeed(dy);
    }

    // 延迟更新可见数据
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

void VirtualTableView::resizeEvent(QResizeEvent* event)
{
    // 处理窗口大小变化事件
    QTableView::resizeEvent(event);

    // 更新可见数据
    if (isVisible() && m_virtualModel) {
        updateVisibleData();
    }
}

void VirtualTableView::showEvent(QShowEvent* event)
{
    // 处理窗口显示事件
    QTableView::showEvent(event);

    // 初始化时更新可见数据
    if (m_isInitializing && m_virtualModel) {
        m_isInitializing = false;
        QTimer::singleShot(0, this, &VirtualTableView::updateVisibleData);
    }
}

void VirtualTableView::updateVisibleData()
{
    if (!m_virtualModel)
        return;

    // 计算可见区域的行范围
    QPair<int, int> visibleRows = calculateVisibleRows();
    int startRow = visibleRows.first;
    int endRow = visibleRows.second;

    // 添加缓冲区
    startRow = qMax(0, startRow - m_bufferSize);
    endRow = qMin(m_virtualModel->rowCount() - 1, endRow + m_bufferSize);

    // 如果可见区域没有变化，不需要更新
    if (startRow == m_visibleStartRow && endRow == m_visibleEndRow)
        return;

    // 更新可见区域
    m_visibleStartRow = startRow;
    m_visibleEndRow = endRow;

    // 通知模型更新可见区域数据
    m_virtualModel->setVisibleRange(startRow, endRow);
}

void VirtualTableView::handleScrollSpeedTimeout()
{
    // 重置滚动速度
    m_currentScrollSpeed = 0.0;
    if (m_virtualModel) {
        m_virtualModel->setScrollSpeed(0.0);
    }
}

QPair<int, int> VirtualTableView::calculateVisibleRows() const
{
    if (!m_virtualModel || m_virtualModel->rowCount() == 0)
        return qMakePair(0, 0);

    // 获取视口矩形
    QRect viewportRect = viewport()->rect();

    // 获取第一个可见单元格的索引
    QModelIndex topLeft = indexAt(QPoint(0, 0));
    int startRow = (topLeft.isValid()) ? topLeft.row() : 0;

    // 获取最后一个可见单元格的索引
    QModelIndex bottomRight = indexAt(QPoint(viewportRect.width() - 1, viewportRect.height() - 1));
    int endRow = (bottomRight.isValid()) ? bottomRight.row() : 0;

    // 如果无法通过indexAt获取有效索引，使用滚动条位置计算
    if (startRow == 0 && endRow == 0 && m_virtualModel->rowCount() > 0) {
        int scrollValue = verticalScrollBar()->value();
        int viewportHeight = viewportRect.height();
        int rowHeight = (m_fixedRowHeight > 0) ? m_fixedRowHeight : verticalHeader()->defaultSectionSize();

        if (rowHeight > 0) {
            startRow = scrollValue / rowHeight;
            endRow = startRow + (viewportHeight / rowHeight) + 1;
        }
    }

    // 确保行索引在有效范围内
    startRow = qMax(0, startRow);
    endRow = qMin(m_virtualModel->rowCount() - 1, endRow);

    // 确保endRow至少等于startRow
    if (endRow < startRow)
        endRow = startRow;

    return qMakePair(startRow, endRow);
}

void VirtualTableView::updateScrollSpeed(int deltaY)
{
    if (m_scrollTimer.isValid()) {
        qint64 elapsed = m_scrollTimer.elapsed();
        if (elapsed > 0) {
            m_currentScrollSpeed = static_cast<double>(deltaY) / (elapsed / 1000.0);

            // 更新滚动速度到模型
            if (m_virtualModel) {
                m_virtualModel->setScrollSpeed(std::abs(m_currentScrollSpeed));
            }

            // 重启滚动速度定时器
            m_scrollSpeedTimer.start();
        }
    }

    m_scrollTimer.restart();
}
