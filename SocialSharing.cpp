#include "SocialSharing.h"
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QFileInfo>
#include <QTextStream>
#include "MusicDatabase.h"

// If compiling for Android, ensure appropriate Qt Android headers are included here
#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#endif

SocialSharing::SocialSharing(QObject* parent)
    : QObject(parent)
{
}

SocialSharing::~SocialSharing()
{
    cleanupTempFiles();
}

void SocialSharing::shareSong(const QString& filepath, const QString& frequency)
{
    if (!validateFile(filepath)) {
        emit shareFailed("Invalid file");
        return;
    }
    
    QString tempFile = createTempFile(filepath);
    if (tempFile.isEmpty()) {
        emit shareFailed("Failed to create temp file");
        return;
    }
    
    // Copy with frequency info in filename
    QFileInfo info(filepath);
    QString newName = info.baseName() + "_" + frequency + "Hz." + info.suffix();
    QString destPath = QFileInfo(tempFile).absolutePath() + "/" + newName;
    QFile::rename(tempFile, destPath);
    m_tempFiles.append(destPath);
    
    // Share via system sharing
#ifdef Q_OS_ANDROID
    // Android sharing intent
    QAndroidJniObject::callStaticMethod<void>(
        "org/qtproject/example/ShareHelper",
        "shareFile",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QAndroidJniObject::fromString(destPath).object(),
        QAndroidJniObject::fromString("audio/*").object()
    );
#elif defined(Q_OS_WIN)
    // Windows - copy to clipboard or open with default app
    QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
#endif
    
    emit shareComplete("Song shared: " + newName);
}

void SocialSharing::sharePlaylist(const QString& playlistName)
{
    // Generate M3U playlist file
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        emit shareFailed("Failed to create playlist file");
        return;
    }
    
    // Write playlist header
    tempFile.write("#EXTM3U\n");
    
    // Write playlist contents
    // This would read from database
    MusicDatabase db;
    auto songs = db.getPlaylist(playlistName);
    
    for (const auto& song : songs) {
        QString line = "#EXTINF:" + QString::number(song.duration) + "," + song.artist + " - " + song.title + "\n";
        tempFile.write(line.toUtf8());
        tempFile.write(song.filepath.toUtf8());
        tempFile.write("\n");
    }
    
    tempFile.flush();
    QString filepath = tempFile.fileName();
    m_tempFiles.append(filepath);
    
    // Share the playlist file
    emit shareComplete("Playlist exported: " + playlistName + ".m3u");
    
    // Open file location
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filepath).absolutePath()));
}

void SocialSharing::shareAudioClip(const QString& filepath, int startSeconds, int duration)
{
    if (!validateFile(filepath)) {
        emit shareFailed("Invalid file");
        return;
    }
    
    // Create a clip from the original file
    // This would use ffmpeg or similar
    QString outputFile = QDir::tempPath() + "/clip_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".wav";
    
#ifdef Q_OS_WIN
    // Use ffmpeg if available
    QString ffmpeg = "ffmpeg";
    QStringList args;
    args << "-ss" << QString::number(startSeconds);
    args << "-t" << QString::number(duration);
    args << "-i" << filepath;
    args << "-acodec" << "copy";
    args << outputFile;
    
    QProcess process;
    process.start(ffmpeg, args);
    process.waitForFinished();
    
    if (process.exitCode() == 0) {
        m_tempFiles.append(outputFile);
        emit shareComplete("Audio clip created: " + outputFile);
        QDesktopServices::openUrl(QUrl::fromLocalFile(outputFile));
    } else {
        emit shareFailed("Failed to create audio clip");
    }
#else
    emit shareFailed("Audio clipping not supported on this platform");
#endif
}

void SocialSharing::exportToM3U(const QString& playlistName, const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit shareFailed("Failed to export playlist");
        return;
    }
    
    MusicDatabase db;
    auto songs = db.getPlaylist(playlistName);
    
    QTextStream out(&file);
    out << "#EXTM3U\n";
    
    for (const auto& song : songs) {
        out << "#EXTINF:" << song.duration << "," << song.artist << " - " << song.title << "\n";
        out << song.filepath << "\n";
    }
    
    file.close();
    emit shareComplete("Playlist exported to: " + filepath);
}

void SocialSharing::importFromM3U(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit shareFailed("Failed to import playlist");
        return;
    }
    
    MusicDatabase db;
    QStringList songs;
    QTextStream in(&file);
    QString line;
    bool readingSong = false;
    
    while (in.readLineInto(&line)) {
        if (line.startsWith("#EXTINF:")) {
            readingSong = true;
        } else if (readingSong && !line.isEmpty() && !line.startsWith("#")) {
            songs.append(line);
            readingSong = false;
        }
    }
    
    file.close();
    
    // Add songs to database
    for (const QString& songPath : songs) {
        if (QFile::exists(songPath)) {
            SongMetadata meta = db.getMetadata(songPath);
            db.addSong(meta);
        }
    }
    
    emit shareComplete("Imported " + QString::number(songs.size()) + " songs from playlist");
}

void SocialSharing::generateSocialMediaPost(const QString& songTitle, const QString& artist, int frequency)
{
    QString post = "🎵 Listening to \"" + songTitle + "\" by " + artist + " at " + QString::number(frequency) + "Hz! #PitchShifter #Music #FrequencyHealing";
    
    // Copy to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(post);
    
    emit shareComplete("Post copied to clipboard!");
}

void SocialSharing::shareViaEmail(const QString& filepath, const QString& recipient)
{
    if (!validateFile(filepath)) {
        emit shareFailed("Invalid file");
        return;
    }
    
    QFileInfo info(filepath);
    QUrl mailto;
    mailto.setScheme("mailto");
    mailto.setPath(recipient);
    mailto.setQuery("subject=Shared Audio File&body=Attached is " + info.fileName());
    
    QDesktopServices::openUrl(mailto);
    emit shareComplete("Email composer opened");
}

void SocialSharing::shareViaBluetooth(const QString& filepath)
{
#ifdef Q_OS_ANDROID
    QAndroidJniObject::callStaticMethod<void>(
        "org/qtproject/example/ShareHelper",
        "shareViaBluetooth",
        "(Ljava/lang/String;)V",
        QAndroidJniObject::fromString(filepath).object()
    );
#else
    QDesktopServices::openUrl(QUrl("bluetooth:" + filepath));
#endif
    emit shareComplete("Bluetooth sharing initiated");
}

bool SocialSharing::validateFile(const QString& filepath)
{
    QFileInfo info(filepath);
    return info.exists() && info.isFile() && info.size() > 0;
}

QString SocialSharing::createTempFile(const QString& filepath)
{
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        return QString();
    }
    
    QFile original(filepath);
    if (!original.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    tempFile.write(original.readAll());
    tempFile.flush();
    original.close();
    
    return tempFile.fileName();
}

void SocialSharing::cleanupTempFiles()
{
    for (const QString& file : m_tempFiles) {
        if (QFile::exists(file)) {
            QFile::remove(file);
        }
    }
    m_tempFiles.clear();
}
