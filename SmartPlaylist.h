#ifndef SMART_PLAYLIST_H
#define SMART_PLAYLIST_H

#include <QWidget> // Change from <QObject> to <QWidget>
#include <QString>
#include <vector>
#include "MusicDatabase.h"

enum class PlaylistRuleType {
    TitleContains,
    ArtistContains,
    GenreEquals,
    YearBetween,
    PlayCountGreater,
    LastPlayedDays,
    FrequencyEquals,
    MoodEquals,
    BPMBetween,
    DurationBetween
};

struct PlaylistRule {
    PlaylistRuleType type;
    QString value;
    bool isAnd; // true: AND, false: OR
};

class SmartPlaylist : public QWidget // Change public QObject to public QWidget
{
    Q_OBJECT

public:
    explicit SmartPlaylist(QWidget* parent = nullptr); // Change QObject* to QWidget*
    ~SmartPlaylist();

    void setName(const QString& name);
    void setRules(const std::vector<PlaylistRule>& rules);
    void setSortBy(const QString& sortBy, bool ascending = true);
    void setMaxSongs(int maxSongs);
    
    void generate();
    void refresh();
    
    // Missing Method Fix: Declare the method called by MainWindow.cpp
    void createNewFromSelection(); 

    std::vector<SongMetadata> getSongs() const;
    QString getName() const { return m_name; }
    int getSongCount() const { return m_songs.size(); }
    float getTotalDuration() const;
    
signals:
    void generated(const QString& name, int count);
    void refreshed(const QString& name, int count);

private:
    bool matchesRules(const SongMetadata& song);
    std::vector<SongMetadata> sortSongs(const std::vector<SongMetadata>& songs);
    
    QString m_name;
    std::vector<PlaylistRule> m_rules;
    QString m_sortBy;
    bool m_ascending;
    int m_maxSongs;
    std::vector<SongMetadata> m_songs;
};

#endif