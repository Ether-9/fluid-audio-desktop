#ifndef ALBUM_ART_WIDGET_H
#define ALBUM_ART_WIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class AlbumArtWidget : public QWidget
{
    Q_OBJECT
    
    // Crucial Fix: Expose Q_PROPERTY bindings to allow QPropertyAnimation to modify these states at runtime
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(qreal scale READ scale WRITE setScale)
    Q_PROPERTY(qreal rotationAngle READ rotationAngle WRITE setRotationAngle)

public:
    // Crucial Fix: Enums moved to the top of the class block so they compile before public methods use them as parameters
    enum Shape {
        Square,
        Rounded,
        Circle,
        Hexagon,
        Diamond
    };
    
    enum Effect {
        None,
        Shadow,
        Glow,
        Blur,
        Glass,
        Gradient,
        Vinyl
    };

    explicit AlbumArtWidget(QWidget* parent = nullptr);
    ~AlbumArtWidget();

    void setArt(const QPixmap& art);
    void setArt(const QString& songPath);
    void setSize(int size);
    void setShape(Shape shape);
    void setEffect(Effect effect);
    
    void showArt();
    void hideArt();
    void fadeIn(int duration = 500);
    void fadeOut(int duration = 500);
    void rotateArt(bool enable);
    void spinArt(int duration = 10000);

    // Dynamic property accessors
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity) { m_opacity = opacity; update(); }

    qreal scale() const { return m_scale; }
    void setScale(qreal scale) { m_scale = scale; update(); }

    qreal rotationAngle() const { return m_rotationAngle; }
    void setRotationAngle(qreal angle) { m_rotationAngle = angle; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override; // Restored matching slot from implementation

signals:
    void artClicked();
    void artDoubleClicked();

private:
    void drawSquareShape(QPainter& painter, const QRect& rect);
    void drawRoundedShape(QPainter& painter, const QRect& rect);
    void drawCircleShape(QPainter& painter, const QRect& rect);
    void drawHexagonShape(QPainter& painter, const QRect& rect);
    void drawDiamondShape(QPainter& painter, const QRect& rect);
    
    void applyEffect(QPainter& painter, const QRect& rect);
    void applyShadowEffect(QPainter& painter, const QRect& rect);
    void applyGlowEffect(QPainter& painter, const QRect& rect);
    void applyGlassEffect(QPainter& painter, const QRect& rect);
    void applyVinylEffect(QPainter& painter, const QRect& rect);
    
    QPixmap m_art;
    QPixmap m_currentArt;
    Shape m_shape;
    Effect m_effect;
    int m_size;
    bool m_isVisible;
    bool m_rotate;
    
    // Fixed: Unified types to double/qreal to match QPropertyAnimation internal types
    qreal m_rotationAngle;
    qreal m_scale;
    qreal m_opacity;
    
    QPropertyAnimation* m_fadeAnimation;
    QPropertyAnimation* m_scaleAnimation;
    QPropertyAnimation* m_rotationAnimation;
    QParallelAnimationGroup* m_animationGroup;
};

#endif // ALBUM_ART_WIDGET_H
