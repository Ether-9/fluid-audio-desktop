#include "WaveformWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent> // Added for QResizeEvent visibility
#include <QTimer>       // Added for QTimer usage details
#include <QPainterPath>
#include <cmath>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent), m_progress(0.0f), m_zoom(1.0f), 
      m_sampleRate(44100), m_waveformType(0), 
      m_totalSamples(0), m_visibleSamples(0),
      m_foregroundColor(Qt::green), m_backgroundColor(Qt::black),
      m_playheadColor(Qt::white), m_gridColor(QColor(50, 50, 50)),
      m_dragging(false), m_needsRedraw(true)
{
    setMinimumHeight(100);
    setMinimumWidth(200);
    setMouseTracking(true);
    
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (m_needsRedraw) {
            update();
            m_needsRedraw = false;
        }
    });
    m_refreshTimer->start(50);
}

WaveformWidget::~WaveformWidget()
{
    // QTimer is automatically deleted via parent child system, 
    // but explicit delete is safe since it was manually allocated.
    delete m_refreshTimer;
}

void WaveformWidget::setAudioData(const std::vector<float>& audioData, int sampleRate)
{
    m_audioData = audioData;
    m_sampleRate = sampleRate;
    m_totalSamples = audioData.size();
    m_needsRedraw = true;
    generateWaveform();
    update();
}

void WaveformWidget::setProgress(float progress)
{
    m_progress = qBound(0.0f, progress, 1.0f);
    m_needsRedraw = true;
    update();
}

void WaveformWidget::setZoom(float zoom)
{
    m_zoom = qBound(0.1f, zoom, 100.0f);
    m_needsRedraw = true;
    emit zoomChanged(m_zoom);
    update();
}

void WaveformWidget::setColors(const QColor& foreground, const QColor& background)
{
    m_foregroundColor = foreground;
    m_backgroundColor = background;
    m_needsRedraw = true;
    update();
}

void WaveformWidget::setPlayheadColor(const QColor& color)
{
    m_playheadColor = color;
    m_needsRedraw = true;
    update();
}

void WaveformWidget::setWaveformType(int type)
{
    m_waveformType = qBound(0, type, 3);
    m_needsRedraw = true;
    update();
}

void WaveformWidget::clear()
{
    m_audioData.clear();
    m_waveformData.clear();
    m_totalSamples = 0;
    m_needsRedraw = true;
    update();
}

void WaveformWidget::generateWaveform()
{
    if (m_audioData.empty()) {
        m_waveformData.clear();
        return;
    }
    
    int width = this->width();
    if (width <= 0) width = 200;
    
    int samplesPerPixel = m_totalSamples / width;
    if (samplesPerPixel < 1) samplesPerPixel = 1;
    
    m_waveformData.clear();
    m_waveformData.reserve(width * 2);
    
    for (int i = 0; i < width; ++i) {
        float minVal = 1.0f;
        float maxVal = -1.0f;
        
        int start = i * samplesPerPixel;
        int end = std::min(start + samplesPerPixel, m_totalSamples);
        
        for (int j = start; j < end; ++j) {
            float val = m_audioData[j];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
        }
        
        m_waveformData.push_back(minVal);
        m_waveformData.push_back(maxVal);
    }
}

void WaveformWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // Fill background
    painter.fillRect(rect, m_backgroundColor);
    
    // Draw grid
    drawGrid(painter, rect);
    
    // Draw waveform
    switch (m_waveformType) {
        case 0: drawStandardWaveform(painter, rect); break;
        case 1: drawFilledWaveform(painter, rect); break;
        case 2: drawBarsWaveform(painter, rect); break;
        case 3: drawSpectrogram(painter, rect); break;
        default: drawStandardWaveform(painter, rect); break;
    }
    
    // Draw playhead
    drawPlayhead(painter, rect);
}

void WaveformWidget::drawStandardWaveform(QPainter& painter, const QRect& rect)
{
    if (m_waveformData.empty() || rect.width() <= 0) return;
    
    int width = rect.width();
    int height = rect.height();
    int centerY = height / 2;
    
    QPen pen(m_foregroundColor);
    pen.setWidth(1);
    painter.setPen(pen);
    
    for (int i = 0; i < width && i * 2 < (int)m_waveformData.size(); ++i) {
        float minVal = m_waveformData[i * 2];
        float maxVal = m_waveformData[i * 2 + 1];
        
        int yMin = centerY + minVal * (height / 2);
        int yMax = centerY + maxVal * (height / 2);
        
        painter.drawLine(i, yMin, i, yMax);
    }
}

void WaveformWidget::drawFilledWaveform(QPainter& painter, const QRect& rect)
{
    if (m_waveformData.empty() || rect.width() <= 0) return;
    
    int width = rect.width();
    int height = rect.height();
    int centerY = height / 2;
    
    QPen pen(m_foregroundColor);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(QBrush(m_foregroundColor));
    
    QPainterPath path;
    path.moveTo(0, centerY);
    
    for (int i = 0; i < width && i * 2 < (int)m_waveformData.size(); ++i) {
        float maxVal = m_waveformData[i * 2 + 1];
        int yMax = centerY + maxVal * (height / 2);
        path.lineTo(i, yMax);
    }
    
    path.lineTo(width - 1, centerY);
    path.closeSubpath();
    painter.drawPath(path);
}

void WaveformWidget::drawBarsWaveform(QPainter& painter, const QRect& rect)
{
    if (m_waveformData.empty() || rect.width() <= 0) return;
    
    int width = rect.width();
    int height = rect.height();
    int centerY = height / 2;
    int barWidth = std::max(2, width / 100);
    int spacing = 2;
    
    QPen pen(m_foregroundColor);
    pen.setWidth(1);
    painter.setPen(pen);
    
    for (int i = 0; i < width && i * 2 < (int)m_waveformData.size(); i += barWidth + spacing) {
        float maxVal = m_waveformData[i * 2 + 1];
        int barHeight = std::abs(maxVal * (height / 2));
        
        painter.fillRect(i, centerY - barHeight, barWidth, barHeight * 2, m_foregroundColor);
    }
}

void WaveformWidget::drawSpectrogram(QPainter& painter, const QRect& rect)
{
    // Placeholder for spectrogram
    painter.fillRect(rect, m_backgroundColor);
}

void WaveformWidget::drawPlayhead(QPainter& painter, const QRect& rect)
{
    int x = static_cast<int>(m_progress * rect.width());
    
    QPen pen(m_playheadColor);
    pen.setWidth(2);
    painter.setPen(pen);
    
    painter.drawLine(x, 0, x, rect.height());
    
    // Draw circle at playhead
    painter.setBrush(QBrush(m_playheadColor));
    painter.drawEllipse(QPointF(x, rect.height() / 2.0), 5.0, 5.0); // Fixed: Precision cast to QPointF to suppress type ambiguity
}

void WaveformWidget::drawGrid(QPainter& painter, const QRect& rect)
{
    QPen pen(m_gridColor);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    
    // Horizontal grid lines
    for (int i = 0; i <= 4; ++i) {
        int y = i * rect.height() / 4;
        painter.drawLine(0, y, rect.width(), y);
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        float position = static_cast<float>(event->pos().x()) / width();
        emit positionClicked(position);
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && event->buttons() & Qt::LeftButton) {
        float position = static_cast<float>(event->pos().x()) / width();
        emit positionClicked(position);
    }
}

void WaveformWidget::wheelEvent(QWheelEvent* event)
{
    float zoomDelta = 1.0f + (event->angleDelta().y() / 120.0f) * 0.1f;
    float newZoom = m_zoom * zoomDelta;
    setZoom(newZoom);
}

void WaveformWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    m_needsRedraw = true;
    generateWaveform();
}
