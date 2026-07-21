#include "SmartPlaylist.h"
#include <algorithm>
#include <QDateTime>
#include <QVariant>
#include <QWidget>
#include <QMessageBox> // Added to display playlist action results if needed

SmartPlaylist::SmartPlaylist(QWidget* parent)
    : QWidget(parent), m_maxSongs(50), m_ascending(true)
{
    m_sortBy = "title";
}

SmartPlaylist::~SmartPlaylist()
{
}

void SmartPlaylist::setName(const QString& name)
{
    m_name = name;
}

void SmartPlaylist::setRules(const std::vector<PlaylistRule>& rules)
{
    m_rules = rules;
}

void SmartPlaylist::setSortBy(const QString& sortBy, bool ascending)
{
    m_sortBy = sortBy;
    m_ascending = ascending;
}

void SmartPlaylist::setMaxSongs(int maxSongs)
{
    m_maxSongs = maxSongs;
}

void SmartPlaylist::generate()
{
    MusicDatabase db;
    auto allSongs = db.getAllSongs();
    m_songs.clear();
    
    for (const auto& song : allSongs) {
        if (matchesRules(song)) {
            m_songs.push_back(song);
        }
    }
    
    m_songs = sortSongs(m_songs);
    
    // Limit to max songs
    if ((int)m_songs.size() > m_maxSongs) {
        m_songs.resize(m_maxSongs);
    }
    
    emit generated(m_name, m_songs.size());
}

void SmartPlaylist::refresh()
{
    generate();
    emit refreshed(m_name, m_songs.size());
}

std::vector<SongMetadata> SmartPlaylist::getSongs() const
{
    return m_songs;
}

float SmartPlaylist::getTotalDuration() const
{
    float total = 0.0f;
    for (const auto& song : m_songs) {
        total += song.duration;
    }
    return total;
}

bool SmartPlaylist::matchesRules(const SongMetadata& song)
{
    if (m_rules.empty()) return true;
    
    bool match = true;
    bool firstRule = true;
    
    for (const auto& rule : m_rules) {
        bool ruleMatch = false;
        
        switch (rule.type) {
            case PlaylistRuleType::TitleContains:
                ruleMatch = song.title.contains(rule.value, Qt::CaseInsensitive);
                break;
            case PlaylistRuleType::ArtistContains:
                ruleMatch = song.artist.contains(rule.value, Qt::CaseInsensitive);
                break;
            case PlaylistRuleType::GenreEquals:
                ruleMatch = song.genre == rule.value;
                break;
            case PlaylistRuleType::YearBetween:
                // Parse years and compare (Stubbed/Extendable)
                break;
            case PlaylistRuleType::PlayCountGreater:
                ruleMatch = song.playCount > rule.value.toInt();
                break;
            case PlaylistRuleType::LastPlayedDays:
                // Compare last played date (Stubbed/Extendable)
                break;
            case PlaylistRuleType::FrequencyEquals:
                ruleMatch = QVariant(song.frequency).toString() == rule.value;
                break;
            case PlaylistRuleType::MoodEquals:
                // Compare mood (Stubbed/Extendable)
                break;
            case PlaylistRuleType::BPMBetween:
                // Compare BPM range (Stubbed/Extendable)
                break;
            case PlaylistRuleType::DurationBetween:
                // Compare duration range (Stubbed/Extendable)
                break;
            default:
                ruleMatch = true;
                break;
        }
        
        if (firstRule) {
            match = ruleMatch;
            firstRule = false;
        } else if (rule.isAnd) {
            match = match && ruleMatch;
        } else {
            match = match || ruleMatch;
        }
    }
    
    return match;
}

std::vector<SongMetadata> SmartPlaylist::sortSongs(const std::vector<SongMetadata>& songs)
{
    auto sorted = songs;
    std::sort(sorted.begin(), sorted.end(), [this](const SongMetadata& a, const SongMetadata& b) {
        if (m_sortBy == "title") {
            return m_ascending ? a.title < b.title : a.title > b.title;
        } else if (m_sortBy == "artist") {
            return m_ascending ? a.artist < b.artist : a.artist > b.artist;
        } else if (m_sortBy == "duration") {
            return m_ascending ? a.duration < b.duration : a.duration > b.duration;
        } else if (m_sortBy == "playCount") {
            return m_ascending ? a.playCount < b.playCount : a.playCount > b.playCount;
        } else if (m_sortBy == "frequency") {
            return m_ascending ? a.frequency < b.frequency : a.frequency > b.frequency;
        }
        return m_ascending ? a.title < b.title : a.title > b.title;
    });
    return sorted;
}

// --- ADDED SELECTION GENERATION FUNCTION ---
void SmartPlaylist::createNewFromSelection()
{
    // Generates a smart rule set automatically from the active selection.
    // Example logic: Gather selected tracks, establish rules based on their common artists or genres.
    m_songs.clear();
    
    // Quick procedural fallback generation if called by UI
    m_name = "New Selection Playlist";
    m_rules.clear();
    
    // Default fallback rule is "Title Contains" with blank string to grab current songs, or customized via UI selection
    PlaylistRule defaultSelectionRule;
    defaultSelectionRule.type = PlaylistRuleType::TitleContains;
    defaultSelectionRule.value = "";
    defaultSelectionRule.isAnd = true;
    m_rules.push_back(defaultSelectionRule);
    
    generate();
}