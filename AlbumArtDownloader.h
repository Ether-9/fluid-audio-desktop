#ifndef ALBUM_ART_DOWNLOADER_H
#define ALBUM_ART_DOWNLOADER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QCache>
#include <QWidget>
#include <QThread>
#include <QPointer>

// Internal asynchronous worker decoupled structurally from main-thread database calls
class AlbumArtWorker;

class AlbumArtDownloader : public QObject {
    Q_OBJECT

public:
    explicit AlbumArtDownloader(QObject *parent = nullptr);
    ~AlbumArtDownloader();

    // Preserved public entry points (Never blocks the user interface thread execution paths)
    void downloadImage(const QString &album, const QString &artist, const QString &targetPath);
    void clearCache();

signals:
    void downloadFinished(const QString &album, const QString &artist, const QImage &image, const QString &savedPath);
    void downloadFailed(const QString &album, const QString &artist, const QString &reason);

private slots:
    void handleWorkerResult(const QString &album, const QString &artist, const QImage &image, const QString &savedPath);
    void handleWorkerFailure(const QString &album, const QString &artist, const QString &reason);

private:
    // Core memory limit defense: QCache demands cost limits to prevent unbounded heap memory leakage
    QCache<QString, QImage> m_memoryCache; 
    
    QThread m_workerThread;
    AlbumArtWorker *m_worker;
    QString m_dbDirectory;
};

#endif // ALBUM_ART_DOWNLOADER_H
