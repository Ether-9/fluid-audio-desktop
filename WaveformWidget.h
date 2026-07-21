#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <vector>

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    ~WaveformWidget();

    void setAudioData(const std::vector<float>& audioData, int sampleRate);
    void setProgress(float progress);
    void setZoom(float zoom);
    void setColors(const QColor& foreground, const QColor& background);
    void setPlayheadColor(const QColor& color);
    void setWaveformType(int type); // 0: Standard, 1: Filled, 2: Bars, 3: Spectrogram

    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void positionClicked(float position);
    void zoomChanged(float zoom);

private:
    void generateWaveform();
    void drawStandardWaveform(QPainter& painter, const QRect& rect);
    void drawFilledWaveform(QPainter& painter, const QRect& rect);
    void drawBarsWaveform(QPainter& painter, const QRect& rect);
    void drawSpectrogram(QPainter& painter, const QRect& rect);
    void drawPlayhead(QPainter& painter, const QRect& rect);
    void drawGrid(QPainter& painter, const QRect& rect);
    
    std::vector<float> m_audioData;
    std::vector<float> m_waveformData;
    float m_progress;
    float m_zoom;
    int m_sampleRate;
    int m_waveformType;
    int m_totalSamples;
    int m_visibleSamples;
    
    QColor m_foregroundColor;
    QColor m_backgroundColor;
    QColor m_playheadColor;
    QColor m_gridColor;
    
    QTimer* m_refreshTimer;
    bool m_dragging;
    bool m_needsRedraw;
};

#endif
