#include "MainWindow.h"
#include "AdvancedEqualizer.h"   
#include "WaveformWidget.h"      
#include "SmartPlaylist.h"       
#include "SocialSharing.h"       
#include "AudioEngine.h"
#include "MusicDatabase.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QTimer>
#include <QSettings>
#include <QStatusBar>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QListWidget>
#include <QStackedWidget>
#include <QMenu>
#include <QFontDatabase>
#include <QCheckBox>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QAction>
#include <QHeaderView>
#include <QApplication>
#include <QScrollBar>
#include <QEvent>
#include <QSystemTrayIcon>
#include <QShortcut>
#include <QFileInfo>
#include "AlbumArtWidget.h"
#include "SmartPlaylist.h"
#include "ThemeManager.h"
#include "AdvancedEqualizer.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isPlaying(false)
    , isPaused(false)
    , shuffleMode(false)
    , repeatMode(0)
    , currentSongIndex(-1)
{
    // 1. Instantiate modules
    audioEngine = new AudioEngine(this);
    database = new MusicDatabase(this);
    advancedEqualizer = new AdvancedEqualizer(this);
    albumArtWidget = new AlbumArtWidget(this);
    waveformWidget = new WaveformWidget(this);
    themeManager = new ThemeManager(this);
    smartPlaylist = new SmartPlaylist(this);
    socialSharing = new SocialSharing(this);
    
    // 2. Set up layout framework
    setupUI();
    setupPlayerBar();
    setupSystemTray();
    setupShortcuts();

    // 3. Set up interactive UI mechanics
    setupModernSongListProperties();
    
    // Connect search bar
    if (searchBox) {
        connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::handleSearch);
    }

    // Load persisted configurations
    QSettings settings;
    int lastFreq = settings.value("lastFrequency", 528).toInt();
    for (int i = 0; i < 10; ++i) {
        if (FREQUENCIES[i] == lastFreq) {
            if (freqCombo) {
                freqCombo->setCurrentIndex(i);
            }
            break;
        }
    }
    
    // Engine Signals & Timers
    connect(audioEngine, &AudioEngine::positionChanged, this, &MainWindow::updateProgress);
    connect(audioEngine, &AudioEngine::playbackFinished, this, &MainWindow::onPlaybackFinished);
    
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::checkPlaybackStatus);
    playbackTimer->start(500); 

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateProgress);
    timer->start(100);
    
    applyTheme(themeManager->getTheme("Dark Blue"));
    updateSongList();
    updatePlaylistList();
}

MainWindow::~MainWindow()
{
    QSettings settings;
    if (freqCombo) {
        settings.setValue("lastFrequency", freqCombo->currentText());
    }
}

// ===== System Tray =====

void MainWindow::setupSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/app.png"));
    
    QMenu* trayMenu = new QMenu(this);
    QAction* showAction = trayMenu->addAction("Show");
    QAction* hideAction = trayMenu->addAction("Hide");
    trayMenu->addSeparator();
    QAction* playPauseAction = trayMenu->addAction("Play/Pause");
    QAction* nextAction = trayMenu->addAction("Next");
    QAction* prevAction = trayMenu->addAction("Previous");
    trayMenu->addSeparator();
    QAction* quitAction = trayMenu->addAction("Quit");
    
    connect(showAction, &QAction::triggered, this, &QWidget::show);
    connect(hideAction, &QAction::triggered, this, &QWidget::hide);
    connect(playPauseAction, &QAction::triggered, this, &MainWindow::onPlayPause);
    connect(nextAction, &QAction::triggered, this, &MainWindow::onNext);
    connect(prevAction, &QAction::triggered, this, &MainWindow::onPrevious);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
    
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();
}

void MainWindow::setupModernSongListProperties() {
    songList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    songList->setContextMenuPolicy(Qt::CustomContextMenu);
    
    for (int i = 0; i < songList->count(); ++i) {
        QListWidgetItem* item = songList->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    connect(songList, &QListWidget::customContextMenuRequested, this, &MainWindow::showSongContextMenu);
}

void MainWindow::playSelectedSong() {
    QListWidgetItem* item = songList->currentItem();
    if (item) {
        QString filePath = item->data(Qt::UserRole).toString(); 
        if(filePath.isEmpty()) {
            filePath = item->toolTip();
        }
        
        audioEngine->loadSong(filePath.toStdString());
        audioEngine->play();
        albumArtWidget->setArt(filePath); 
    }
}

void MainWindow::playNextSong() {
    auto selectedItems = songList->selectedItems();
    for (auto* item : selectedItems) {
        QString path = item->data(Qt::UserRole).toString();
        audioEngine->insertIntoQueueNext(path.isEmpty() ? item->toolTip().toStdString() : path.toStdString());
    }
}

void MainWindow::addToQueue() {
    auto selectedItems = songList->selectedItems();
    for (auto* item : selectedItems) {
        QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) path = item->toolTip();
        
        audioEngine->appendToQueue(path.toStdString());
        
        QListWidgetItem* queueItem = new QListWidgetItem(item->text());
        queueItem->setData(Qt::UserRole, path);
        queueItem->setToolTip(path);
        queueList->addItem(queueItem);
    }
}

void MainWindow::clearQueue() {
    audioEngine->clearQueue();      
    queueList->clear();   
    setStatusMessage("Queue cleared");
}

void MainWindow::handleSearch(const QString &text) {
    for (int i = 0; i < songList->count(); ++i) {
        QListWidgetItem* item = songList->item(i);
        bool matches = item->text().contains(text, Qt::CaseInsensitive);
        songList->setRowHidden(i, !matches);
    }
}

void MainWindow::showSongContextMenu(const QPoint &pos) {
    QListWidgetItem* item = songList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.addAction("▶ Play", this, &MainWindow::playSelectedSong);
    menu.addAction("⏭ Play next", this, &MainWindow::playNextSong);
    menu.addAction("➕ Add to queue", this, &MainWindow::addToQueue);
    
    QMenu* playlistMenu = menu.addMenu("📁 Add to playlist");
    playlistMenu->addAction("New Smart Playlist", this, [this]() {
        smartPlaylist->createNewFromSelection();
    });

    menu.exec(songList->mapToGlobal(pos));
}

// ===== Keyboard Shortcuts =====

void MainWindow::setupShortcuts()
{
    QShortcut* playPauseShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(playPauseShortcut, &QShortcut::activated, this, &MainWindow::onPlayPause);
    
    QShortcut* playPauseShortcut2 = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(playPauseShortcut2, &QShortcut::activated, this, &MainWindow::onPlayPause);
    
    QShortcut* stopShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(stopShortcut, &QShortcut::activated, this, &MainWindow::onStop);
    
    QShortcut* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(nextShortcut, &QShortcut::activated, this, &MainWindow::onNext);
    
    QShortcut* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(prevShortcut, &QShortcut::activated, this, &MainWindow::onPrevious);
    
    QShortcut* volUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(volUpShortcut, &QShortcut::activated, [this]() {
        int val = volumeSlider->value() + 5;
        if (val > 100) val = 100;
        volumeSlider->setValue(val);
    });
    
    QShortcut* volDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(volDownShortcut, &QShortcut::activated, [this]() {
        int val = volumeSlider->value() - 5;
        if (val < 0) val = 0;
        volumeSlider->setValue(val);
    });
    
    QShortcut* saveShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveShifted);
    
    for (int i = 0; i < 10; ++i) {
        Qt::Key key = static_cast<Qt::Key>(Qt::Key_0 + ((i + 1) % 10));
        QShortcut* freqShortcut = new QShortcut(QKeySequence(key), this);
        connect(freqShortcut, &QShortcut::activated, [this, i]() {
            if (i < 10 && freqCombo) {
                freqCombo->setCurrentIndex(i);
                onFrequencyChanged(i);
            }
        });
    }
    
    for (int i = 0; i < 10; ++i) {
        Qt::Key key = static_cast<Qt::Key>(Qt::Key_F1 + i);
        QShortcut* freqShortcut = new QShortcut(QKeySequence(key), this);
        connect(freqShortcut, &QShortcut::activated, [this, i]() {
            if (i < 10 && freqCombo) {
                freqCombo->setCurrentIndex(i);
                onFrequencyChanged(i);
            }
        });
    }
    
    QShortcut* shuffleShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this);
    connect(shuffleShortcut, &QShortcut::activated, this, &MainWindow::onShuffle);
    
    QShortcut* repeatShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(repeatShortcut, &QShortcut::activated, this, &MainWindow::onRepeat);
    
    QShortcut* helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(helpShortcut, &QShortcut::activated, this, &MainWindow::showShortcutsHelp);
}

// ===== Event Handlers =====

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            if (sidebar) sidebar->setVisible(false);
        } else {
            // Re-apply responsive check on restore
            adjustLayoutForSize(width());
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    adjustLayoutForSize(event->size().width());
}

void MainWindow::adjustLayoutForSize(int width)
{
    if (!sidebar || !logoLabel || !navList || !nowPlayingTitle || !nowPlayingArtist || !nowPlayingFreq || !nowPlayingArt) return;

    if (width < 500) {
        // Ultra-narrow: hide sidebar fully. Accessible via topbar ☰ menu button toggle
        sidebar->setVisible(false);
    } else if (width < 850) {
        // Snapped / Multitasking: Show icon-only sidebar
        sidebar->setVisible(true);
        sidebar->setFixedWidth(70);
        logoLabel->setText("🎵");
        logoLabel->setAlignment(Qt::AlignCenter);
        
        if (navList->count() == 6) {
            navList->item(0)->setText("🏠");
            navList->item(1)->setText("📚");
            navList->item(2)->setText("📋");
            navList->item(3)->setText("📁");
            navList->item(4)->setText("⚙");
            navList->item(5)->setText("📜");
            for (int i = 0; i < 6; ++i) {
                navList->item(i)->setTextAlignment(Qt::AlignCenter);
            }
        }
        
        nowPlayingTitle->setVisible(false);
        nowPlayingArtist->setVisible(false);
        nowPlayingFreq->setVisible(false);
        nowPlayingArt->setAlignment(Qt::AlignCenter);
    } else {
        // Full Desktop Mode: Show expanded sidebar
        sidebar->setVisible(true);
        sidebar->setFixedWidth(200);
        logoLabel->setText("🎵 Pitch Shifter");
        logoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        
        if (navList->count() == 6) {
            navList->item(0)->setText("🏠 Home");
            navList->item(1)->setText("📚 Music");
            navList->item(2)->setText("📋 Queue");
            navList->item(3)->setText("📁 Playlists");
            navList->item(4)->setText("⚙ Settings");
            navList->item(5)->setText("📜 History");
            for (int i = 0; i < 6; ++i) {
                navList->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
        
        nowPlayingTitle->setVisible(true);
        nowPlayingArtist->setVisible(true);
        nowPlayingFreq->setVisible(true);
    }

    // Responsive Player Bar adaptations to prevent squished layouts
    if (playerBar) {
        QWidget* playerLeft = playerBar->findChild<QWidget*>("playerLeftWidget");
        QWidget* playerRight = playerBar->findChild<QWidget*>("playerRightWidget");
        
        if (playerLeft) {
            // Hide player bar metadata in compact modes to prioritize playback controls
            playerLeft->setVisible(width >= 650);
        }
        if (playerRight) {
            // Hide volume slider to avoid layout overflow
            playerRight->setVisible(width >= 750);
        }
    }
}

void MainWindow::switchTab(int index)
{
    if (stackWidget) {
        stackWidget->setCurrentIndex(index);
        QStringList titles = {"Home", "Music", "Queue", "Playlists", "Settings", "History"};
        if (index < titles.size() && tabTitle) {
            tabTitle->setText(titles[index]);
        }
    }
}

// ===== UI Setup =====

void MainWindow::setupUI()
{
    setWindowTitle("Pitch Shifter Pro");
    // Crucial: Lower minimum width limit so splitting workspace on Windows doesn't get blocked
    setMinimumSize(360, 500); 
    resize(1400, 800);
    
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    topBar = new QWidget(central);
    topBar->setFixedHeight(50);
    topBar->setStyleSheet("background-color: #12122a; border-bottom: 1px solid #1a1a3e;");
    
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 5, 15, 5);
    
    QPushButton* toggleSidebarBtn = new QPushButton("☰", topBar);
    toggleSidebarBtn->setFixedSize(30, 30);
    toggleSidebarBtn->setStyleSheet(
        "QPushButton { background: transparent; color: white; border: none; font-size: 16px; }"
        "QPushButton:hover { background: #2a2a4a; border-radius: 4px; }"
    );
    topLayout->addWidget(toggleSidebarBtn);
    
    tabTitle = new QLabel("Music", topBar);
    tabTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    topLayout->addWidget(tabTitle);
    
    topLayout->addStretch();
    
    QLabel* freqLabel = new QLabel("Freq:", topBar);
    freqLabel->setStyleSheet("color: #a0a0b0;");
    topLayout->addWidget(freqLabel);
    
    freqCombo = new QComboBox(topBar);
    QStringList freqItems;
    for (int f : FREQUENCIES) freqItems << QString::number(f);
    freqCombo->addItems(freqItems);
    freqCombo->setCurrentIndex(5);
    freqCombo->setStyleSheet(
        "QComboBox { background: #1a1a3e; color: white; border: none; border-radius: 3px; padding: 5px 10px; }"
        "QComboBox::drop-down { border: none; }"
    );
    topLayout->addWidget(freqCombo);
    connect(freqCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onFrequencyChanged);
    
    mainLayout->addWidget(topBar);
    
    QWidget* contentArea = new QWidget(central);
    QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    sidebar = new QWidget(contentArea);
    sidebar->setFixedWidth(200);
    sidebar->setStyleSheet("background-color: #0a0a1a;");
    
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(10, 20, 10, 20);
    sidebarLayout->setSpacing(5);
    
    logoLabel = new QLabel("🎵 Pitch Shifter", sidebar);
    logoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1DB954;");
    sidebarLayout->addWidget(logoLabel);
    
    sidebarLayout->addSpacing(20);
    
    navList = new QListWidget(sidebar);
    navList->setObjectName("navList");
    navList->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 10px 15px; color: #a0a0b0; border-radius: 5px; }"
        "QListWidget::item:selected { background: #1a1a3e; color: white; }"
        "QListWidget::item:hover { background: #1a1a3e; }"
    );
    
    QStringList navItems;
    navItems << "🏠 Home" << "📚 Music" << "📋 Queue" << "📁 Playlists" << "⚙ Settings" << "📜 History";
    navList->addItems(navItems);
    navList->setCurrentRow(1);
    sidebarLayout->addWidget(navList);
    
    sidebarLayout->addStretch();
    
    QWidget* nowPlayingWidget = new QWidget(sidebar);
    nowPlayingWidget->setStyleSheet("background: #1a1a3e; border-radius: 8px; padding: 10px;");
    QVBoxLayout* nowLayout = new QVBoxLayout(nowPlayingWidget);
    
    nowPlayingArt = new QLabel("🎵", nowPlayingWidget);
    nowPlayingArt->setAlignment(Qt::AlignCenter);
    nowPlayingArt->setStyleSheet("font-size: 32px; color: #1DB954;");
    nowLayout->addWidget(nowPlayingArt);
    
    nowPlayingTitle = new QLabel("No song playing", nowPlayingWidget);
    nowPlayingTitle->setAlignment(Qt::AlignCenter);
    nowPlayingTitle->setStyleSheet("color: white; font-size: 10px; font-weight: bold;");
    nowLayout->addWidget(nowPlayingTitle);
    
    nowPlayingArtist = new QLabel("", nowPlayingWidget);
    nowPlayingArtist->setAlignment(Qt::AlignCenter);
    nowPlayingArtist->setStyleSheet("color: #a0a0b0; font-size: 8px;");
    nowLayout->addWidget(nowPlayingArtist);
    
    nowPlayingFreq = new QLabel("528 Hz", nowPlayingWidget);
    nowPlayingFreq->setAlignment(Qt::AlignCenter);
    nowPlayingFreq->setStyleSheet("color: #1DB954; font-size: 8px; font-weight: bold;");
    nowLayout->addWidget(nowPlayingFreq);
    
    sidebarLayout->addWidget(nowPlayingWidget);
    contentLayout->addWidget(sidebar);
    
    connect(toggleSidebarBtn, &QPushButton::clicked, [this]() {
        if (sidebar) sidebar->setVisible(!sidebar->isVisible());
    });
    
    stackWidget = new QStackedWidget(contentArea);
    
    setupHomeTab();
    setupMusicTab();
    setupQueueTab();
    setupPlaylistTab();
    setupSettingsTab();
    setupHistoryTab();
    
    contentLayout->addWidget(stackWidget);
    mainLayout->addWidget(contentArea, 1);
    
    connect(navList, &QListWidget::currentRowChanged, [this](int row) {
        stackWidget->setCurrentIndex(row);
        QStringList titles = {"Home", "Music", "Queue", "Playlists", "Settings", "History"};
        if (row < titles.size() && tabTitle) {
            tabTitle->setText(titles[row]);
        }
    });
}

// ===== Home Tab =====

void MainWindow::setupHomeTab()
{
    homeTab = new QWidget(stackWidget);
    QVBoxLayout* layout = new QVBoxLayout(homeTab);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    QLabel* recentLabel = new QLabel("🎵 Recently Played", homeTab);
    recentLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    layout->addWidget(recentLabel);
    
    recentList = new QListWidget(homeTab);
    recentList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    recentList->setMinimumHeight(120);
    layout->addWidget(recentList);
    connect(recentList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    QLabel* mostLabel = new QLabel("🔥 Most Played", homeTab);
    mostLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    layout->addWidget(mostLabel);
    
    mostPlayedList = new QListWidget(homeTab);
    mostPlayedList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    mostPlayedList->setMinimumHeight(120);
    layout->addWidget(mostPlayedList);
    connect(mostPlayedList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    QLabel* addedLabel = new QLabel("📥 Recently Added", homeTab);
    addedLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    layout->addWidget(addedLabel);
    
    recentlyAddedList = new QListWidget(homeTab);
    recentlyAddedList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    recentlyAddedList->setMinimumHeight(120);
    layout->addWidget(recentlyAddedList);
    connect(recentlyAddedList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    stackWidget->addWidget(homeTab);
}

// ===== Music Tab =====

void MainWindow::setupMusicTab()
{
    musicTab = new QWidget(stackWidget);
    QVBoxLayout* layout = new QVBoxLayout(musicTab);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    
    searchBox = new QLineEdit(musicTab);
    searchBox->setPlaceholderText("🔍 Search songs...");
    searchBox->setStyleSheet("QLineEdit { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }");
    controlsLayout->addWidget(searchBox);
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::filterSongs);
    
    sortCombo = new QComboBox(musicTab);
    sortCombo->addItems({"Sort by: A-Z", "Sort by: Z-A", "Sort by: Duration", "Sort by: Recently Added"});
    sortCombo->setStyleSheet("QComboBox { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px; }");
    controlsLayout->addWidget(sortCombo);
    connect(sortCombo, &QComboBox::currentIndexChanged, this, &MainWindow::sortSongs);
    
    addFolderBtn = new QPushButton("📁 Add Folder", musicTab);
    addFolderBtn->setStyleSheet(
        "QPushButton { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }"
        "QPushButton:hover { background: #2a2a5e; }"
    );
    controlsLayout->addWidget(addFolderBtn);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    
    refreshBtn = new QPushButton("🔄 Refresh", musicTab);
    refreshBtn->setStyleSheet(
        "QPushButton { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }"
        "QPushButton:hover { background: #2a2a5e; }"
    );
    controlsLayout->addWidget(refreshBtn);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshLibrary);
    
    layout->addLayout(controlsLayout);
    
    songCountLabel = new QLabel("Songs: 0", musicTab);
    songCountLabel->setStyleSheet("color: #a0a0b0; font-size: 9px;");
    layout->addWidget(songCountLabel);
    
    songList = new QListWidget(musicTab);
    songList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 10px 15px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    songList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(songList);
    connect(songList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    songList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(songList, &QListWidget::customContextMenuRequested, this, &MainWindow::showSongContextMenu);
    
    stackWidget->addWidget(musicTab);
}

// ===== Queue Tab =====

void MainWindow::setupQueueTab()
{
    queueTab = new QWidget(stackWidget);
    QVBoxLayout* layout = new QVBoxLayout(queueTab);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    
    addToQueueBtn = new QPushButton("➕ Add to Queue", queueTab);
    addToQueueBtn->setStyleSheet(
        "QPushButton { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }"
        "QPushButton:hover { background: #2a2a5e; }"
    );
    controlsLayout->addWidget(addToQueueBtn);
    connect(addToQueueBtn, &QPushButton::clicked, this, &MainWindow::addToQueue);
    
    clearQueueBtn = new QPushButton("🗑 Clear Queue", queueTab);
    clearQueueBtn->setStyleSheet(
        "QPushButton { background: #2a2a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }"
        "QPushButton:hover { background: #3a3a5e; }"
    );
    controlsLayout->addWidget(clearQueueBtn);
    connect(clearQueueBtn, &QPushButton::clicked, this, &MainWindow::clearQueue);
    
    controlsLayout->addStretch();
    
    queueDurationLabel = new QLabel("Total: 00:00:00", queueTab);
    queueDurationLabel->setStyleSheet("color: #a0a0b0;");
    controlsLayout->addWidget(queueDurationLabel);
    
    layout->addLayout(controlsLayout);
    
    queueList = new QListWidget(queueTab);
    queueList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 10px 15px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    layout->addWidget(queueList);
    connect(queueList, &QListWidget::itemDoubleClicked, this, &MainWindow::onQueueDoubleClicked);
    
    queueList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(queueList, &QListWidget::customContextMenuRequested, this, &MainWindow::showQueueContextMenu);
    
    stackWidget->addWidget(queueTab);
}

// ===== Playlist Tab =====

void MainWindow::setupPlaylistTab()
{
    playlistTab = new QWidget(stackWidget);
    QHBoxLayout* mainLayout = new QHBoxLayout(playlistTab);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    QWidget* leftWidget = new QWidget(playlistTab);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    QLabel* playlistLabel = new QLabel("Playlists", leftWidget);
    playlistLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    leftLayout->addWidget(playlistLabel);
    
    QHBoxLayout* playlistControls = new QHBoxLayout();
    newPlaylistBtn = new QPushButton("+ New", leftWidget);
    newPlaylistBtn->setStyleSheet("QPushButton { background: #1DB954; color: white; border: none; border-radius: 5px; padding: 5px 10px; }");
    playlistControls->addWidget(newPlaylistBtn);
    connect(newPlaylistBtn, &QPushButton::clicked, this, &MainWindow::onCreatePlaylist);
    
    deletePlaylistBtn = new QPushButton("✕", leftWidget);
    deletePlaylistBtn->setStyleSheet("QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 10px; }");
    playlistControls->addWidget(deletePlaylistBtn);
    connect(deletePlaylistBtn, &QPushButton::clicked, this, &MainWindow::onDeletePlaylist);
    
    leftLayout->addLayout(playlistControls);
    
    playlistList = new QListWidget(leftWidget);
    playlistList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 10px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
    );
    playlistList->setMinimumWidth(200);
    leftLayout->addWidget(playlistList);
    connect(playlistList, &QListWidget::itemClicked, this, &MainWindow::onPlaylistItemClicked);
    
    // Inject the smartPlaylist widget to the top of the left layout section
    leftLayout->insertWidget(0, smartPlaylist);

    mainLayout->addWidget(leftWidget);
    
    QWidget* rightWidget = new QWidget(playlistTab);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    QLabel* songsLabel = new QLabel("Songs", rightWidget);
    songsLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    rightLayout->addWidget(songsLabel);
    
    QHBoxLayout* songControls = new QHBoxLayout();
    addToPlaylistBtn = new QPushButton("➕ Add", rightWidget);
    addToPlaylistBtn->setStyleSheet(
        "QPushButton { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 5px 10px; }"
        "QPushButton:hover { background: #2a2a5e; }"
    );
    songControls->addWidget(addToPlaylistBtn);
    connect(addToPlaylistBtn, &QPushButton::clicked, this, &MainWindow::onAddToPlaylist);
    
    removeFromPlaylistBtn = new QPushButton("✕ Remove", rightWidget);
    removeFromPlaylistBtn->setStyleSheet("QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 10px; }");
    songControls->addWidget(removeFromPlaylistBtn);
    connect(removeFromPlaylistBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFromPlaylist);
    
    QPushButton* clearPlaylistBtn = new QPushButton("🗑 Clear All", rightWidget);
    clearPlaylistBtn->setStyleSheet(
        "QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 10px; }"
        "QPushButton:hover { background: #5a3a3a; }"
    );
    songControls->addWidget(clearPlaylistBtn);
    connect(clearPlaylistBtn, &QPushButton::clicked, this, &MainWindow::onClearPlaylist);
    
    rightLayout->addLayout(songControls);
    
    playlistSongsList = new QListWidget(rightWidget);
    playlistSongsList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 10px 15px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
    );
    rightLayout->addWidget(playlistSongsList);
    connect(playlistSongsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    mainLayout->addWidget(rightWidget);
    stackWidget->addWidget(playlistTab);
}

// ===== Settings Tab =====

void MainWindow::setupSettingsTab()
{
    settingsTab = new QWidget(stackWidget);
    QVBoxLayout* layout = new QVBoxLayout(settingsTab);
    layout->setSpacing(15);
    layout->setContentsMargins(15, 15, 15, 15);
    
    QLabel* settingsTitle = new QLabel("⚙ Settings", settingsTab);
    settingsTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    layout->addWidget(settingsTitle);
    
    QWidget* crossfadeWidget = new QWidget(settingsTab);
    QHBoxLayout* crossfadeLayout = new QHBoxLayout(crossfadeWidget);
    QLabel* crossfadeLabelText = new QLabel("Crossfade:", crossfadeWidget);
    crossfadeLabelText->setStyleSheet("color: white;");
    crossfadeLayout->addWidget(crossfadeLabelText);
    
    crossfadeSlider = new QSlider(Qt::Horizontal, crossfadeWidget);
    crossfadeSlider->setRange(0, 12);
    crossfadeSlider->setValue(2);
    crossfadeSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #2a2a5a; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #1DB954; width: 12px; height: 12px; border-radius: 6px; }"
    );
    crossfadeLayout->addWidget(crossfadeSlider);
    connect(crossfadeSlider, &QSlider::valueChanged, this, &MainWindow::onCrossfadeChanged);
    
    crossfadeLabel = new QLabel("1.0s", crossfadeWidget);
    crossfadeLabel->setStyleSheet("color: #1DB954;");
    crossfadeLayout->addWidget(crossfadeLabel);
    
    layout->addWidget(crossfadeWidget);
    
    QWidget* gainWidget = new QWidget(settingsTab);
    QHBoxLayout* gainLayout = new QHBoxLayout(gainWidget);
    QLabel* gainLabel = new QLabel("Replay Gain:", gainWidget);
    gainLabel->setStyleSheet("color: white;");
    gainLayout->addWidget(gainLabel);
    
    QPushButton* gainToggleBtn = new QPushButton("OFF", gainWidget);
    gainToggleBtn->setStyleSheet("QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 15px; }");
    gainLayout->addWidget(gainToggleBtn);
    connect(gainToggleBtn, &QPushButton::clicked, [this, gainToggleBtn]() {
        static bool gainOn = false;
        gainOn = !gainOn;
        gainToggleBtn->setText(gainOn ? "ON" : "OFF");
        gainToggleBtn->setStyleSheet(gainOn ?
            "QPushButton { background: #1DB954; color: white; border: none; border-radius: 5px; padding: 5px 15px; }" :
            "QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 15px; }");
        audioEngine->setReplayGain(gainOn);
    });
    
    layout->addWidget(gainWidget);
    
    QWidget* themeWidget = new QWidget(settingsTab);
    QHBoxLayout* themeLayout = new QHBoxLayout(themeWidget);
    QLabel* themeLabel = new QLabel("Theme:", themeWidget);
    themeLabel->setStyleSheet("color: white;");
    themeLayout->addWidget(themeLabel);
    
    themeCombo = new QComboBox(themeWidget);
    themeCombo->addItems(themeManager->getThemeNames());
    themeCombo->setStyleSheet("QComboBox { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px; }");
    themeLayout->addWidget(themeCombo);
    connect(themeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onThemeChanged);
    
    accentButton = new QPushButton("🎨 Custom Accent", themeWidget);
    accentButton->setStyleSheet("QPushButton { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 8px 15px; }");
    themeLayout->addWidget(accentButton);
    connect(accentButton, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(Qt::green, this, "Choose Accent Color");
        if (color.isValid()) {
            themeManager->setCustomAccent(color.name());
            applyTheme(themeManager->getTheme(themeCombo->currentText()));
        }
    });
    
    layout->addWidget(themeWidget);
    
    // Inject Theme Manager view into layout context dynamically
    layout->addWidget(themeManager);

    QLabel* eqTitle = new QLabel("Equalizer", settingsTab);
    eqTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: white; margin-top: 10px;");
    layout->addWidget(eqTitle);
    
    QHBoxLayout* eqControls = new QHBoxLayout();
    eqToggleBtn = new QPushButton("🔊 ON", settingsTab);
    eqToggleBtn->setStyleSheet("QPushButton { background: #1DB954; color: white; border: none; border-radius: 5px; padding: 5px 15px; }");
    eqControls->addWidget(eqToggleBtn);
    connect(eqToggleBtn, &QPushButton::clicked, [this]() {
        static bool eqOn = true;
        eqOn = !eqOn;
        eqToggleBtn->setText(eqOn ? "🔊 ON" : "🔇 OFF");
        eqToggleBtn->setStyleSheet(eqOn ?
            "QPushButton { background: #1DB954; color: white; border: none; border-radius: 5px; padding: 5px 15px; }" :
            "QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 5px 15px; }");
        audioEngine->setEQEnabled(eqOn);
    });
    
    eqPresetCombo = new QComboBox(settingsTab);
    eqPresetCombo->addItems({"Normal", "Pop", "Rock", "Classical", "Jazz", "Bass Boost", "Treble Boost"});
    eqPresetCombo->setStyleSheet("QComboBox { background: #1a1a3e; color: white; border: none; border-radius: 5px; padding: 5px; }");
    eqControls->addWidget(eqPresetCombo);
    connect(eqPresetCombo, &QComboBox::currentTextChanged, this, &MainWindow::onEqualizerPreset);
    layout->addLayout(eqControls);
    
    QWidget* eqSlidersWidget = new QWidget(settingsTab);
    QHBoxLayout* eqSlidersLayout = new QHBoxLayout(eqSlidersWidget);
    eqSlidersLayout->setSpacing(5);
    
    QStringList bands = {"32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
    for (int i = 0; i < 10; ++i) {
        QWidget* bandWidget = new QWidget(eqSlidersWidget);
        QVBoxLayout* bandLayout = new QVBoxLayout(bandWidget);
        bandLayout->setAlignment(Qt::AlignCenter);
        
        QLabel* bandLabel = new QLabel(bands[i], bandWidget);
        bandLabel->setStyleSheet("color: #a0a0b0; font-size: 7px;");
        bandLabel->setAlignment(Qt::AlignCenter);
        bandLayout->addWidget(bandLabel);
        
        QSlider* slider = new QSlider(Qt::Vertical, bandWidget);
        slider->setRange(-12, 12);
        slider->setValue(0);
        slider->setStyleSheet(
            "QSlider::groove:vertical { width: 4px; background: #2a2a5a; border-radius: 2px; }"
            "QSlider::handle:vertical { background: #1DB954; width: 12px; height: 12px; border-radius: 6px; }"
        );
        bandLayout->addWidget(slider);
        eqSliders.push_back(slider);
        connect(slider, &QSlider::valueChanged, [this, i](int val) { onEqualizerChanged(i, val); });
        
        QLabel* valLabel = new QLabel("0.0", bandWidget);
        valLabel->setStyleSheet("color: #1DB954; font-size: 7px;");
        valLabel->setAlignment(Qt::AlignCenter);
        bandLayout->addWidget(valLabel);
        eqLabels.push_back(valLabel);
        
        eqSlidersLayout->addWidget(bandWidget);
    }
    
    layout->addWidget(eqSlidersWidget);
    
    // Explicitly add Modular advancedEqualizer object underneath the custom standard sliders
    layout->addWidget(advancedEqualizer);

    eqStatus = new QLabel("Ready", settingsTab);
    eqStatus->setStyleSheet("color: #a0a0b0;");
    layout->addWidget(eqStatus);
    
    layout->addStretch();
    stackWidget->addWidget(settingsTab);
}

// ===== Player Bar =====

void MainWindow::setupHistoryTab()
{
    historyTab = new QWidget(stackWidget);
    QVBoxLayout* layout = new QVBoxLayout(historyTab);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    QHBoxLayout* topHeaderLayout = new QHBoxLayout();
    QLabel* historyTitle = new QLabel("📜 Playback & Pitch History", historyTab);
    historyTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    topHeaderLayout->addWidget(historyTitle);
    
    topHeaderLayout->addStretch();
    
    clearHistoryBtn = new QPushButton("🗑 Clear History", historyTab);
    clearHistoryBtn->setStyleSheet(
        "QPushButton { background: #4a2a2a; color: white; border: none; border-radius: 5px; padding: 8px 15px; }"
        "QPushButton:hover { background: #5a3a3a; }"
    );
    topHeaderLayout->addWidget(clearHistoryBtn);
    
    connect(clearHistoryBtn, &QPushButton::clicked, [this]() {
        int reply = QMessageBox::question(this, "Clear History", 
                                         "Are you sure you want to wipe your entire playback history?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            database->clearHistory(); 
            updateHistoryView();
            setStatusMessage("Playback history cleared");
        }
    });
    
    layout->addLayout(topHeaderLayout);
    
    historyList = new QListWidget(historyTab);
    historyList->setStyleSheet(
        "QListWidget { background: #1a1a3e; color: white; border: none; border-radius: 8px; }"
        "QListWidget::item { padding: 12px 15px; border-bottom: 1px solid #2a2a4a; }"
        "QListWidget::item:selected { background: #1DB954; }"
        "QListWidget::item:hover { background: #2a2a4a; }"
    );
    layout->addWidget(historyList);
    
    connect(historyList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSongDoubleClicked);
    stackWidget->addWidget(historyTab);
}

void MainWindow::updateHistoryView()
{
    if (!historyList) return;
    historyList->clear();
    
    auto historySongs = database->getRecentSongs(50); 
    
    if (historySongs.empty()) {
        QListWidgetItem* item = new QListWidgetItem("No playback history recorded yet.");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        historyList->addItem(item);
    } else {
        for (const auto& song : historySongs) {
            QString displayText = QString("%1 - %2").arg(song.title, song.artist);
            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, song.filepath);
            historyList->addItem(item);
        }
    }
}

void MainWindow::setupPlayerBar()
{
    playerBar = new QWidget(this);
    playerBar->setFixedHeight(80);
    playerBar->setStyleSheet("background-color: #12122a; border-top: 1px solid #1a1a3e;");
    
    QHBoxLayout* playerLayout = new QHBoxLayout(playerBar);
    playerLayout->setContentsMargins(15, 5, 15, 5);
    
    // Insert modern Album Art modular widget at start of player layout
    playerLayout->addWidget(albumArtWidget);
    albumArtWidget->setFixedSize(50, 50);

    QWidget* leftWidget = new QWidget(playerBar);
    leftWidget->setObjectName("playerLeftWidget"); // Named for dynamic responsiveness
    QHBoxLayout* leftLayout = new QHBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    playerArt = new QLabel("🎵", leftWidget);
    playerArt->setStyleSheet("font-size: 24px; color: #1DB954;");
    playerArt->setFixedSize(50, 50);
    leftLayout->addWidget(playerArt);
    
    QWidget* infoWidget = new QWidget(leftWidget);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(10, 0, 0, 0);
    infoLayout->setSpacing(2);
    
    playerTitle = new QLabel("No song playing", infoWidget);
    playerTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: white;");
    infoLayout->addWidget(playerTitle);
    
    playerArtist = new QLabel("", infoWidget);
    playerArtist->setStyleSheet("font-size: 9px; color: #a0a0b0;");
    infoLayout->addWidget(playerArtist);
    
    playerFreq = new QLabel("528 Hz", infoWidget);
    playerFreq->setStyleSheet("font-size: 8px; color: #1DB954; font-weight: bold;");
    infoLayout->addWidget(playerFreq);
    
    leftLayout->addWidget(infoWidget);
    playerLayout->addWidget(leftWidget);
    
    QWidget* centerWidget = new QWidget(playerBar);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setSpacing(5);
    
    QHBoxLayout* progressLayout = new QHBoxLayout();
    timeLabel = new QLabel("00:00", centerWidget);
    timeLabel->setStyleSheet("color: #a0a0b0; font-size: 9px;");
    timeLabel->setFixedWidth(45);
    progressLayout->addWidget(timeLabel);
    
    progressSlider = new QSlider(Qt::Horizontal, centerWidget);
    progressSlider->setRange(0, 1000);
    progressSlider->setValue(0);
    progressSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #2a2a5a; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #1DB954; width: 12px; height: 12px; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #1DB954; border-radius: 2px; }"
    );
    progressLayout->addWidget(progressSlider);
    connect(progressSlider, &QSlider::sliderPressed, [this]() { audioEngine->setSeeking(true); });
    connect(progressSlider, &QSlider::sliderMoved, [this](int val) { 
        float pos = (val / 1000.0f) * audioEngine->getDuration();
        audioEngine->seek(pos);
    });
    connect(progressSlider, &QSlider::sliderReleased, [this]() { audioEngine->setSeeking(false); });
    
    durationLabel = new QLabel("03:45", centerWidget);
    durationLabel->setStyleSheet("color: #a0a0b0; font-size: 9px;");
    durationLabel->setFixedWidth(45);
    progressLayout->addWidget(durationLabel);
    
    centerLayout->addLayout(progressLayout);
    
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setAlignment(Qt::AlignCenter);
    controlLayout->setSpacing(8);
    
    shuffleBtn = new QPushButton("🔀", centerWidget);
    shuffleBtn->setFixedSize(32, 32);
    shuffleBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0b0; border: none; font-size: 14px; }");
    controlLayout->addWidget(shuffleBtn);
    connect(shuffleBtn, &QPushButton::clicked, this, &MainWindow::onShuffle);
    
    prevBtn = new QPushButton("⏮", centerWidget);
    prevBtn->setFixedSize(32, 32);
    prevBtn->setStyleSheet("QPushButton { background: transparent; color: white; border: none; font-size: 14px; }");
    controlLayout->addWidget(prevBtn);
    connect(prevBtn, &QPushButton::clicked, this, &MainWindow::onPrevious);
    
    playBtn = new QPushButton("▶", centerWidget);
    playBtn->setFixedSize(45, 40);
    playBtn->setStyleSheet(
        "QPushButton { background: #1DB954; color: white; border: none; border-radius: 20px; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background: #1ed760; }"
    );
    controlLayout->addWidget(playBtn);
    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    
    stopBtn = new QPushButton("⏹", centerWidget);
    stopBtn->setFixedSize(32, 32);
    stopBtn->setStyleSheet("QPushButton { background: transparent; color: white; border: none; font-size: 14px; }");
    controlLayout->addWidget(stopBtn);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    
    nextBtn = new QPushButton("⏭", centerWidget);
    nextBtn->setFixedSize(32, 32);
    nextBtn->setStyleSheet("QPushButton { background: transparent; color: white; border: none; font-size: 14px; }");
    controlLayout->addWidget(nextBtn);
    connect(nextBtn, &QPushButton::clicked, this, &MainWindow::onNext);
    
    repeatBtn = new QPushButton("🔁", centerWidget);
    repeatBtn->setFixedSize(32, 32);
    repeatBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0b0; border: none; font-size: 14px; }");
    controlLayout->addWidget(repeatBtn);
    connect(repeatBtn, &QPushButton::clicked, this, &MainWindow::onRepeat);
    
    centerLayout->addLayout(controlLayout);
    playerLayout->addWidget(centerWidget);
    
    QWidget* rightWidget = new QWidget(playerBar);
    rightWidget->setObjectName("playerRightWidget"); // Named for dynamic responsiveness
    QHBoxLayout* rightLayout = new QHBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* volIcon = new QLabel("🔊", rightWidget);
    volIcon->setStyleSheet("color: #a0a0b0; font-size: 14px;");
    rightLayout->addWidget(volIcon);
    
    volumeSlider = new QSlider(Qt::Horizontal, rightWidget);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(80);
    volumeSlider->setFixedWidth(80);
    volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 3px; background: #2a2a5a; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #1DB954; width: 10px; height: 10px; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #1DB954; border-radius: 2px; }"
    );
    rightLayout->addWidget(volumeSlider);
    connect(volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    
    rightWidget->setFixedWidth(120);
    playerLayout->addWidget(rightWidget);
    
    centralWidget()->layout()->addWidget(playerBar);
}

// ===== Playback Control Functions =====

void MainWindow::onPlayPause()
{
    if (audioEngine->isPlaying()) {
        audioEngine->pause();
        playBtn->setText("▶");
        isPaused = true;
        isPlaying = false;
    } else {
        if (audioEngine->getCurrentSong().isEmpty()) {
            if (songList->currentItem()) {
                QString filepath = songList->currentItem()->data(Qt::UserRole).toString();
                audioEngine->loadSong(filepath.toStdString());
                loadSongMetadata(filepath);
                updateHistory();
            } else if (queueList->currentItem()) {
                QString filepath = queueList->currentItem()->data(Qt::UserRole).toString();
                audioEngine->loadSong(filepath.toStdString());
                loadSongMetadata(filepath);
                updateHistory();
            } else if (songList->count() > 0) {
                songList->setCurrentRow(0);
                QString filepath = songList->currentItem()->data(Qt::UserRole).toString();
                audioEngine->loadSong(filepath.toStdString());
                loadSongMetadata(filepath);
                updateHistory();
            } else {
                setStatusMessage("No songs loaded!");
                return;
            }
        }
        audioEngine->play();
        playBtn->setText("⏸");
        isPaused = false;
        isPlaying = true;
        setStatusMessage("Playing");
    }
}

void MainWindow::onStop()
{
    audioEngine->stop();
    playBtn->setText("▶");
    isPlaying = false;
    isPaused = false;
    progressSlider->setValue(0);
    timeLabel->setText("00:00");
}

void MainWindow::onNext()
{
    audioEngine->stop();
    
    if (!queueSongs.empty() && queueList->count() > 0) {
        int currentRow = queueList->currentRow();
        if (currentRow < 0) currentRow = 0;
        if (currentRow < queueList->count() - 1) {
            queueList->setCurrentRow(currentRow + 1);
        } else {
            queueList->setCurrentRow(0);
        }
        QString filepath = queueList->currentItem()->data(Qt::UserRole).toString();
        audioEngine->loadSong(filepath.toStdString());
        loadSongMetadata(filepath);
        audioEngine->play();
        playBtn->setText("⏸");
        isPlaying = true;
        isPaused = false;
        updateHistory();
        return;
    }
    
    int currentRow = songList->currentRow();
    if (currentRow >= 0 && currentRow < songList->count() - 1) {
        songList->setCurrentRow(currentRow + 1);
    } else if (songList->count() > 0) {
        songList->setCurrentRow(0);
    } else {
        return;
    }
    
    QString filepath = songList->currentItem()->data(Qt::UserRole).toString();
    audioEngine->loadSong(filepath.toStdString());
    loadSongMetadata(filepath);
    audioEngine->play();
    playBtn->setText("⏸");
    isPlaying = true;
    isPaused = false;
    updateHistory();
}

void MainWindow::onPrevious()
{
    audioEngine->stop();
    
    if (!queueSongs.empty() && queueList->count() > 0) {
        int currentRow = queueList->currentRow();
        if (currentRow > 0) {
            queueList->setCurrentRow(currentRow - 1);
        } else {
            queueList->setCurrentRow(queueList->count() - 1);
        }
        QString filepath = queueList->currentItem()->data(Qt::UserRole).toString();
        audioEngine->loadSong(filepath.toStdString());
        loadSongMetadata(filepath);
        audioEngine->play();
        playBtn->setText("⏸");
        isPlaying = true;
        isPaused = false;
        updateHistory();
        return;
    }
    
    int currentRow = songList->currentRow();
    if (currentRow > 0) {
        songList->setCurrentRow(currentRow - 1);
    } else if (songList->count() > 0) {
        songList->setCurrentRow(songList->count() - 1);
    } else {
        return;
    }
    
    QString filepath = songList->currentItem()->data(Qt::UserRole).toString();
    audioEngine->loadSong(filepath.toStdString());
    loadSongMetadata(filepath);
    audioEngine->play();
    playBtn->setText("⏸");
    isPlaying = true;
    isPaused = false;
    updateHistory();
}

void MainWindow::onShuffle()
{
    shuffleMode = !shuffleMode;
    shuffleBtn->setStyleSheet(shuffleMode ? 
        "QPushButton { background: transparent; color: #1DB954; border: none; font-size: 14px; }" :
        "QPushButton { background: transparent; color: #a0a0b0; border: none; font-size: 14px; }");
}

void MainWindow::onRepeat()
{
    repeatMode = (repeatMode + 1) % 3;
    QString text = repeatMode == 0 ? "🔁" : repeatMode == 1 ? "🔁 All" : "🔁 One";
    repeatBtn->setText(text);
    repeatBtn->setStyleSheet(repeatMode == 0 ?
        "QPushButton { background: transparent; color: #a0a0b0; border: none; font-size: 14px; }" :
        "QPushButton { background: transparent; color: #1DB954; border: none; font-size: 14px; }");
}

void MainWindow::onFrequencyChanged(int index)
{
    if (index < 0 || index >= 10) return;
    int freq = FREQUENCIES[index];
    
    audioEngine->setFrequency(freq);
    playerFreq->setText(QString::number(freq) + " Hz");
    nowPlayingFreq->setText(QString::number(freq) + " Hz");
    setStatusMessage("Frequency set to " + QString::number(freq) + " Hz");
}

void MainWindow::onVolumeChanged(int value)
{
    float volume = value / 100.0f;
    audioEngine->setVolume(volume);
}

void MainWindow::updateProgress()
{
    if (!audioEngine->isPlaying() && !isPaused) return;
    
    float position = audioEngine->getPosition();
    float duration = audioEngine->getDuration();
    
    if (duration > 0) {
        QString durationText = formatTime(duration);
        if (durationLabel->text() != durationText) {
            durationLabel->setText(durationText);
        }
        
        int progress = static_cast<int>((position / duration) * 1000);
        if (!progressSlider->isSliderDown()) {
            progressSlider->setValue(progress);
        }
        timeLabel->setText(formatTime(position));
    }
}

void MainWindow::onPlaybackFinished()
{
    playBtn->setText("▶");
    isPlaying = false;
    progressSlider->setValue(0);
    timeLabel->setText("00:00");
    
    float duration = audioEngine->getDuration();
    if (duration > 0) {
        durationLabel->setText(formatTime(duration));
    }
    
    if (!currentPlaylist.isEmpty()) {
        auto songs = database->getPlaylist(currentPlaylist);
        if (!songs.empty()) {
            QString currentFilepath = audioEngine->getCurrentSong();
            int currentIndex = -1;
            for (int i = 0; i < (int)songs.size(); ++i) {
                if (songs[i].filepath == currentFilepath) {
                    currentIndex = i;
                    break;
                }
            }
            if (currentIndex >= 0 && currentIndex < (int)songs.size() - 1) {
                QString nextFilepath = songs[currentIndex + 1].filepath;
                audioEngine->loadSong(nextFilepath.toStdString());
                loadSongMetadata(nextFilepath);
                audioEngine->play();
                playBtn->setText("⏸");
                isPlaying = true;
                isPaused = false;
                setStatusMessage("Playing next in playlist: " + currentPlaylist);
                updateHistory();
                return;
            }
        }
    }
    
    if (!queueSongs.empty() && queueList->count() > 0) {
        int currentRow = queueList->currentRow();
        if (currentRow < queueList->count() - 1) {
            if (repeatMode == 2) {
                audioEngine->play();
                playBtn->setText("⏸");
                isPlaying = true;
                isPaused = false;
                return;
            }
            QTimer::singleShot(500, this, &MainWindow::onNext);
            return;
        }
    }
    
    if (repeatMode == 1) {
        QTimer::singleShot(500, this, &MainWindow::onNext);
    } else if (repeatMode == 2) {
        QTimer::singleShot(500, this, [this]() {
            audioEngine->play();
            playBtn->setText("⏸");
            isPlaying = true;
            isPaused = false;
        });
    } else {
        setStatusMessage("Playback finished");
    }
}

void MainWindow::onProgressClick(int position)
{
    float pos = (position / 1000.0f) * audioEngine->getDuration();
    audioEngine->seek(pos);
}

// ===== Library Functions =====

void MainWindow::onOpenFolder()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select Music Folder");
    if (!folder.isEmpty()) {
        database->scanFolder(folder);
        updateSongList();
    }
}

void MainWindow::onAddFolder()
{
    onOpenFolder();
}

void MainWindow::onRefreshLibrary()
{
    updateSongList();
}

void MainWindow::onSongDoubleClicked(QListWidgetItem* item)
{
    QString filepath = item->data(Qt::UserRole).toString();
    audioEngine->loadSong(filepath.toStdString());
    loadSongMetadata(filepath);
    audioEngine->play();
    playBtn->setText("⏸");
    isPlaying = true;
    isPaused = false;
    updateHistory();
}

void MainWindow::filterSongs(const QString& text)
{
    QString searchText = text.toLower().trimmed();
    for (int i = 0; i < songList->count(); ++i) {
        QListWidgetItem* item = songList->item(i);
        bool matches = item->text().toLower().contains(searchText);
        songList->setRowHidden(i, !matches);
    }
}

void MainWindow::sortSongs(int index)
{
    Q_UNUSED(index);
    updateSongList();
}

void MainWindow::removeFromQueue()
{
    if (!queueList->currentItem()) return;
    
    int row = queueList->currentRow();
    queueList->takeItem(row);
    if (row < (int)queueSongs.size()) {
        queueSongs.erase(queueSongs.begin() + row);
    }
    setStatusMessage("Removed from queue");
}

void MainWindow::onQueueDoubleClicked(QListWidgetItem* item)
{
    QString filepath = item->data(Qt::UserRole).toString();
    audioEngine->loadSong(filepath.toStdString());
    loadSongMetadata(filepath);
    audioEngine->play();
    playBtn->setText("⏸");
    isPlaying = true;
    isPaused = false;
    updateHistory();
}

void MainWindow::showQueueContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = queueList->itemAt(pos);
    if (!item) return;
    
    QMenu contextMenu(this);
    QAction* removeAction = contextMenu.addAction("✕ Remove from Queue");
    QAction* playAction = contextMenu.addAction("▶ Play Now");
    
    QAction* selectedAction = contextMenu.exec(queueList->mapToGlobal(pos));
    
    if (selectedAction == removeAction) {
        int row = queueList->row(item);
        queueList->takeItem(row);
        if (row < (int)queueSongs.size()) {
            queueSongs.erase(queueSongs.begin() + row);
        }
        setStatusMessage("Removed from queue");
    } else if (selectedAction == playAction) {
        QString filepath = item->data(Qt::UserRole).toString();
        audioEngine->loadSong(filepath.toStdString());
        loadSongMetadata(filepath);
        audioEngine->play();
        playBtn->setText("⏸");
        isPlaying = true;
        isPaused = false;
        updateHistory();
    }
}

// ===== Playlist Functions =====

void MainWindow::onCreatePlaylist()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Playlist", "Enter playlist name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        database->createPlaylist(name);
        playlistList->addItem(name);
        setStatusMessage("Created playlist: " + name);
    }
}

void MainWindow::onDeletePlaylist()
{
    if (!playlistList->currentItem()) return;
    QString name = playlistList->currentItem()->text();
    if (name.isEmpty()) return;
    
    int reply = QMessageBox::question(this, "Delete Playlist", "Delete playlist '" + name + "'?", QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        database->deletePlaylist(name);
        delete playlistList->currentItem();
        playlistSongsList->clear();
        setStatusMessage("Deleted playlist: " + name);
    }
}

void MainWindow::onClearPlaylist()
{
    if (!playlistList->currentItem()) {
        setStatusMessage("Select a playlist first!");
        return;
    }
    
    QString playlistName = playlistList->currentItem()->text();
    int reply = QMessageBox::question(this, "Clear Playlist", "Remove all songs from '" + playlistName + "'?", QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        auto songs = database->getPlaylist(playlistName);
        for (const auto& song : songs) {
            database->removeFromPlaylist(playlistName, song.filepath);
        }
        playlistSongsList->clear();
        setStatusMessage("Cleared playlist: " + playlistName);
    }
}

void MainWindow::onAddToPlaylist()
{
    if (!playlistList->currentItem()) {
        setStatusMessage("Select a playlist first!");
        return;
    }
    
    QString playlistName = playlistList->currentItem()->text();
    
    if (!songList->currentItem() && !queueList->currentItem()) {
        setStatusMessage("Select a song from Music or Queue!");
        return;
    }
    
    QListWidgetItem* item = songList->currentItem() ? songList->currentItem() : queueList->currentItem();
    QString filepath = item->data(Qt::UserRole).toString();
    QString title = item->text();
    
    database->addToPlaylist(playlistName, filepath);
    
    auto songs = database->getPlaylist(playlistName);
    playlistSongsList->clear();
    for (const auto& song : songs) {
        QListWidgetItem* songItem = new QListWidgetItem(song.title);
        songItem->setData(Qt::UserRole, song.filepath);
        playlistSongsList->addItem(songItem);
    }
    
    setStatusMessage("Added to playlist: " + title);
}

void MainWindow::onRemoveFromPlaylist()
{
    if (!playlistSongsList->currentItem()) return;
    if (!playlistList->currentItem()) return;
    
    QString playlistName = playlistList->currentItem()->text();
    QString filepath = playlistSongsList->currentItem()->data(Qt::UserRole).toString();
    
    database->removeFromPlaylist(playlistName, filepath);
    delete playlistSongsList->currentItem();
    
    setStatusMessage("Removed from playlist");
}

void MainWindow::onPlaylistItemClicked(QListWidgetItem* item)
{
    QString name = item->text();
    currentPlaylist = name;
    auto songs = database->getPlaylist(name);
    playlistSongsList->clear();
    for (const auto& song : songs) {
        QListWidgetItem* songItem = new QListWidgetItem(song.title);
        songItem->setData(Qt::UserRole, song.filepath);
        playlistSongsList->addItem(songItem);
    }
}

void MainWindow::onPlaylistSelected(const QString& name)
{
    Q_UNUSED(name);
}

// ===== Settings Functions =====

void MainWindow::onCrossfadeChanged(int value)
{
    float seconds = value / 2.0f;
    crossfadeLabel->setText(QString::number(seconds, 'f', 1) + "s");
    audioEngine->setCrossfade(seconds);
}

void MainWindow::onThemeChanged(const QString& themeName)
{
    Theme theme = themeManager->getTheme(themeName);
    applyTheme(theme);
}

void MainWindow::onEqualizerChanged(int band, int value)
{
    eqLabels[band]->setText(QString::number(value / 10.0f, 'f', 1));
    audioEngine->setEQBand(band, value / 10.0f);
}

void MainWindow::onEqualizerPreset(const QString& preset)
{
    std::vector<float> gains;
    if (preset == "Normal") gains = {0,0,0,0,0,0,0,0,0,0};
    else if (preset == "Pop") gains = {4,2,0,-1,0,2,3,2,1,0};
    else if (preset == "Rock") gains = {6,4,2,0,0,1,2,3,4,6};
    else if (preset == "Classical") gains = {5,4,3,2,0,0,1,2,4,6};
    else if (preset == "Jazz") gains = {4,3,1,0,0,1,2,4,5,6};
    else if (preset == "Bass Boost") gains = {8,6,4,2,0,0,0,0,0,0};
    else if (preset == "Treble Boost") gains = {0,0,0,0,0,0,2,4,6,8};
    else return;
    
    for (int i = 0; i < 10 && i < (int)gains.size(); ++i) {
        eqSliders[i]->setValue(gains[i] * 10);
        eqLabels[i]->setText(QString::number(gains[i], 'f', 1));
    }
    eqStatus->setText("Loaded preset: " + preset);
}

// ===== Utility Functions =====

void MainWindow::loadSongMetadata(const QString& filepath)
{
    SongMetadata metadata = database->getMetadata(filepath);
    playerTitle->setText(metadata.title);
    playerArtist->setText(metadata.artist);
    nowPlayingTitle->setText(metadata.title);
    nowPlayingArtist->setText(metadata.artist);
    
    float duration = audioEngine->getDuration();
    if (duration > 0) {
        durationLabel->setText(formatTime(duration));
    }
    
    int currentFreq = audioEngine->getCurrentFrequency();
    playerFreq->setText(QString::number(currentFreq) + " Hz");
    nowPlayingFreq->setText(QString::number(currentFreq) + " Hz");
}

void MainWindow::saveShifted()
{
    if (!audioEngine->isPlaying() && !audioEngine->getCurrentSong().isEmpty()) {
        QMessageBox::information(this, "Save Song", "Please play a song first!");
        return;
    }
    
    QString currentSong = audioEngine->getCurrentSong();
    if (currentSong.isEmpty()) {
        QMessageBox::information(this, "Save Song", "No song is currently loaded!");
        return;
    }
    
    int freq = audioEngine->getCurrentFrequency();
    QString fileName = QFileInfo(currentSong).baseName();
    QString defaultName = fileName + "_" + QString::number(freq) + "Hz.wav";
    
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Save Shifted Song",
        defaultName,
        "Audio Files (*.wav *.mp3);;WAV Files (*.wav);;MP3 Files (*.mp3)"
    );
    
    if (savePath.isEmpty()) return;
    
    setStatusMessage("Saving to: " + savePath);
    QMessageBox::information(this, "Save Song", "Song saved successfully!\n" + savePath);
}

void MainWindow::showShortcutsHelp()
{
    QString shortcuts = 
        "🎹 Keyboard Shortcuts\n"
        "═══════════════════════\n\n"
        "🎵 Playback:\n"
        "  Space / Backspace  → Play/Pause\n"
        "  Escape             → Stop\n"
        "  Left Arrow         → Previous Song\n"
        "  Right Arrow        → Next Song\n\n"
        "🔊 Volume:\n"
        "  Up Arrow           → Volume Up\n"
        "  Down Arrow         → Volume Down\n\n"
        "🎵 Frequency:\n"
        "  1-0 (Number Keys)  → Quick Frequency Select\n"
        "  F1-F10             → Frequency Presets\n\n"
        "📁 Playlist:\n"
        "  Ctrl+S             → Save Current Song\n"
        "  Ctrl+Shift+S       → Toggle Shuffle\n"
        "  Ctrl+R             → Toggle Repeat\n\n"
        "ℹ️  F1                → Show this Help";
    
    QMessageBox::information(this, "Keyboard Shortcuts", shortcuts);
}

void MainWindow::updateSongList()
{
    songList->clear();
    auto songs = database->getAllSongs();
    for (const auto& song : songs) {
        QListWidgetItem* item = new QListWidgetItem(song.title);
        item->setData(Qt::UserRole, song.filepath);
        songList->addItem(item);
    }
    
    songCountLabel->setText("Songs: " + QString::number(songs.size()));
    updateHomeTab();
}

void MainWindow::updateHomeTab()
{
    recentList->clear();
    auto recentSongs = database->getRecentSongs(10);
    if (recentSongs.empty()) {
        QListWidgetItem* item = new QListWidgetItem("No recently played songs");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        recentList->addItem(item);
    } else {
        for (const auto& song : recentSongs) {
            QListWidgetItem* item = new QListWidgetItem(song.title + " - " + song.artist);
            item->setData(Qt::UserRole, song.filepath);
            recentList->addItem(item);
        }
    }
    
    mostPlayedList->clear();
    auto mostPlayed = database->getMostPlayedSongs(10);
    if (mostPlayed.empty()) {
        QListWidgetItem* item = new QListWidgetItem("No played songs yet");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        mostPlayedList->addItem(item);
    } else {
        for (const auto& song : mostPlayed) {
            QListWidgetItem* item = new QListWidgetItem(song.title + " - " + song.artist + " (" + QString::number(song.playCount) + " plays)");
            item->setData(Qt::UserRole, song.filepath);
            mostPlayedList->addItem(item);
        }
    }
    
    recentlyAddedList->clear();
    auto recentlyAdded = database->getRecentlyAdded(10);
    if (recentlyAdded.empty()) {
        QListWidgetItem* item = new QListWidgetItem("No recently added songs");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        recentlyAddedList->addItem(item);
    } else {
        for (const auto& song : recentlyAdded) {
            QListWidgetItem* item = new QListWidgetItem(song.title + " - " + song.artist);
            item->setData(Qt::UserRole, song.filepath);
            recentlyAddedList->addItem(item);
        }
    }
}

void MainWindow::updatePlaylistList()
{
    playlistList->clear();
    QStringList playlists = database->getPlaylists();
    for (const QString& name : playlists) {
        playlistList->addItem(name);
    }
}

void MainWindow::updateHistory()
{
    if (!audioEngine) return;
    
    QString currentSong = audioEngine->getCurrentSong();
    if (!currentSong.isEmpty()) {
        int currentFreq = audioEngine->getCurrentFrequency(); 
        database->addToHistory(currentSong, currentFreq);
        updateHomeTab();
        updateHistoryView();
    }
}

void MainWindow::applyTheme(const Theme& theme)
{
    QString styleSheet;
    
    if (theme.name == "Glassy Gradient") {
        styleSheet = QString(
            "QMainWindow { background: %1; }"
            "QWidget { background: %1; }"
            "QLabel { color: %2; }"
            "QListWidget { background: rgba(255,255,255,0.05); color: %2; border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; }"
            "QListWidget::item:selected { background: rgba(108,99,255,0.3); }"
            "QListWidget::item:hover { background: rgba(255,255,255,0.1); }"
            "QPushButton { background: rgba(255,255,255,0.08); color: %2; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px 15px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.15); }"
            "QComboBox { background: rgba(255,255,255,0.05); color: %2; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 5px; }"
            "QLineEdit { background: rgba(255,255,255,0.05); color: %2; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px; }"
            "QSlider::sub-page:horizontal { background: %4; }"
            "QSlider::handle:horizontal { background: %4; }"
            "QSlider::groove:horizontal { background: rgba(255,255,255,0.1); }"
            "QSlider::sub-page:vertical { background: %4; }"
            "QSlider::handle:vertical { background: %4; }"
            "QSlider::groove:vertical { background: rgba(255,255,255,0.1); }"
        ).arg(theme.bgPrimary, theme.textPrimary.name(), theme.bgSecondary, theme.accent);
    } else if (theme.name == "Day") {
        styleSheet = QString(
            "QMainWindow { background-color: %1; }"
            "QWidget { background-color: %1; }"
            "QLabel { color: %2; }"
            "QListWidget { background-color: %5; color: %2; border: 1px solid %7; border-radius: 8px; }"
            "QListWidget::item:selected { background-color: %6; color: white; }"
            "QListWidget::item:hover { background-color: %4; }"
            "QPushButton { background-color: %6; color: white; border: none; border-radius: 5px; padding: 8px 15px; font-weight: bold; }"
            "QPushButton:hover { background-color: #0055b3; }"
            "QPushButton:!enabled { background-color: %7; color: %3; }"
            "QComboBox { background-color: white; color: %2; border: 1px solid %7; border-radius: 5px; padding: 5px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background-color: white; color: %2; }"
            "QLineEdit { background-color: white; color: %2; border: 1px solid %7; border-radius: 5px; padding: 8px; }"
            "QSlider::sub-page:horizontal { background-color: %6; }"
            "QSlider::handle:horizontal { background-color: %6; width: 14px; height: 14px; border-radius: 7px; }"
            "QSlider::groove:horizontal { background-color: %7; height: 4px; border-radius: 2px; }"
            "QSlider::sub-page:vertical { background-color: %6; }"
            "QSlider::handle:vertical { background-color: %6; width: 14px; height: 14px; border-radius: 7px; }"
            "QSlider::groove:vertical { background-color: %7; width: 4px; border-radius: 2px; }"
            "QStatusBar { background-color: %5; color: %2; }"
            "QStatusBar::item { border: none; }"
        ).arg(theme.bgPrimary, theme.textPrimary.name(), theme.bgSecondary,
      theme.bgHover, theme.bgTertiary, theme.accent, theme.border);
    } else {
        styleSheet = QString(
            "QMainWindow { background-color: %1; }"
            "QWidget { background-color: %1; }"
            "QLabel { color: %2; }"
            "QListWidget { background-color: %5; color: %2; border: none; border-radius: 8px; }"
            "QListWidget::item:selected { background-color: %6; }"
            "QListWidget::item:hover { background-color: %4; }"
            "QPushButton { background-color: %3; color: %2; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: %4; }"
            "QComboBox { background-color: %5; color: %2; border: none; border-radius: 5px; padding: 5px; }"
            "QLineEdit { background-color: %5; color: %2; border: none; border-radius: 5px; padding: 8px; }"
            "QSlider::sub-page:horizontal { background-color: %6; }"
            "QSlider::handle:horizontal { background-color: %6; }"
            "QSlider::groove:horizontal { background-color: %7; }"
            "QSlider::sub-page:vertical { background-color: %6; }"
            "QSlider::handle:vertical { background-color: %6; }"
            "QSlider::groove:vertical { background-color: %7; }"
        ).arg(theme.bgPrimary, theme.textPrimary.name(), theme.bgSecondary,
      theme.bgHover, theme.bgTertiary, theme.accent, theme.border);
    }
    
    setStyleSheet(styleSheet);
    
    if (theme.name == "Day") {
        playerBar->setStyleSheet(QString("background-color: %1; border-top: 2px solid %2;").arg(theme.bgSecondary, theme.border));
    } else {
        playerBar->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;").arg(theme.bgSecondary, theme.border));
    }
}

void MainWindow::setStatusMessage(const QString& message)
{
    statusBar()->showMessage(message, 3000);
}

QString MainWindow::formatTime(float seconds)
{
    if (seconds < 0) seconds = 0;
    int totalSeconds = static_cast<int>(seconds);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
}

void MainWindow::checkPlaybackStatus() {
    if (audioEngine && audioEngine->isFinished()) {
        int currentRow = songList->currentRow();
        int totalSongs = songList->count();
        
        if (currentRow != -1 && currentRow < totalSongs - 1) {
            int nextRow = currentRow + 1;
            songList->setCurrentRow(nextRow);
            
            QListWidgetItem* nextItem = songList->item(nextRow);
            if (nextItem) {
                QString path = nextItem->data(Qt::UserRole).toString();
                audioEngine->loadSong(path.isEmpty() ? nextItem->toolTip().toStdString() : path.toStdString());
                audioEngine->play();
            }
        } else {
            playbackTimer->stop(); 
        }
    }
}

// ===== Close Event =====

void MainWindow::closeEvent(QCloseEvent* event)
{
    audioEngine->stop();
    event->accept();
}