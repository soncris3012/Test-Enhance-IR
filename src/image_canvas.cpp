#include "image_canvas.hpp"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>

ImageCanvas::ImageCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setStyleSheet("background-color: #1c1c1c;");
}

void ImageCanvas::setImage(const QImage& img) {
    const bool sizeChanged = (m_image.size() != img.size()) || img.isNull();
    m_image = img;
    if (sizeChanged) {
        m_autoFit = true;
    }
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
    applyAutoFit();
}

void ImageCanvas::applyAutoFit() {
    m_autoFit = true;
    updateFitScale();
    update();
}

void ImageCanvas::setZoomAndPan(double scale, const QPointF& offset) {
    m_scale = scale;
    m_offset = offset;
    m_autoFit = false;
    update();
}

void ImageCanvas::updateFitScale() {
    if (m_image.isNull() || width() <= 0 || height() <= 0) return;

    const double sx = static_cast<double>(width() - 20) / m_image.width();
    const double sy = static_cast<double>(height() - 40) / m_image.height();
    m_scale = std::min(sx, sy);
    if (m_scale <= 0.001) m_scale = 1.0;

    const double drawW = m_image.width() * m_scale;
    const double drawH = m_image.height() * m_scale;
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
    p.fillRect(rect(), QColor(28, 28, 28));

    if (!m_image.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 2.0);
        QRectF targetRect(m_offset.x(), m_offset.y(), m_image.width() * m_scale, m_image.height() * m_scale);
        p.drawImage(targetRect, m_image);

        // Subtle border around image
        p.setPen(QColor(60, 60, 60));
        p.drawRect(targetRect);
    } else {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, "No image loaded");
    }

    // Clean, minimalist title badge at top-left
    if (!m_title.isEmpty()) {
        QFont titleFont = font();
        titleFont.setPointSize(10);
        titleFont.setWeight(QFont::DemiBold);
        p.setFont(titleFont);
        QFontMetrics fm = p.fontMetrics();
        const int textW = fm.horizontalAdvance(m_title);
        const int textH = fm.height();
        QRect badgeRect(10, 10, textW + 14, textH + 6);

        p.setBrush(QColor(20, 20, 20, 210));
        p.setPen(QColor(55, 55, 55));
        p.drawRoundedRect(badgeRect, 3, 3);

        p.setPen(QColor(220, 220, 220));
        p.drawText(badgeRect, Qt::AlignCenter, m_title);
    }

    // Unobtrusive zoom badge at bottom-right
    if (!m_image.isNull()) {
        const QString zoomStr = QString::number(static_cast<int>(m_scale * 100)) + "%";
        QFont zoomFont = font();
        zoomFont.setPointSize(9);
        p.setFont(zoomFont);
        QFontMetrics fm = p.fontMetrics();
        const int textW = fm.horizontalAdvance(zoomStr);
        QRect badgeRect(width() - textW - 16, height() - 23, textW + 10, 16);

        p.setBrush(QColor(20, 20, 20, 180));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(badgeRect, 2, 2);

        p.setPen(QColor(160, 160, 160));
        p.drawText(badgeRect, Qt::AlignCenter, zoomStr);
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event) {
    if (m_image.isNull()) return;

    m_autoFit = false;
    const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);

    const QPointF mousePos = event->position();
    const QPointF imgPoint = (mousePos - m_offset) / m_scale;

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
        const QPoint delta = event->pos() - m_lastMousePos;
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

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        resetView();
        emit viewReset();
    }
}
