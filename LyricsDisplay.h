#ifndef LYRICSDISPLAY_H
#define LYRICSDISPLAY_H

#include <QWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <vector>

struct LyricLine {
    double timestamp;    // in seconds
    QString text;
};

struct LyricData {
    QString title;
    QString artist;
    QString album;
    std::vector<LyricLine> lines;
};

class LyricsDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit LyricsDisplay(QWidget* parent = nullptr);
    ~LyricsDisplay() override;

    void loadLyrics(const QString& songTitle, const QString& artist);
    void setCurrentPosition(double position);
    void setColors(const QColor& textColor, const QColor& highlightColor);
    void setFontSize(int size);
    void clear();

signals:
    void lyricsFound(const QString& lyrics);
    void lyricsNotFound();

private:
    void parseLRCFile(const QString& content);
    void parsePlainText(const QString& content);
    void fetchLyricsOnline(const QString& songTitle, const QString& artist);
    void fetchFromGenius(const QString& songTitle, const QString& artist);
    void fetchFromLyricsAPI(const QString& songTitle, const QString& artist);
    int findCurrentLine(double position);
    
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
    LyricData m_lyrics;
    double m_currentPosition;
    int m_currentLineIndex;
    QColor m_textColor;
    QColor m_highlightColor;
    int m_fontSize;
    QTimer* m_updateTimer;
    QNetworkAccessManager* m_networkManager;
    
    std::vector<QString> m_lyricLines;
};

#endif // LYRICSDISPLAY_H
