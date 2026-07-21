#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QWidget> // Change from <QObject> to <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QColor>

struct Theme {
    QString name;
    bool isDark;
    
    // Qt QColor elements used by ThemeManager's stylesheet compiler
    QColor windowBg;
    QColor panelBg;
    QColor cardBg;
    QColor accentColor;
    QColor accentHover;
    QColor textPrimary;
    QColor textSecondary;
    QColor borderColor;
    QColor waveformForeground;
    QColor waveformBackground;

    // String compatibility fields used by MainWindow
    QString bgPrimary;
    QString bgSecondary;
    QString bgTertiary;
    QString bgHover;
    QString border;
    QString accent;
};

class ThemeManager : public QWidget { // Change public QObject to public QWidget
    Q_OBJECT
public:
    explicit ThemeManager(QWidget* parent = nullptr); // Change QObject* to QWidget*
    ~ThemeManager();

    static ThemeManager& instance();

    QStringList getThemeNames() const; 
    Theme getTheme(const QString& name) const;
    Theme getCurrentTheme() const;
    void applyTheme(const QString& name);
    void setCustomAccent(const QString& hexColor);

signals:
    void themeChanged(const Theme& theme);

private:
    void initThemes();
    QString generateStyleSheet(const Theme& theme) const;

    QMap<QString, Theme> m_themes;
    QString m_currentThemeName;
    QString m_customAccent;
};

#endif // THEME_MANAGER_H