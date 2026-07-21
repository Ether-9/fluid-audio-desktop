#include "ThemeManager.h"
#include <QApplication>
#include <QWidget>

ThemeManager::ThemeManager(QWidget* parent)
    : QWidget(parent)
{
    initThemes();
    // Default theme matching your original project defaults
    m_currentThemeName = "Fluent Dark";
}

ThemeManager::~ThemeManager() {}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

void ThemeManager::initThemes()
{
    // Helper lambda to easily construct themes with both QColor and CSS compatibility fields
    auto createTheme = [this](
        QString name, bool isDark,
        QColor winBg, QColor panBg, QColor cardBg, QColor acc, QColor accHov,
        QColor txt1, QColor txt2, QColor bor, QColor waveFg, QColor waveBg
    ) -> Theme {
        Theme t;
        t.name = name;
        t.isDark = isDark;
        t.windowBg = winBg;
        t.panelBg = panBg;
        t.cardBg = cardBg;
        t.accentColor = acc;
        t.accentHover = accHov;
        t.textPrimary = txt1;
        t.textSecondary = txt2;
        t.borderColor = bor;
        t.waveformForeground = waveFg;
        t.waveformBackground = waveBg;

        // Map to string compatibility equivalents for MainWindow
        t.bgPrimary = winBg.name();
        t.bgSecondary = panBg.name();
        t.bgTertiary = cardBg.name();
        t.bgHover = accHov.name();
        t.border = bor.name();
        t.accent = acc.name();
        return t;
    };

    // 1. Fluent Dark
    m_themes["Fluent Dark"] = createTheme(
        "Fluent Dark", true,
        QColor(32, 32, 32), QColor(26, 26, 26), QColor(45, 45, 45),
        QColor(0, 120, 212), QColor(0, 140, 240),
        QColor(255, 255, 255), QColor(170, 170, 170), QColor(55, 55, 55),
        QColor(0, 120, 212), QColor(20, 20, 20)
    );

    // 2. Nordic Frost
    m_themes["Nordic Frost"] = createTheme(
        "Nordic Frost", true,
        QColor(46, 52, 64), QColor(36, 41, 51), QColor(59, 66, 82),
        QColor(136, 192, 208), QColor(143, 188, 187),
        QColor(236, 239, 244), QColor(216, 222, 233), QColor(76, 86, 106),
        QColor(136, 192, 208), QColor(30, 34, 42)
    );

    // 3. Obsidian Neon
    m_themes["Obsidian Neon"] = createTheme(
        "Obsidian Neon", true,
        QColor(12, 12, 12), QColor(0, 0, 0), QColor(22, 22, 22),
        QColor(186, 85, 211), QColor(218, 112, 214),
        QColor(255, 255, 255), QColor(140, 140, 140), QColor(35, 35, 35),
        QColor(186, 85, 211), QColor(5, 5, 5)
    );

    // 4. Forest Moss
    m_themes["Forest Moss"] = createTheme(
        "Forest Moss", true,
        QColor(26, 36, 33), QColor(20, 28, 25), QColor(38, 51, 47),
        QColor(212, 163, 89), QColor(233, 196, 106),
        QColor(241, 243, 242), QColor(163, 177, 167), QColor(48, 64, 59),
        QColor(212, 163, 89), QColor(15, 20, 18)
    );

    // 5. Cyberpunk Red
    m_themes["Cyberpunk Red"] = createTheme(
        "Cyberpunk Red", true,
        QColor(21, 21, 24), QColor(15, 15, 18), QColor(31, 31, 36),
        QColor(255, 46, 99), QColor(255, 92, 131),
        QColor(245, 246, 250), QColor(143, 149, 163), QColor(45, 45, 54),
        QColor(255, 46, 99), QColor(10, 10, 12)
    );

    // 6. Fluent Light
    m_themes["Fluent Light"] = createTheme(
        "Fluent Light", false,
        QColor(243, 243, 243), QColor(255, 255, 255), QColor(235, 235, 235),
        QColor(0, 120, 212), QColor(0, 90, 158),
        QColor(32, 32, 32), QColor(100, 100, 100), QColor(218, 218, 218),
        QColor(0, 120, 212), QColor(248, 248, 248)
    );

    // 7. Sakura Blossom
    m_themes["Sakura Blossom"] = createTheme(
        "Sakura Blossom", false,
        QColor(255, 240, 245), QColor(255, 255, 255), QColor(255, 228, 225),
        QColor(255, 105, 180), QColor(255, 20, 147),
        QColor(74, 53, 60), QColor(143, 115, 125), QColor(245, 200, 210),
        QColor(255, 105, 180), QColor(255, 250, 250)
    );

    // 8. Sandstone Sage
    m_themes["Sandstone Sage"] = createTheme(
        "Sandstone Sage", false,
        QColor(245, 242, 235), QColor(252, 251, 249), QColor(236, 230, 218),
        QColor(118, 146, 121), QColor(92, 116, 95),
        QColor(44, 49, 44), QColor(110, 118, 110), QColor(220, 212, 198),
        QColor(118, 146, 121), QColor(249, 247, 243)
    );

    // 9. Ocean Breeze
    m_themes["Ocean Breeze"] = createTheme(
        "Ocean Breeze", false,
        QColor(240, 248, 255), QColor(255, 255, 255), QColor(224, 238, 238),
        QColor(0, 168, 181), QColor(0, 134, 139),
        QColor(24, 43, 49), QColor(91, 114, 121), QColor(206, 222, 224),
        QColor(0, 168, 181), QColor(245, 251, 255)
    );

    // 10. Orchard Sunset
    m_themes["Orchard Sunset"] = createTheme(
        "Orchard Sunset", false,
        QColor(253, 246, 230), QColor(255, 253, 247), QColor(248, 234, 212),
        QColor(230, 92, 58), QColor(194, 69, 39),
        QColor(58, 43, 38), QColor(128, 107, 99), QColor(236, 215, 185),
        QColor(230, 92, 58), QColor(254, 251, 243)
    );

    // Alias "Day" and "Glassy Gradient" to prevent errors in MainWindow.cpp
    m_themes["Day"] = m_themes["Fluent Light"];
    m_themes["Glassy Gradient"] = m_themes["Obsidian Neon"];
}

QStringList ThemeManager::getThemeNames() const
{
    return m_themes.keys();
}

Theme ThemeManager::getTheme(const QString& name) const
{
    Theme t = m_themes.value(name, m_themes["Fluent Dark"]);
    if (!m_customAccent.isEmpty()) {
        t.accent = m_customAccent;
        t.accentColor = QColor(m_customAccent);
    }
    return t;
}

Theme ThemeManager::getCurrentTheme() const
{
    return getTheme(m_currentThemeName);
}

void ThemeManager::applyTheme(const QString& name)
{
    if (!m_themes.contains(name)) return;
    m_currentThemeName = name;
    Theme theme = getTheme(name);
    
    qApp->setStyleSheet(generateStyleSheet(theme));
    emit themeChanged(theme);
}

void ThemeManager::setCustomAccent(const QString& hexColor) {
    m_customAccent = hexColor;
}

QString ThemeManager::generateStyleSheet(const Theme& theme) const
{
    return QString(
        "/* Main Application Window */\n"
        "QMainWindow { background-color: %1; color: %2; }\n\n"
        
        "/* Panels */\n"
        "QFrame#sidebar, QFrame#playerControls {\n"
        "  background-color: %3;\n"
        "  border: 1px solid %4;\n"
        "  border-radius: 8px;\n"
        "}\n\n"
        
        "/* Labels */\n"
        "QLabel { color: %5; font-family: 'Segoe UI', system-ui; }\n"
        "QLabel#secondary { color: %6; }\n\n"
        
        "/* ListViews and Cards */\n"
        "QListView {\n"
        "  background-color: %3;\n"
        "  border: none;\n"
        "  border-radius: 8px;\n"
        "  outline: 0;\n"
        "}\n"
        "QListView::item {\n"
        "  background-color: %7;\n"
        "  color: %5;\n"
        "  border-radius: 6px;\n"
        "  padding: 8px;\n"
        "  margin: 2px 4px;\n"
        "}\n"
        "QListView::item:hover {\n"
        "  background-color: %8;\n"
        "}\n"
        "QListView::item:selected {\n"
        "  background-color: %8;\n"
        "  color: %5;\n"
        "  border-left: 4px solid %8;\n"
        "}\n\n"
        
        "/* Buttons */\n"
        "QPushButton {\n"
        "  background-color: %7;\n"
        "  color: %5;\n"
        "  border: 1px solid %4;\n"
        "  border-radius: 6px;\n"
        "  padding: 6px 12px;\n"
        "  font-weight: bold;\n"
        "}\n"
        "QPushButton:hover {\n"
        "  background-color: %4;\n"
        "}\n"
        "QPushButton:pressed {\n"
        "  background-color: %3;\n"
        "}\n"
        "QPushButton#accentButton {\n"
        "  background-color: %8;\n"
        "  color: white;\n"
        "  border: none;\n"
        "}\n"
        "QPushButton#accentButton:hover { background-color: %9; }\n\n"
        
        "/* Sliders */\n"
        "QSlider::groove:horizontal {\n"
        "  height: 4px;\n"
        "  background: %4;\n"
        "  border-radius: 2px;\n"
        "}\n"
        "QSlider::sub-page:horizontal {\n"
        "  background: %8;\n"
        "  border-radius: 2px;\n"
        "}\n"
        "QSlider::handle:horizontal {\n"
        "  background: %8;\n"
        "  border: none;\n"
        "  width: 12px;\n"
        "  height: 12px;\n"
        "  margin: -4px 0;\n"
        "  border-radius: 6px;\n"
        "}\n"
        "QSlider::handle:horizontal:hover { background: %9; }\n"
    )
    .arg(theme.windowBg.name())
    .arg(theme.textPrimary.name())
    .arg(theme.panelBg.name())
    .arg(theme.borderColor.name())
    .arg(theme.textPrimary.name())
    .arg(theme.textSecondary.name())
    .arg(theme.cardBg.name())
    .arg(theme.accentColor.name())
    .arg(theme.accentHover.name());
}