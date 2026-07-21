#ifndef SOCIALSHARING_H
#define SOCIALSHARING_H

#include <QObject>
#include <QString>
#include <QStringList> // Added for QStringList support

class SocialSharing : public QObject
{
    Q_OBJECT

public:
    explicit SocialSharing(QObject* parent = nullptr);
    ~SocialSharing();

    void shareSong(const QString& filepath, const QString& frequency);
    void sharePlaylist(const QString& playlistName);
    void shareAudioClip(const QString& filepath, int startSeconds, int duration);
    
    void exportToM3U(const QString& playlistName, const QString& filepath);
    void importFromM3U(const QString& filepath);
    
    void generateSocialMediaPost(const QString& songTitle, const QString& artist, int frequency);
    void shareViaEmail(const QString& filepath, const QString& recipient);
    void shareViaBluetooth(const QString& filepath);
    
signals:
    void shareProgress(int percent);
    void shareComplete(const QString& message);
    void shareFailed(const QString& error);

private:
    bool validateFile(const QString& filepath);
    QString createTempFile(const QString& filepath);
    void cleanupTempFiles();
    
    QStringList m_tempFiles; // Fixed: Changed from std::vector<QString> to match implementation
};

#endif
