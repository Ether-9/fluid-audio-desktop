#include "MusicDatabase.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

MusicDatabase::MusicDatabase(QObject* parent) : QObject(parent)
{
    qDebug() << "MusicDatabase initialized";
    m_playlistNames << "Favorites" << "Recent" << "Most Played";
    for (const QString& name : m_playlistNames) {
        m_playlists[name] = std::vector<PlaylistSong>();
    }
}

MusicDatabase::~MusicDatabase()
{
    qDebug() << "MusicDatabase destroyed";
}

void MusicDatabase::scanFolder(const QString& folder)
{
    qDebug() << "Scanning folder:" << folder;
    QDir dir(folder);
    QStringList filters;
    filters << "*.mp3" << "*.wav" << "*.flac" << "*.m4a" << "*.ogg" << "*.aac";
    QStringList files = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    
    for (const QString& file : files) {
        SongMetadata song;
        song.filepath = dir.filePath(file);
        song.title = QFileInfo(file).baseName();
        song.artist = "Unknown";
        song.album = "Unknown";
        song.genre = "Unknown";
        song.duration = 0;
        song.playCount = 0;
        song.frequency = 440;
        addSong(song);
    }
    
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subdir : subdirs) {
        scanFolder(dir.filePath(subdir));
    }
}

void MusicDatabase::addSong(const SongMetadata& song)
{
    for (const auto& s : m_songs) {
        if (s.filepath == song.filepath) {
            return;
        }
    }
    m_songs.push_back(song);
    m_recentlyAdded.push_back(song);
    if (m_recentlyAdded.size() > 50) {
        m_recentlyAdded.erase(m_recentlyAdded.begin());
    }
}

void MusicDatabase::addSong(const QString& filepath, const QString& title, const QString& artist, float duration)
{
    SongMetadata song;
    song.filepath = filepath;
    song.title = title;
    song.artist = artist;
    song.album = "";
    song.genre = "Unknown";
    song.duration = duration;
    song.playCount = 0;
    song.frequency = 440;
    addSong(song);
}

void MusicDatabase::clearHistory() {
    // Assuming your database uses an SQLite or SQL query, or an in-memory vector.
    // If you are using SQLite, run:
    QSqlQuery query;
    query.exec("DELETE FROM history"); // Or whatever your history table is named!
    
    // If it's an in-memory std::vector or QStringList:
    // historyList.clear();
}

std::vector<SongMetadata> MusicDatabase::getAllSongs()
{
    return m_songs;
}

std::vector<SongMetadata> MusicDatabase::getRecentSongs(int limit)
{
    std::vector<SongMetadata> recent;
    QStringList addedPaths; // Prevent duplicate songs in the recent display list
    
    for (const QString& path : m_history) {
        if (recent.size() >= static_cast<size_t>(limit)) break;
        if (addedPaths.contains(path)) continue;
        
        for (const auto& song : m_songs) {
            if (song.filepath == path) {
                recent.push_back(song);
                addedPaths.append(path);
                break;
            }
        }
    }
    return recent;
}

std::vector<SongMetadata> MusicDatabase::getMostPlayedSongs(int limit)
{
    std::vector<SongMetadata> sorted = m_songs;
    
    // Sort songs: highest play count first
    std::sort(sorted.begin(), sorted.end(), [](const SongMetadata& a, const SongMetadata& b) {
        return a.playCount > b.playCount;
    });
    
    std::vector<SongMetadata> mostPlayed;
    for (const auto& song : sorted) {
        if (mostPlayed.size() >= static_cast<size_t>(limit)) break;
        if (song.playCount > 0) {
            mostPlayed.push_back(song);
        }
    }
    return mostPlayed;
}

std::vector<SongMetadata> MusicDatabase::getRecentlyAdded(int limit)
{
    std::vector<SongMetadata> recent;
    int count = 0;
    for (auto it = m_recentlyAdded.rbegin(); it != m_recentlyAdded.rend() && count < limit; ++it) {
        recent.push_back(*it);
        count++;
    }
    return recent;
}

SongMetadata MusicDatabase::getMetadata(const QString& filepath)
{
    for (auto& song : m_songs) {
        if (song.filepath == filepath) {
            return song;
        }
    }
    SongMetadata meta;
    meta.filepath = filepath;
    meta.title = QFileInfo(filepath).baseName();
    meta.artist = "Unknown";
    meta.duration = 0;
    meta.playCount = 0;
    meta.frequency = 440;
    return meta;
}

void MusicDatabase::incrementPlayCount(const QString& filepath)
{
    for (auto& song : m_songs) {
        if (song.filepath == filepath) {
            song.playCount++;
            qDebug() << "Incremented play count for:" << song.title << "to" << song.playCount;
            break;
        }
    }
}

// Unified function to record plays
void MusicDatabase::registerSongPlay(const QString& filepath, int frequency)
{
    // 1. Increment Play Count
    incrementPlayCount(filepath);
    
    // 2. Add raw path to history queue
    m_history.prepend(filepath);
    if (m_history.size() > 100) {
        m_history.removeLast();
    }
    
    // 3. Keep standard playlist matching
    addToPlaylist("Recent", filepath);
    
    qDebug() << "Registered song play for path:" << filepath << "@" << frequency << "Hz";
}

QStringList MusicDatabase::getPlaylists()
{
    return m_playlistNames;
}

std::vector<PlaylistSong> MusicDatabase::getPlaylist(const QString& name)
{
    if (m_playlists.find(name) != m_playlists.end()) {
        return m_playlists[name];
    }
    return std::vector<PlaylistSong>();
}

void MusicDatabase::createPlaylist(const QString& name)
{
    if (!m_playlistNames.contains(name)) {
        m_playlistNames.append(name);
        m_playlists[name] = std::vector<PlaylistSong>();
        qDebug() << "Created playlist:" << name;
    }
}

void MusicDatabase::deletePlaylist(const QString& name)
{
    if (name == "Favorites" || name == "Recent" || name == "Most Played") {
        qDebug() << "Cannot delete default system playlist:" << name;
        return;
    }
    if (m_playlists.find(name) != m_playlists.end()) {
        m_playlists.erase(name);
        m_playlistNames.removeAll(name);
        qDebug() << "Deleted playlist:" << name;
    }
}

void MusicDatabase::addToPlaylist(const QString& playlist, const QString& filepath)
{
    if (m_playlists.find(playlist) == m_playlists.end()) {
        createPlaylist(playlist);
    }
    
    for (const auto& song : m_playlists[playlist]) {
        if (song.filepath == filepath) {
            return;
        }
    }
    
    SongMetadata meta = getMetadata(filepath);
    PlaylistSong ps;
    ps.filepath = filepath;
    ps.title = meta.title;
    ps.artist = meta.artist;
    ps.duration = meta.duration;
    
    m_playlists[playlist].push_back(ps);
    qDebug() << "Added to playlist" << playlist << ":" << filepath;
}

void MusicDatabase::removeFromPlaylist(const QString& playlist, const QString& filepath)
{
    if (playlist == "Favorites" || playlist == "Recent" || playlist == "Most Played") {
        qDebug() << "System playlists bypass simple manual removal constraints";
    }
    
    if (m_playlists.find(playlist) != m_playlists.end()) {
        auto& songs = m_playlists[playlist];
        songs.erase(std::remove_if(songs.begin(), songs.end(),
            [&filepath](const PlaylistSong& song) {
                return song.filepath == filepath;
            }), songs.end());
        qDebug() << "Removed from playlist" << playlist << ":" << filepath;
    }
}

bool MusicDatabase::playlistExists(const QString& name)
{
    return m_playlistNames.contains(name);
}

QStringList MusicDatabase::getHistory(int limit)
{
    QStringList list;
    int count = 0;
    for (const QString& path : m_history) {
        if (count >= limit) break;
        list.append(path);
        count++;
    }
    return list;
}

void MusicDatabase::addToHistory(const QString& filepath, int frequency)
{
    registerSongPlay(filepath, frequency);
}
