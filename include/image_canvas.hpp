#pragma once
#include <QWidget>
#include <QImage>
#include <QPoint>

class ImageCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setTitle(const QString& title);
    void resetView();
    void applyAutoFit();

    void setZoomAndPan(double scale, const QPointF& offset);

signals:
    void viewChanged(double scale, const QPointF& offset);
    void viewReset();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage m_image;
    QString m_title;
    double m_scale = 1.0;
    QPointF m_offset = QPointF(0, 0);
    bool m_isDragging = false;
    QPoint m_lastMousePos;
    bool m_autoFit = true;

    void updateFitScale();
};
