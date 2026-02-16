/*****************************************************************

Copyright (c) 2000 Matthias Elter <elter@kde.org>
                   Matthias Ettrich <ettrich@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#ifndef POPUPMENUTITLE_H
#define POPUPMENUTITLE_H

#include <tqfont.h>
#include <tqstring.h>
#include <tqstyle.h>
#include <tqpainter.h>
#include <tqmenudata.h>
#include <tdeversion.h>
#include <tqapplication.h>

#ifndef TQT_SIGNAL
#define TQT_SIGNAL TQ_SIGNAL
#endif
#ifndef TQT_SLOT
#define TQT_SLOT TQ_SLOT
#endif

// Compatibility for TDE 14.1.5+ where tqdrawPrimitive was renamed to drawPrimitive
#if KDE_IS_VERSION(14, 1, 2)
#define tqdrawPrimitive drawPrimitive
#endif

class PopupMenuTitle : public TQCustomMenuItem
{
public:
    PopupMenuTitle(const TQString &name, const TQFont &font);

    bool fullSpan () const { return true; }

    void setSuffix(const TQString &suffix) 
    { 
        m_suffix = suffix; 
    }

    void paint(TQPainter* p, const TQColorGroup& cg,
               bool /* act */, bool /*enabled*/,
               int x, int y, int w, int h)
    {
        p->save();
        TQRect r(x, y, w, h);
#ifndef TQT_SIGNAL
#define TQT_SIGNAL TQ_SIGNAL
#endif
#ifndef TQT_SLOT
#define TQT_SLOT TQ_SLOT
#endif

        TQApplication::style().tqdrawPrimitive(TQStyle::PE_HeaderSectionMenu,
                                    p, r, cg);

        if (!m_desktopName.isEmpty())
        {
            p->setPen(cg.buttonText());
            p->setFont(m_font);

            if (m_suffix.isEmpty()) {
                p->drawText(x, y, w, h,
                            AlignCenter | SingleLine,
                            m_desktopName);
            } else {
                // Manual centering for mixed-font text
                TQFont normalFont = m_font;
                normalFont.setBold(false);
                
                TQFontMetrics fmBold(m_font);
                TQFontMetrics fmNormal(normalFont);
                
                int w1 = fmBold.width(m_desktopName);
                int w2 = fmNormal.width(m_suffix);
                int totalW = w1 + w2;
                
                // Ensure we don't overflow if window is too narrow (clip from right)
                // But for centering:
                int startX = x + (w - totalW) / 2;
                if (startX < x) startX = x; // flush left if too big
                
                // Vertical alignment assumes single line height is centered by drawText logic
                // Using AlignVCenter should work.
                
                p->drawText(startX, y, w1, h, AlignLeft | AlignVCenter | SingleLine, m_desktopName);
                
                normalFont.setItalic(true);
                p->setFont(normalFont);
                p->setPen(TQColor(125, 125, 125));
                p->drawText(startX + w1, y, w2, h, AlignLeft | AlignVCenter | SingleLine, m_suffix);
            }
        }

        p->setPen(cg.highlight());
        p->drawLine(0, 0, r.right(), 0);
        p->restore();
    }

    void setFont(const TQFont &font)
    {
        m_font = font;
        m_font.setBold(true);
    }

    TQSize sizeHint()
    {
      if (m_suffix.isEmpty()) {
          TQSize size = TQFontMetrics(m_font).size(AlignHCenter, m_desktopName);
          size.setHeight(size.height() +
                         (TQApplication::style().pixelMetric(TQStyle::PM_DefaultFrameWidth) * 2 + 1));
          return size;
      } else {
          // Combined size
          TQFont normalFont = m_font;
          normalFont.setBold(false);
          int w = TQFontMetrics(m_font).width(m_desktopName) + 
                  TQFontMetrics(normalFont).width(m_suffix);
          int h = TQFontMetrics(m_font).height(); // Height dictated by biggest font (usually same)
          
          return TQSize(w, h + (TQApplication::style().pixelMetric(TQStyle::PM_DefaultFrameWidth) * 2 + 1));
      }
    }

  private:
    TQString m_desktopName;
    TQString m_suffix;
    TQFont m_font;
};

#endif
