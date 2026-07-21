#include "AlbumArtWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <QWidget>

// Mock placeholder class to preserve dependency layout for compilation
class AlbumArtManager {
public:
    QPixmap getAlbumArt(const QString&) { return QPixmap(); }
};

AlbumArtWidget::AlbumArtWidget(QWidget* parent)
    : QWidget(parent), m_shape(Square), m_effect(Shadow), 
      m_size(200), m_isVisible(true), m_rotate(false), 
      m_rotationAngle(0.0), m_scale(1.0), m_opacity(1.0)
{
    setFixedSize(m_size, m_size);
    setMinimumSize(50, 50);
    
    m_art = QPixmap(m_size, m_size);
    m_art.fill(QColor(30, 30, 40));
    m_currentArt = m_art;
    
    // Fix: Strings point to matching Q_PROPERTY declarations on target objects
    m_fadeAnimation = new QPropertyAnimation(this, "opacity");
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    
    m_scaleAnimation = new QPropertyAnimation(this, "scale");
    m_scaleAnimation->setDuration(200);
    m_scaleAnimation->setStartValue(0.8);
    m_scaleAnimation->setEndValue(1.0);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutBack);
    
    m_rotationAnimation = new QPropertyAnimation(this, "rotationAngle");
    m_rotationAnimation->setDuration(10000);
    m_rotationAnimation->setStartValue(0.0);
    m_rotationAnimation->setEndValue(360.0);
    m_rotationAnimation->setLoopCount(-1);
    
    m_animationGroup = new QParallelAnimationGroup(this);
    m_animationGroup->addAnimation(m_fadeAnimation);
    m_animationGroup->addAnimation(m_scaleAnimation);
}

AlbumArtWidget::~AlbumArtWidget()
{
    // Fix: Rely on parent ownership or explicitly remove animation bindings to prevent dangling exceptions
}

void AlbumArtWidget::setArt(const QPixmap& art)
{
    if (!art.isNull()) {
        m_art = art;
        m_currentArt = art;
        update();
    }
}

void AlbumArtWidget::setArt(const QString& songPath)
{
    AlbumArtManager manager;
    QPixmap art = manager.getAlbumArt(songPath);
    setArt(art);
}

void AlbumArtWidget::setSize(int size)
{
    m_size = size;
    setFixedSize(size, size);
    update();
}

void AlbumArtWidget::setShape(Shape shape)
{
    m_shape = shape;
    update();
}

void AlbumArtWidget::setEffect(Effect effect)
{
    m_effect = effect;
    update();
}

void AlbumArtWidget::showArt()
{
    m_isVisible = true;
    fadeIn();
}

void AlbumArtWidget::hideArt()
{
    m_isVisible = false;
    fadeOut();
}

void AlbumArtWidget::fadeIn(int duration)
{
    m_fadeAnimation->stop();
    m_scaleAnimation->stop();
    
    m_fadeAnimation->setDuration(duration);
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(1.0);
    
    m_scaleAnimation->setStartValue(m_scale < 0.99 ? m_scale : 0.8);
    m_scaleAnimation->setEndValue(1.0);
    
    m_animationGroup->start();
}

void AlbumArtWidget::fadeOut(int duration)
{
    m_fadeAnimation->stop();
    m_fadeAnimation->setDuration(duration);
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->start();
}

void AlbumArtWidget::rotateArt(bool enable)
{
    m_rotate = enable;
    if (enable) {
        if (m_rotationAnimation->state() != QAbstractAnimation::Running) {
            m_rotationAnimation->start();
        }
    } else {
        m_rotationAnimation->stop();
        setRotationAngle(0.0);
    }
}

void AlbumArtWidget::spinArt(int duration)
{
    m_rotationAnimation->stop();
    m_rotationAnimation->setDuration(duration);
    m_rotationAnimation->start();
}

void AlbumArtWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    if (!m_isVisible || m_opacity <= 0.0) return;
    
    painter.save();
    painter.setOpacity(m_opacity);
    
    // Fix: Correct mathematical pivot scaling behavior around center coordinate paths
    QPointF center(width() / 2.0, height() / 2.0);
    painter.translate(center);
    
    if (m_rotate || m_rotationAngle > 0.0) {
        painter.rotate(m_rotationAngle);
    }
    
    if (m_scale != 1.0) {
        painter.scale(m_scale, m_scale);
    }
    
    // Shift canvas back relative to localized drawing bounds
    painter.translate(-center.x(), -center.y());
    
    QRect rect(0, 0, width(), height());
    
    switch (m_shape) {
        case Square:   drawSquareShape(painter, rect);   break;
        case Rounded:  drawRoundedShape(painter, rect);  break;
        case Circle:   drawCircleShape(painter, rect);   break;
        case Hexagon:  drawHexagonShape(painter, rect);  break;
        case Diamond:  drawDiamondShape(painter, rect);  break;
        default:       drawSquareShape(painter, rect);   break;
    }
    
    painter.restore();
}

void AlbumArtWidget::drawSquareShape(QPainter& painter, const QRect& rect)
{
    painter.drawPixmap(rect, m_currentArt);
    applyEffect(painter, rect);
}

void AlbumArtWidget::drawRoundedShape(QPainter& painter, const QRect& rect)
{
    painter.save();
    QPainterPath path;
    path.addRoundedRect(rect, 15, 15);
    painter.setClipPath(path);
    painter.drawPixmap(rect, m_currentArt);
    painter.restore();
    applyEffect(painter, rect);
}

void AlbumArtWidget::drawCircleShape(QPainter& painter, const QRect& rect)
{
    painter.save();
    QPainterPath path;
    path.addEllipse(rect);
    painter.setClipPath(path);
    painter.drawPixmap(rect, m_currentArt);
    painter.restore();
    applyEffect(painter, rect);
}

void AlbumArtWidget::drawHexagonShape(QPainter& painter, const QRect& rect)
{
    painter.save();
    QPainterPath path;
    QPolygonF polygon;
    QPointF center = rect.center();
    qreal radius = rect.width() / 2.0;
    
    for (int i = 0; i < 6; ++i) {
        qreal angle = i * 60.0 * M_PI / 180.0 - 30.0 * M_PI / 180.0;
        qreal x = center.x() + radius * std::cos(angle);
        qreal y = center.y() + radius * std::sin(angle);
        polygon << QPointF(x, y);
    }
    path.addPolygon(polygon);
    painter.setClipPath(path);
    painter.drawPixmap(rect, m_currentArt);
    painter.restore();
    applyEffect(painter, rect);
}

void AlbumArtWidget::drawDiamondShape(QPainter& painter, const QRect& rect)
{
    painter.save();
    QPainterPath path;
    QPolygonF polygon;
    QPointF center = rect.center();
    qreal radius = rect.width() / 2.0;
    
    polygon << QPointF(center.x(), center.y() - radius);
    polygon << QPointF(center.x() + radius, center.y());
    polygon << QPointF(center.x(), center.y() + radius);
    polygon << QPointF(center.x() - radius, center.y());
    
    path.addPolygon(polygon);
    painter.setClipPath(path);
    painter.drawPixmap(rect, m_currentArt);
    painter.restore();
    applyEffect(painter, rect);
}

void AlbumArtWidget::applyEffect(QPainter& painter, const QRect& rect)
{
    switch (m_effect) {
        case Shadow:  applyShadowEffect(painter, rect);  break;
        case Glow:    applyGlowEffect(painter, rect);    break;
        case Glass:   applyGlassEffect(painter, rect);   break;
        case Vinyl:   applyVinylEffect(painter, rect);   break;
        default:                                         break;
    }
}

void AlbumArtWidget::applyShadowEffect(QPainter& painter, const QRect& rect)
{
    // Fix: Critical mitigation. Instantiating a new QGraphicsDropShadowEffect inside a paintEvent causes immediate memory leak loops.
    // Instead, we manually sketch a smooth outer composite shadow line directly using the canvas painter context layers.
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 35));
    painter.drawRoundedRect(rect.adjusted(2, 4, -2, 0), 10, 10);
    painter.restore();
}

void AlbumArtWidget::applyGlowEffect(QPainter& painter, const QRect& rect)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 100), 3));
    if (m_shape == Circle) painter.drawEllipse(rect);
    else painter.drawRoundedRect(rect, 12, 12);
    painter.restore();
}

void AlbumArtWidget::applyGlassEffect(QPainter& painter, const QRect& rect)
{
    painter.save();
    QLinearGradient glassGradient(0, 0, 0, rect.height());
    glassGradient.setColorAt(0.0, QColor(255, 255, 255, 45));
    glassGradient.setColorAt(0.4, QColor(255, 255, 255, 15));
    glassGradient.setColorAt(0.41, QColor(255, 255, 255, 0));
    glassGradient.setColorAt(1.0, QColor(255, 255, 255, 20));
    
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.fillRect(rect, glassGradient);
    painter.restore();
}

void AlbumArtWidget::applyVinylEffect(QPainter& painter, const QRect& rect)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    
    // Outer grooved track lines
    painter.setPen(QPen(QColor(20, 20, 20, 220), 2));
    painter.drawEllipse(rect);
    
    QRect innerRect = rect.adjusted(10, 10, -10, -10);
    painter.setPen(QPen(QColor(40, 40, 40, 120), 1));
    painter.drawEllipse(innerRect);
    
    // Internal center spindle label matrix
    QRect labelRect = rect.adjusted(rect.width() / 3, rect.height() / 3, -rect.width() / 3, -rect.height() / 3);
    painter.setBrush(QColor(40, 40, 50, 240));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(labelRect);
    
    int holeSize = 10;
    QRect holeRect(rect.center().x() - holeSize / 2, rect.center().y() - holeSize / 2, holeSize, holeSize);
    painter.setBrush(QColor(80, 80, 90));
    painter.drawEllipse(holeRect);
    painter.restore();
}

void AlbumArtWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit artClicked();
    }
    QWidget::mousePressEvent(event);
}

void AlbumArtWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit artDoubleClicked();
        rotateArt(!m_rotate);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AlbumArtWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}
