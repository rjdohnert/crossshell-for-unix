#include "color.hpp"
#include "theme_catalog.hpp"
#include "ui_theme.hpp"

const std::vector<UiTheme>& AvailableThemes() {
    static const std::vector<UiTheme> themes = {
        { "blue",   Color::Rgb(28, 84, 190),  Color::Rgb(245, 185, 40), Color::Rgb(255, 255, 255), Color::Rgb(80, 120, 190),  Color::Rgb(100, 190, 255), Color::Rgb(190, 45, 45),  Color::Rgb(20, 50, 140),   Color::Rgb(255, 255, 255) },
        { "black",  Color::Rgb(30, 30, 30),   Color::Rgb(210, 210, 210), Color::Rgb(245, 245, 245), Color::Rgb(85, 85, 85),    Color::Rgb(230, 230, 230), Color::Rgb(120, 30, 30),  Color::Rgb(18, 18, 18),    Color::Rgb(245, 245, 245) },
        { "orange", Color::Rgb(220, 105, 22), Color::Rgb(70, 45, 25),   Color::Rgb(20, 20, 20),    Color::Rgb(170, 95, 45),   Color::Rgb(255, 190, 90),  Color::Rgb(110, 45, 20),  Color::Rgb(150, 65, 20),   Color::Rgb(255, 250, 235) },
        { "silver", Color::Rgb(188, 194, 204), Color::Rgb(70, 80, 92),   Color::Rgb(15, 20, 28),    Color::Rgb(135, 145, 158), Color::Rgb(250, 250, 255), Color::Rgb(95, 65, 95),   Color::Rgb(78, 86, 96),    Color::Rgb(245, 248, 252) },
        { "pink",   Color::Rgb(225, 88, 156), Color::Rgb(70, 30, 55),   Color::Rgb(30, 15, 25),    Color::Rgb(190, 85, 135),  Color::Rgb(255, 185, 220), Color::Rgb(125, 35, 85),  Color::Rgb(145, 45, 100),  Color::Rgb(255, 245, 250) },
        { "yellow", Color::Rgb(236, 205, 55), Color::Rgb(60, 55, 20),   Color::Rgb(20, 20, 10),    Color::Rgb(175, 155, 55),  Color::Rgb(255, 245, 135), Color::Rgb(120, 95, 15),  Color::Rgb(135, 115, 30),  Color::Rgb(255, 250, 215) }
    };
    return themes;
}
