#include "image_canvas.hpp"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>

ImageCanvas::ImageCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setStyleSheet("background-color: #1a1a1a;");
}

void ImageCanvas::setImage(const QImage& img) {
    m_image = img;
    if (m_autoFit) {
        updateFitScale();
    }
    update();
}

void ImageCanvas::setTitle(const QString& title) {
    m_title = title;
    update();
}

void ImageCanvas::resetView() {
    m_autoFit = true;
    updateFitScale();
    update();
    emit viewChanged(m_scale, m_offset);
}

void ImageCanvas::setZoomAndPan(double scale, const QPointF& offset) {
    m_scale = scale;
    m_offset = offset;
    m_autoFit = false;
    update();
}

void ImageCanvas::updateFitScale() {
    if (m_image.isNull() || width() <= 0 || height() <= 0) return;

    double sx = static_cast<double>(width() - 20) / m_image.width();
    double sy = static_cast<double>(height() - 40) / m_image.height();
    m_scale = std::min(sx, sy);
    if (m_scale <= 0.001) m_scale = 1.0;

    double drawW = m_image.width() * m_scale;
    double drawH = m_image.height() * m_scale;
    m_offset = QPointF((width() - drawW) / 2.0, (height() - drawH + 20) / 2.0);
}

void ImageCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_autoFit) {
        updateFitScale();
    }
}

void ImageCanvas::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(26, 26, 30));

    if (!m_image.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 2.0);
        QRectF targetRect(m_offset.x(), m_offset.y(), m_image.width() * m_scale, m_image.height() * m_scale);
        p.drawImage(targetRect, m_image);

        // Border around image
        p.setPen(QColor(80, 80, 90));
        p.drawRect(targetRect);
    } else {
        p.setPen(QColor(150, 150, 150));
        p.drawText(rect(), Qt::AlignCenter, "No image loaded");
    }

    // Title badge at top-left
    if (!m_title.isEmpty()) {
        p.setFont(QFont("sans-serif", 10, QFont::Bold));
        QFontMetrics fm = p.fontMetrics();
        int textW = fm.horizontalAdvance(m_title);
        int textH = fm.height();
        QRect badgeRect(10, 10, textW + 16, textH + 8);

        p.setBrush(QColor(0, 0, 0, 180));
        p.setPen(QColor(60, 60, 70));
        p.drawRoundedRect(badgeRect, 4, 4);

        p.setPen(QColor(240, 240, 240));
        p.drawText(badgeRect, Qt::AlignCenter, m_title);
    }

    // Zoom badge at bottom-right
    if (!m_image.isNull()) {
        QString zoomStr = QString::number(static_cast<int>(m_scale * 100)) + "%";
        p.setFont(QFont("sans-serif", 9));
        QFontMetrics fm = p.fontMetrics();
        int textW = fm.horizontalAdvance(zoomStr);
        QRect badgeRect(width() - textW - 20, height() - 25, textW + 12, 18);

        p.setBrush(QColor(0, 0, 0, 150));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(badgeRect, 3, 3);

        p.setPen(QColor(180, 180, 180));
        p.drawText(badgeRect, Qt::AlignCenter, zoomStr);
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event) {
    if (m_image.isNull()) return;

    m_autoFit = false;
    double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);

    QPointF mousePos = event->position();
    // Anchor zoom around mouse position
    QPointF imgPoint = (mousePos - m_offset) / m_scale;

    m_scale = std::clamp(m_scale * factor, 0.05, 30.0);
    m_offset = mousePos - imgPoint * m_scale;

    update();
    emit viewChanged(m_scale, m_offset);
}

void ImageCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        m_autoFit = false;
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_offset += QPointF(delta.x(), delta.y());
        update();
        emit viewChanged(m_scale, m_offset);
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}
