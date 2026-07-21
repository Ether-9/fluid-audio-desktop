#include "AlbumArtDownloader.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>
#include <QDateTime>
#include <QObject>

// Internal execution engine tracking thread-confined database access layers safely
class AlbumArtWorker : public QObject {
    Q_OBJECT
public:
    AlbumArtWorker(const QString &dbPath) : m_dbPath(dbPath), m_nam(nullptr) {}
    ~AlbumArtWorker() {
        // Safe database extraction during background thread closure
        QSqlDatabase::removeDatabase("AlbumArtWorkerConnection");
    }

public slots:
    void initThreadContext() {
        m_nam = new QNetworkAccessManager(this);
        
        // Fix: Establish explicit execution boundary context mapping for QSqlDatabase inside the target thread loop
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "AlbumArtWorkerConnection");
        db.setDatabaseName(m_dbPath);
        if (!db.open()) {
            emit errorOccurred("", "", "Could not configure local persistent SQLite cache store.");
            return;
        }

        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS art_cache (key TEXT PRIMARY KEY, filepath TEXT, timestamp INTEGER)");
    }

    void processDownloadRequest(const QString &album, const QString &artist, const QString &targetPath) {
        QString lookupKey = QString("%1-%2").arg(artist.toLower().trimmed(), album.toLower().trimmed());
        
        QSqlDatabase db = QSqlDatabase::database("AlbumArtWorkerConnection");
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("SELECT filepath FROM art_cache WHERE key = :key");
            query.bindValue(":key", lookupKey);
            if (query.exec() && query.next()) {
                QString diskPath = query.value(0).toString();
                QImage diskImg(diskPath);
                if (!diskImg.isNull()) {
                    emit requestFinished(album, artist, diskImg, diskPath);
                    return;
                }
            }
        }

        // Fire outbound platform integrations (Using programmatic iTunes integration loop fallback)
        QString apiQueryUrl = QString("https://itunes.apple.com/search?term=%1+%2&entity=album&limit=1")
                              .arg(QUrl::toPercentEncoding(artist), QUrl::toPercentEncoding(album));
        
        QNetworkReply *reply = m_nam->get(QNetworkRequest(QUrl(apiQueryUrl)));
        connect(reply, &QNetworkReply::finished, this, [this, reply, album, artist, targetPath, lookupKey]() {
            // Clean dynamic structures instantly on scope close
            reply->deleteLater();
            
            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(album, artist, "Network error: " + reply->errorString());
                return;
            }

            QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
            QJsonArray results = json.object()["results"].toArray();
            if (results.isEmpty()) {
                emit errorOccurred(album, artist, "No artwork variations found on cloud repositories.");
                return;
            }

            // Target max high-resolution variants
            QString artUrlStr = results[0].toObject()["artworkUrl100"].toString().replace("100x100bb", "600x600bb");
            if (artUrlStr.isEmpty()) {
                emit errorOccurred(album, artist, "Artwork parsing node empty.");
                return;
            }

            QNetworkReply *imgReply = m_nam->get(QNetworkRequest(QUrl(artUrlStr)));
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, album, artist, targetPath, lookupKey]() {
                imgReply->deleteLater();
                if (imgReply->error() != QNetworkReply::NoError) {
                    emit errorOccurred(album, artist, "Failed downloading target asset payload.");
                    return;
                }

                QImage finalImg;
                if (!finalImg.loadFromData(imgReply->readAll())) {
                    emit errorOccurred(album, artist, "Corrupt asset payload data.");
                    return;
                }

                // Verify target execution folders are cleanly created
                QFileInfo info(targetPath);
                QDir().mkpath(info.absolutePath());

                if (!finalImg.save(targetPath, "JPEG", 90)) {
                    emit errorOccurred(album, artist, "Failed mapping payload image data to drive track paths.");
                    return;
                }

                // Append reference inside SQL indexes
                QSqlDatabase dbInner = QSqlDatabase::database("AlbumArtWorkerConnection");
                if (dbInner.isOpen()) {
                    QSqlQuery insQuery(dbInner);
                    insQuery.prepare("INSERT OR REPLACE INTO art_cache (key, filepath, timestamp) VALUES (:key, :path, :ts)");
                    insQuery.bindValue(":key", lookupKey);
                    insQuery.bindValue(":path", targetPath);
                    insQuery.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
                    insQuery.exec();
                }

                emit requestFinished(album, artist, finalImg, targetPath);
            });
        });
    }

    void clearAllStorage() {
        QSqlDatabase db = QSqlDatabase::database("AlbumArtWorkerConnection");
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.exec("SELECT filepath FROM art_cache");
            while (query.next()) {
                QFile::remove(query.value(0).toString());
            }
            query.exec("DELETE FROM art_cache");
        }
    }

signals:
    void requestFinished(const QString &album, const QString &artist, const QImage &image, const QString &savedPath);
    void errorOccurred(const QString &album, const QString &artist, const QString &reason);

private:
    QString m_dbPath;
    QNetworkAccessManager *m_nam;
};

// Main interface orchestrator
AlbumArtDownloader::AlbumArtDownloader(QObject *parent) : QObject(parent) {
    // Explicit Memory Boundary Isolation Setup: Set maximum cost tracking bounds 
    m_memoryCache.setMaxCost(50 * 1024 * 1024); // Up to 50MB of raw images managed explicitly
    
    m_dbDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache";
    QDir().mkpath(m_dbDirectory);
    QString databaseFilePath = m_dbDirectory + "/art_cache.db";

    m_worker = new AlbumArtWorker(databaseFilePath);
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, m_worker, &AlbumArtWorker::initThreadContext);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    // Internal cross-thread bindings
    connect(m_worker, &AlbumArtWorker::requestFinished, this, &AlbumArtDownloader::handleWorkerResult);
    connect(m_worker, &AlbumArtWorker::errorOccurred, this, &AlbumArtDownloader::handleWorkerFailure);

    m_workerThread.start();
}

AlbumArtDownloader::~AlbumArtDownloader() {
    m_workerThread.quit();
    m_workerThread.wait();
}

void AlbumArtDownloader::downloadImage(const QString &album, const QString &artist, const QString &targetPath) {
    QString lookupKey = QString("%1-%2").arg(artist.toLower().trimmed(), album.toLower().trimmed());
    
    // Check Tier 1 Quick Cache instantly
    if (m_memoryCache.contains(lookupKey)) {
        emit downloadFinished(album, artist, *(m_memoryCache.object(lookupKey)), targetPath);
        return;
    }

    // Invoke processing across thread barriers safely using Qt Meta-Object translation mechanisms
    QMetaObject::invokeMethod(m_worker, "processDownloadRequest", Qt::QueuedConnection,
                              Q_ARG(QString, album), Q_ARG(QString, artist), Q_ARG(QString, targetPath));
}

void AlbumArtDownloader::clearCache() {
    m_memoryCache.clear();
    QMetaObject::invokeMethod(m_worker, "clearAllStorage", Qt::QueuedConnection);
}

void AlbumArtDownloader::handleWorkerResult(const QString &album, const QString &artist, const QImage &image, const QString &savedPath) {
    QString lookupKey = QString("%1-%2").arg(artist.toLower().trimmed(), album.toLower().trimmed());
    if (!m_memoryCache.contains(lookupKey)) {
        // Track resource cost metrics using image width * height * color byte footprint values
        int cost = image.width() * image.height() * 4;
        m_memoryCache.insert(lookupKey, new QImage(image), cost);
    }
    emit downloadFinished(album, artist, image, savedPath);
}

void AlbumArtDownloader::handleWorkerFailure(const QString &album, const QString &artist, const QString &reason) {
    emit downloadFailed(album, artist, reason);
}

#include "AlbumArtDownloader.moc"
