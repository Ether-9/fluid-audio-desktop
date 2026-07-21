#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QList>
#include <QString>
#include <QPoint>
#include <array>

// Forward declarations of custom classes
class AudioEngine;
class MusicDatabase;
class ThemeManager;
class AdvancedEqualizer;
class AlbumArtWidget;
class WaveformWidget;
class SmartPlaylist;
class SocialSharing;
struct Theme;

// Forward declarations of Qt classes
class QListWidgetItem;
class QSlider;
class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QListWidget;
class QStackedWidget;
class QSystemTrayIcon;
class QTimer;
class QEvent;
class QResizeEvent;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onPlayPause();
    void onStop();
    void onNext();
    void onPrevious();
    void onShuffle();
    void onRepeat();
    void onVolumeChanged(int value);
    void onProgressClick(int position);
    void onOpenFolder();
    void onFrequencyChanged(int index);
    void onSongDoubleClicked(QListWidgetItem* item);
    void onQueueDoubleClicked(QListWidgetItem* item);
    void onPlaylistItemClicked(QListWidgetItem* item);
    void onPlaylistSelected(const QString &name);
    void onAddFolder();
    void onRefreshLibrary();
    void onCreatePlaylist();
    void onDeletePlaylist();
    void onAddToPlaylist();
    void onRemoveFromPlaylist();
    void onClearPlaylist();
    void onCrossfadeChanged(int value);
    void onThemeChanged(const QString& name);
    void onEqualizerPreset(const QString& preset);
    void onEqualizerChanged(int band, int value);
    
    void updateProgress();
    void onPlaybackFinished();
    void checkPlaybackStatus();
    void filterSongs(const QString& text);
    void handleSearch(const QString& text);
    void sortSongs(int index);
    void showSongContextMenu(const QPoint& pos);
    void showQueueContextMenu(const QPoint& pos);
    void saveShifted();
    void showShortcutsHelp();
    void removeFromQueue();

private:
    void setupUI();
    void setupHomeTab();
    void setupMusicTab();
    void setupQueueTab();
    void setupPlaylistTab();
    void setupSettingsTab();
    void setupHistoryTab();
    void setupPlayerBar();
    
    void setupSystemTray();
    void setupShortcuts();
    void setupModernSongListProperties();
    void adjustLayoutForSize(int width);
    void switchTab(int index);
    
    void updateSongList();
    void updatePlaylistList();
    void updateHistoryView();
    void setStatusMessage(const QString& msg);
    
    void loadSongMetadata(const QString &filepath);
    void updateHomeTab();
    void updateHistory();
    void applyTheme(const Theme &theme);
    QString formatTime(float seconds);

    void playSelectedSong();
    void playNextSong();
    void addToQueue();
    void clearQueue();

    // Custom Modular Widgets
    AdvancedEqualizer* advancedEqualizer;
    AlbumArtWidget* albumArtWidget;
    WaveformWidget* waveformWidget;
    ThemeManager* themeManager;
    SmartPlaylist* smartPlaylist;
    SocialSharing* socialSharing;

    // Core Audio Engines
    AudioEngine* audioEngine;
    MusicDatabase* database;
    
    // Core App State Variables
    bool isPlaying;
    bool isPaused;
    bool shuffleMode;
    int repeatMode;
    int currentSongIndex;

    // Timers
    QTimer* playbackTimer;

    // Main Dynamic Controls constructed programmatically
    QWidget* topBar;
    QWidget* sidebar;
    QStackedWidget* stackWidget;
    QWidget* playerBar;
    QSystemTrayIcon* trayIcon;

    QLabel* tabTitle;
    QComboBox* freqCombo;
    QListWidget* navList;
    QLabel* logoLabel; // Responsive logo member reference
    
    QLabel* nowPlayingArt;
    QLabel* nowPlayingTitle;
    QLabel* nowPlayingArtist;
    QLabel* nowPlayingFreq;

    // Tabs
    QWidget* homeTab;
    QListWidget* recentList;
    QListWidget* mostPlayedList;
    QListWidget* recentlyAddedList;

    QWidget* musicTab;
    QLineEdit* searchBox;
    QComboBox* sortCombo;
    QPushButton* addFolderBtn;
    QPushButton* refreshBtn;
    QLabel* songCountLabel;
    QListWidget* songList;

    QWidget* queueTab;
    QPushButton* addToQueueBtn;
    QPushButton* clearQueueBtn;
    QLabel* queueDurationLabel;
    QListWidget* queueList;

    QList<QString> queueSongs; 
    QString currentPlaylist;

    QWidget* playlistTab;
    QPushButton* newPlaylistBtn;
    QPushButton* deletePlaylistBtn;
    QListWidget* playlistList;
    QPushButton* addToPlaylistBtn;
    QPushButton* removeFromPlaylistBtn;
    QListWidget* playlistSongsList;

    QWidget* settingsTab;
    QSlider* crossfadeSlider;
    QLabel* crossfadeLabel;
    QComboBox* themeCombo;
    QPushButton* accentButton;
    QPushButton* eqToggleBtn;
    QComboBox* eqPresetCombo;
    QVector<QSlider*> eqSliders;
    QVector<QLabel*> eqLabels;
    QLabel* eqStatus;

    QWidget* historyTab;
    QPushButton* clearHistoryBtn;
    QListWidget* historyList;

    // Player Bar UI Elements
    QLabel* playerArt;
    QLabel* playerTitle;
    QLabel* playerArtist;
    QLabel* playerFreq;
    QLabel* timeLabel;
    QSlider* progressSlider;
    QLabel* durationLabel;
    
    QPushButton* shuffleBtn;
    QPushButton* prevBtn;
    QPushButton* playBtn;
    QPushButton* stopBtn;
    QPushButton* nextBtn;
    QPushButton* repeatBtn;

    QSlider* volumeSlider;

    const std::array<int, 10> FREQUENCIES = {174, 285, 396, 417, 432, 528, 639, 741, 852, 963};
};

#endif // MAINWINDOW_H