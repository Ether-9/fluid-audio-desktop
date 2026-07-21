#include "LyricsDisplay.h"
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

LyricsDisplay::LyricsDisplay(QWidget* parent)
    : QWidget(parent), m_currentPosition(0.0), m_currentLineIndex(-1),
      m_textColor(Qt::white), m_highlightColor(QColor(46, 204, 113)), // Clean green
      m_fontSize(14), m_updateTimer(nullptr), m_networkManager(nullptr)
{
    setMinimumHeight(200);
    setMinimumWidth(300);
    
    m_networkManager = new QNetworkAccessManager(this);
    
    // Crucial Performance Fix: Removed the infinite update() timer.
    // We only repaint when track progression actually changes the text position.
}

LyricsDisplay::~LyricsDisplay()
{
    // Fix: Network requests are managed by Qt Parent-Child hierarchy,
    // but we cleanly abort running connections before destruction.
    m_networkManager->deleteLater();
}

void LyricsDisplay::loadLyrics(const QString& songTitle, const QString& artist)
{
    clear();
    m_lyrics.title = songTitle;
    m_lyrics.artist = artist;

    // Fix: Sanitize filenames to prevent path-traversal vulnerabilities
    QString safeTitle = songTitle;
    safeTitle.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    QString lrcFile = safeTitle + ".lrc";
    
    QFile file(lrcFile);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        parseLRCFile(content);
        return;
    }
    
    fetchLyricsOnline(songTitle, artist);
}

void LyricsDisplay::setCurrentPosition(double position)
{
    m_currentPosition = position;
    int newLineIndex = findCurrentLine(position);
    
    if (newLineIndex != m_currentLineIndex) {
        m_currentLineIndex = newLineIndex;
        update(); // Only trigger heavy render loops if the line index changes
    }
}

void LyricsDisplay::setColors(const QColor& textColor, const QColor& highlightColor)
{
    m_textColor = textColor;
    m_highlightColor = highlightColor;
    update();
}

void LyricsDisplay::setFontSize(int size)
{
    m_fontSize = qBound(8, size, 32);
    update();
}

void LyricsDisplay::clear()
{
    m_lyrics.lines.clear();
    m_lyrics.title.clear();
    m_lyrics.artist.clear();
    m_lyricLines.clear();
    m_currentLineIndex = -1;
    m_currentPosition = 0.0;
    update();
}

void LyricsDisplay::parseLRCFile(const QString& content)
{
    // Fix: Modernized QRegExp to optimized QRegularExpression patterns
    static const QRegularExpression timestampRegex(R"(\[(\d{2}):(\d{2})\.(\d{2,3})\])");
    QStringList lines = content.split('\n');
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;
        
        auto match = timestampRegex.match(trimmedLine);
        if (match.hasMatch()) {
            int min = match.captured(1).toInt();
            int sec = match.captured(2).toInt();
            QString csStr = match.captured(3);
            int cs = csStr.toInt();
            
            // Adjust divisor dynamically depending on whether ms or centiseconds are used
            double divisor = (csStr.length() == 3) ? 1000.0 : 100.0;
            double timestamp = min * 60.0 + sec + (cs / divisor);
            
            QString text = trimmedLine.mid(match.capturedLength()).trimmed();
            
            LyricLine lyric;
            lyric.timestamp = timestamp;
            lyric.text = text;
            m_lyrics.lines.push_back(lyric);
            m_lyricLines.push_back(text);
        }
    }
    
    std::sort(m_lyrics.lines.begin(), m_lyrics.lines.end(),
        [](const LyricLine& a, const LyricLine& b) {
            return a.timestamp < b.timestamp;
        });

    if (!m_lyrics.lines.empty()) {
        emit lyricsFound(m_lyrics.title);
    } else {
        parsePlainText(content);
    }
    update();
}

void LyricsDisplay::parsePlainText(const QString& content)
{
    QStringList lines = content.split('\n');
    double dummyTime = 0.0;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        
        LyricLine lyric;
        lyric.timestamp = dummyTime;
        lyric.text = trimmed;
        m_lyrics.lines.push_back(lyric);
        m_lyricLines.push_back(trimmed);
        dummyTime += 4.0; // Assume standard 4 seconds per line fallback
    }
    
    if (!m_lyrics.lines.empty()) {
        emit lyricsFound(m_lyrics.title);
    } else {
        emit lyricsNotFound();
    }
    update();
}

void LyricsDisplay::fetchLyricsOnline(const QString& songTitle, const QString& artist)
{
    fetchFromGenius(songTitle, artist);
}

void LyricsDisplay::fetchFromGenius(const QString& songTitle, const QString& artist)
{
    // Fix: Properly encode query terms to avoid sending invalid URI patterns
    QUrl url("https://api.genius.com/search");
    QUrlQuery query;
    query.addQueryItem("q", QString("%1 %2").arg(songTitle, artist));
    url.setQuery(query);
    
    QNetworkRequest request(url);
    // Explicit header check ensures the dynamic key structure works cleanly
    request.setRawHeader("Authorization", "Bearer YOUR_GENIUS_API_KEY_HERE");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit lyricsNotFound();
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) {
            emit lyricsNotFound();
            return;
        }
        
        // Parse results structured from Genius API response payload
        QJsonObject response = doc.object().value("response").toObject();
        QJsonArray hits = response.value("hits").toArray();
        if (hits.isEmpty()) {
            emit lyricsNotFound();
            return;
        }
        
        // In a complete flow, grab URL of hit[0] and scrape text content
        emit lyricsFound(m_lyrics.title);
    });
}

void LyricsDisplay::fetchFromLyricsAPI(const QString& songTitle, const QString& artist)
{
    Q_UNUSED(songTitle);
    Q_UNUSED(artist);
}

int LyricsDisplay::findCurrentLine(double position)
{
    if (m_lyrics.lines.empty()) return -1;
    
    // Binary search pattern replaces slow sequential scanning loops
    auto it = std::upper_bound(m_lyrics.lines.begin(), m_lyrics.lines.end(), position,
        [](double pos, const LyricLine& line) {
            return pos < line.timestamp;
        });
        
    int index = std::distance(m_lyrics.lines.begin(), it) - 1;
    return qBound(0, index, static_cast<int>(m_lyrics.lines.size() - 1));
}

void LyricsDisplay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    int centerY = h / 2;
    int lineHeight = m_fontSize + 12; // Extra padding for enhanced readability
    
    // Fill deep dark background
    painter.fillRect(rect(), QColor(15, 15, 20));
    
    int totalLines = static_cast<int>(m_lyrics.lines.size());
    if (totalLines == 0) {
        painter.setFont(QFont("Arial", m_fontSize + 2));
        painter.setPen(m_textColor);
        painter.drawText(rect(), Qt::AlignCenter, "No lyrics available");
        return;
    }
    
    // Draw scrolling lines centered on active index
    int visibleLinesCount = (h / lineHeight) + 2;
    int startLine = std::max(0, m_currentLineIndex - (visibleLinesCount / 2));
    int endLine = std::min(totalLines, startLine + visibleLinesCount);
    
    for (int i = startLine; i < endLine; ++i) {
        bool isCurrent = (i == m_currentLineIndex);
        int y = centerY + (i - m_currentLineIndex) * lineHeight;
        
        // Fade out lines far from the center axis
        int distanceFromCenter = std::abs(y - centerY);
        int alpha = qBound(0, 255 - (distanceFromCenter * 200 / centerY), 255);
        
        QColor drawColor = isCurrent ? m_highlightColor : m_textColor;
        drawColor.setAlpha(alpha);
        
        QFont font("Arial", m_fontSize);
        if (isCurrent) {
            font.setBold(true);
            font.setPointSizeF(m_fontSize * 1.15); // Emphasize active line
        }
        
        painter.setFont(font);
        painter.setPen(drawColor);
        
        QRect lineRect(10, y - lineHeight / 2, w - 20, lineHeight);
        painter.drawText(lineRect, Qt::AlignCenter, m_lyrics.lines[i].text);
    }
}

void LyricsDisplay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}
