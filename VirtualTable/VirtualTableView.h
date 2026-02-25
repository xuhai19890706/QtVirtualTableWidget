#ifndef VIRTUALTABLEVIEW_H
#define VIRTUALTABLEVIEW_H

#include "VirtualTableModel.h"
#include <QElapsedTimer>
#include <QTableView>
#include <QTimer>

/**
 * @brief 虚拟表格视图类，继承自QTableView
 * 
 * 这个类负责处理滚动事件，计算可见区域，并与VirtualTableModel交互
 * 实现千万级数据的高效滚动和显示
 */
class VirtualTableView : public QTableView {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit VirtualTableView(QWidget* parent = nullptr);
    ~VirtualTableView() override;

    /**
     * @brief 设置虚拟表格模型
     * @param model 虚拟表格模型指针
     */
    void setVirtualModel(VirtualTableModel* model);

    /**
     * @brief 设置缓冲区大小（行数）
     * @param bufferSize 缓冲区大小
     */
    void setBufferSize(int bufferSize);

    /**
     * @brief 设置固定行高
     * @param rowHeight 行高
     */
    void setFixedRowHeight(int rowHeight);

    /**
     * @brief 跳转到指定行
     * @param rowIndex 目标行索引
     * @param scrollToVisible 是否滚动到可见区域
     */
    void jumpToRow(int rowIndex, bool scrollToVisible = true);

    /**
     * @brief 获取当前可见的起始行索引
     * @return 起始行索引
     */
    int visibleStartRow() const;

    /**
     * @brief 获取当前可见的结束行索引
     * @return 结束行索引
     */
    int visibleEndRow() const;

protected:
    // 重写的事件处理方法
    void wheelEvent(QWheelEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    /**
     * @brief 更新可见区域数据
     */
    void updateVisibleData();

    /**
     * @brief 处理滚动速度超时，重置滚动速度
     */
    void handleScrollSpeedTimeout();

private:
    // 私有方法
    /**
     * @brief 计算当前可见区域的行范围
     * @return 可见区域的行范围 [startRow, endRow]
     */
    QPair<int, int> calculateVisibleRows() const;

    /**
     * @brief 更新滚动速度
     * @param deltaY 垂直滚动距离
     */
    void updateScrollSpeed(int deltaY);

    // 私有成员变量
    VirtualTableModel* m_virtualModel; // 虚拟表格模型
    int m_bufferSize; // 缓冲区大小（行数）
    int m_fixedRowHeight; // 固定行高，如果为0则使用默认行高
    int m_visibleStartRow; // 当前可见的起始行索引
    int m_visibleEndRow; // 当前可见的结束行索引
    QTimer m_updateTimer; // 更新可见区域的定时器
    QTimer m_scrollSpeedTimer; // 滚动速度超时定时器
    QElapsedTimer m_scrollTimer; // 滚动时间计时器
    int m_lastScrollPos; // 上一次滚动位置
    double m_currentScrollSpeed; // 当前滚动速度（像素/秒）
    bool m_isInitializing; // 是否正在初始化
};

#endif // VIRTUALTABLEVIEW_H
