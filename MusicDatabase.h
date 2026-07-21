#ifndef MUSICDATABASE_H
#define MUSICDATABASE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <map>

struct SongMetadata {
    QString filepath;
    QString title;
    QString artist;
    QString album;
    QString genre;      
    float duration;
    int playCount;
    int frequency;      
};

struct PlaylistSong {
    QString title;
    QString filepath;
    QString artist;
    float duration;
};

class MusicDatabase : public QObject
{
    Q_OBJECT

public:
    explicit MusicDatabase(QObject* parent = nullptr);
    ~MusicDatabase();

    void scanFolder(const QString& folder);
    void addSong(const SongMetadata& song);
    void addSong(const QString& filepath, const QString& title, const QString& artist = "", float duration = 0);
    std::vector<SongMetadata> getAllSongs();
    std::vector<SongMetadata> getRecentSongs(int limit = 10);
    std::vector<SongMetadata> getMostPlayedSongs(int limit = 10);
    std::vector<SongMetadata> getRecentlyAdded(int limit = 10);
    SongMetadata getMetadata(const QString& filepath);
    void incrementPlayCount(const QString& filepath);
    void clearHistory();
    
    // Core connector for AudioEngine/MainWindow to record a play event
    void registerSongPlay(const QString& filepath, int frequency);

    QStringList getPlaylists();
    std::vector<PlaylistSong> getPlaylist(const QString& name);
    void createPlaylist(const QString& name);
    void deletePlaylist(const QString& name);
    void addToPlaylist(const QString& playlist, const QString& filepath);
    void removeFromPlaylist(const QString& playlist, const QString& filepath);
    bool playlistExists(const QString& name);
    
    QStringList getHistory(int limit = 50);
    void addToHistory(const QString& filepath, int frequency);

private:
    std::vector<SongMetadata> m_songs;
    std::vector<SongMetadata> m_recentlyAdded;
    std::map<QString, std::vector<PlaylistSong>> m_playlists;
    QStringList m_playlistNames;
    
    // Changed to raw paths for perfect, unbreakable lookups
    QStringList m_history; 
};

#endif
