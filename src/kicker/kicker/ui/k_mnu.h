/*****************************************************************

Copyright (c) 1996-2000 the kicker authors. See file AUTHORS.

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

#ifndef __k_mnu_h__
#define __k_mnu_h__

#include <dcopobject.h>
#include <tqintdict.h>
#include <tqpixmap.h>
#include <tqtimer.h>
#include <tdeversion.h>
#include <tqguardedptr.h>

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

#include "service_mnu.h"

namespace KPIM {
    // Yes, ClickLineEdit was copied from libtdepim.
    // Can we have it in tdelibs please?
    class ClickLineEdit;
}

class KickerClientMenu;
class KBookmarkMenu;
class TDEActionCollection;
class KBookmarkOwner;
class Panel;
class TDEAccel;

class PanelKMenu : public PanelServiceMenu, public DCOPObject
{
    TQ_OBJECT
    K_DCOP

k_dcop:
    void slotServiceStartedByStorageId(TQString starter, TQString desktopPath);
    void hideMenu();

public:
    PanelKMenu();
    ~PanelKMenu();

    int insertClientMenu(KickerClientMenu *p);
    void removeClientMenu(int id);

    virtual TQSize sizeHint() const;
    virtual void setMinimumSize(const TQSize &);
    virtual void setMaximumSize(const TQSize &);
    virtual void setMinimumSize(int, int);
    virtual void setMaximumSize(int, int);
    virtual void showMenu();
    void clearRecentMenuItems();

public slots:
    virtual void initialize();

    //### KDE4: workaround for Qt bug, remove later
    virtual void resize(int width, int height);

protected slots:
    void slotLock();
    void slotLogout();
    void slotShutdown();
    void slotReboot();
    void slotSuspend(int);
    void slotPopulateSessions();
    void slotPopulateLogout();
    void slotSessionActivated( int );
    void slotSaveSession();
    void slotInitPowerSystem();

private:
    bool m_canShutdown;
    bool m_canReboot; // Not used strictly but good for consistency? Setup uses canPowerOff for both.
    bool m_canSuspend;
    bool m_canHibernate;
    bool m_canHybridSuspend;
    bool m_canFreeze;
    bool m_powerSystemInitialized;
protected slots:
    void slotRunCommand();
    void slotEditUserContact();
    void slotUpdateSearch(const TQString &searchtext);
    void slotClearSearch();
    void slotFocusSearch();
    void paletteChanged();
    virtual void slotClear();
    virtual void configChanged();
    void updateRecent();
    void slotToggleRecentMode();
    void repairDisplay();
    void windowClearTimeout();
    void slotReinitialize();

protected:
    TQRect sideImageRect();
    TQMouseEvent translateMouseEvent(TQMouseEvent* e);
    void resizeEvent(TQResizeEvent *);
    virtual void hideEvent(TQHideEvent *);
    void paintEvent(TQPaintEvent *);
    void mousePressEvent(TQMouseEvent *);
    void mouseReleaseEvent(TQMouseEvent *);
    void mouseMoveEvent(TQMouseEvent *);
    bool loadSidePixmap();
    void doNewSession(bool lock);
    void filterMenu(PanelServiceMenu* menu, const TQString &searchString);
    void keyPressEvent(TQKeyEvent* e);
    bool eventFilter(TQObject *o, TQEvent *e);
    void createRecentMenuItems();
    virtual void clearSubmenus();
    void buildServiceCache();

private slots:
    void slotOnShow();
    void slotSearchReturnPressed();

private:
    TQPopupMenu                 *sessionsMenu;
    TQPopupMenu                 *logoutMenu;
    TQPixmap                     sidePixmap;
    TQPixmap                     sideTilePixmap;
    int                         client_id;
    bool                        delay_init;
    TQIntDict<KickerClientMenu>  clients;
    KBookmarkMenu              *bookmarkMenu;
    TDEActionCollection          *actionCollection;
    KBookmarkOwner             *bookmarkOwner;
    PopupMenuList               dynamicSubMenus;
    TQGuardedPtr<KPIM::ClickLineEdit> searchEdit;
    TQGuardedPtr<TDEAccel>           accel;
    static const int            searchLineID;
    TQTimer                    *displayRepairTimer;
    TQTimer                    *blockMouseTimer;  // Grace period timer
    bool                        displayRepaired;
    bool                        windowTimerTimedOut;
    TQTimer                    *popupCloseTimer;  // Grace period for sidebar popup close
    bool                        m_inFlatSearchMode;
    TQValueList<KService::Ptr>   m_cachedServices;

protected slots:
    void slotClosePopupTimeout();
    bool                        m_servicesCached;
    KService::Ptr               m_singleMatch;
    int                          m_hoveredSidebarBtn; // -1=none, 0=switchuser, 1=logout
    int m_delayedHoverBtn; // Target button index for delayed hover switch
};

#endif
