#include "cell.hpp"
#include "color.hpp"

Cell::Cell(wchar_t c, Color f , Color b ,
         bool bl , bool dm , bool it , bool ul , bool inv , uint8_t w )
        : ch(c), fg(f), bg(b), bold(bl), dim(dm), italic(it), underline(ul), inverse(inv), width(w) {}

bool Cell::operator==(const Cell& o) const {
        return ch == o.ch && fg == o.fg && bg == o.bg &&
               bold == o.bold && dim == o.dim && italic == o.italic &&
               underline == o.underline && inverse == o.inverse && width == o.width;
    }

bool Cell::operator!=(const Cell& o) const { return !(*this == o); }
