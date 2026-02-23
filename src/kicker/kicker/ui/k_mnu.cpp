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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <dmctl.h>

#include <tqhbox.h>
#include <tqimage.h>
#include <tqlabel.h>
#include <tqpainter.h>
#include <tqstyle.h>
#include <tqtimer.h>
#include <tqtooltip.h>
#include <tqeventloop.h>

#include <dcopclient.h>
#include <tdeapplication.h>
#include <tdeabouttde.h>
#include <tdeaction.h>
#include <kbookmarkmenu.h>
#include <tdeconfig.h>
#include <kdebug.h>
#include <tdeglobal.h>
#include <tdeglobalsettings.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kstandarddirs.h>
#include <tdetoolbarbutton.h>
#include <twin.h>
#include <popupmenutop.h>
#include <tdeaccel.h>
#include <netwm.h>
#undef Success
#undef FocusIn
#undef FocusOut

#include "client_mnu.h"
#include "container_base.h"
#include "global.h"
#include "kbutton.h"
#include "kicker.h"
#include "kickerSettings.h"
#include "konqbookmarkmanager.h"
#include "menuinfo.h"
#include "menumanager.h"
#include "popupmenutitle.h"
#include "quickbrowser_mnu.h"
#include "recentapps.h"
#include "clicklineedit.h"

#include <krun.h>
#include <kprocess.h>
#include <dcopref.h>
#ifdef WITH_TDEHWLIB
#include <tdehardwaredevices.h>
#endif

#include "k_mnu.h"
#include "k_mnu.moc"
#include <kuser.h>

namespace SuspendType {
enum SuspendType {
    NotSpecified = 0,
    Freeze,
    Standby,
    Suspend,
    Hibernate,
    HybridSuspend
};
};

const int PanelKMenu::searchLineID(23140 /*whatever*/);

PanelKMenu::PanelKMenu()
  : PanelServiceMenu(TQString::null, TQString::null, 0, "KMenu")
  , bookmarkMenu(0)
  , bookmarkOwner(0)
  , accel(0)
  , searchEdit(0)
  , displayRepaired(false)
  , m_inFlatSearchMode(false)
  , m_servicesCached(false)
  , m_hoveredSidebarBtn(-1)
  , m_canShutdown(false)
  , m_canReboot(false)
  , m_canSuspend(false)
  , m_canHibernate(false)
  , m_canHybridSuspend(false)
  , m_canFreeze(false)
  , m_powerSystemInitialized(false)
  , m_delayedHoverBtn(-1)
{
    static const TQCString dcopObjId("KMenu");
    DCOPObject::setObjId(dcopObjId);
    // set the first client id to some arbitrarily large value.
    client_id = 10000;
    // Don't automatically clear the main menu.
    disableAutoClear();
    actionCollection = new TDEActionCollection(this);
    setCaption(i18n("TDE Menu"));
    connect(Kicker::the(), TQT_SIGNAL(configurationChanged()),
            this, TQT_SLOT(configChanged()));
    DCOPClient *dcopClient = TDEApplication::dcopClient();
    dcopClient->connectDCOPSignal(0, "appLauncher",
        "serviceStartedByStorageId(TQString,TQString)",
        dcopObjId,
        "slotServiceStartedByStorageId(TQString,TQString)",
        false);
    displayRepairTimer = new TQTimer( this );
    connect( displayRepairTimer, TQT_SIGNAL(timeout()), this, TQT_SLOT(repairDisplay()) );
    blockMouseTimer = new TQTimer(this);
    setMouseTracking(true);

    // Initialise power system capabilities asynchronously to avoid startup lag
    TQTimer::singleShot( 0, this, TQT_SLOT(slotInitPowerSystem()) );

    // Initialize Logout Menu
    logoutMenu = new TQPopupMenu(this);
    connect( logoutMenu, TQT_SIGNAL(aboutToShow()), TQT_SLOT(slotPopulateLogout()) );
    connect( logoutMenu, TQT_SIGNAL(activated(int)), TQT_SLOT(slotSuspend(int)) );
    logoutMenu->installEventFilter(this);
    
    TQPalette pal = palette();
    pal.setColor(TQColorGroup::Background, KickerSettings::classicKMenuBackgroundColor());
    setPalette(pal);
    logoutMenu->setPalette(pal);

    // Grace period timer for sidebar popup closing
    popupCloseTimer = new TQTimer(this);
    connect( popupCloseTimer, TQT_SIGNAL(timeout()), TQT_SLOT(slotClosePopupTimeout()) );
}

PanelKMenu::~PanelKMenu()
{
    clearSubmenus();
    delete bookmarkMenu;
    delete bookmarkOwner;
}

void PanelKMenu::slotServiceStartedByStorageId(TQString starter,
                                               TQString storageId)
{
    if (starter != "kmenu")
    {
        kdDebug() << "KMenu - updating recently used applications: " <<
            storageId << endl;
        KService::Ptr service = KService::serviceByStorageId(storageId);
        updateRecentlyUsedApps(service);
    }
}

void PanelKMenu::hideMenu()
{
    hide();

    // Try to redraw the area under the menu
    // Qt makes this surprisingly difficult to do in a timely fashion!
    while (isShown() == true)
        kapp->eventLoop()->processEvents(1000);
    TQTimer *windowtimer = new TQTimer( this );
    connect( windowtimer, TQT_SIGNAL(timeout()), this, TQT_SLOT(windowClearTimeout()) );
    windowTimerTimedOut = false;
    windowtimer->start( 0, true );	// Wait for all window system events to be processed
    while (windowTimerTimedOut == false)
        kapp->eventLoop()->processEvents(TQEventLoop::ExcludeUserInput, 1000);

    // HACK
    // The TDE Menu takes an unknown amount of time to disappear, and redrawing
    // the underlying window(s) also takes time.  This should allow both of those
    // events to occur (unless you're on a 200MHz Pentium 1 or similar ;-))
    // thereby removing a bad shutdown screen artifact while still providing
    // a somewhat snappy user interface.
    TQTimer *delaytimer = new TQTimer( this );
    connect( delaytimer, TQT_SIGNAL(timeout()), this, TQT_SLOT(windowClearTimeout()) );
    windowTimerTimedOut = false;
    delaytimer->start( 100, true );	// Wait for 100 milliseconds
    while (windowTimerTimedOut == false)
        kapp->eventLoop()->processEvents(TQEventLoop::ExcludeUserInput, 1000);
}

void PanelKMenu::windowClearTimeout()
{
    windowTimerTimedOut = true;
}

bool PanelKMenu::loadSidePixmap()
{
    if (!KickerSettings::useSidePixmap()) {
        return false;
    }

    // Sidebar width: 36px to accommodate icon buttons
    // Height: 36px base, tiling will handle the rest
    int sideWidth = 36;
    int sideHeight = 36;

    // Create pixmaps with the background color of the menu
    sidePixmap.resize(sideWidth, sideHeight);
    sideTilePixmap.resize(sideWidth, sideHeight);

    // Get the background color from the configured setting
    TQColor bgColor = KickerSettings::classicKMenuBackgroundColor();

    // Fill the pixmaps
    sidePixmap.fill(bgColor);
    sideTilePixmap.fill(bgColor);

    return true;
}

void PanelKMenu::paletteChanged()
{
    if (!loadSidePixmap())
    {
        sidePixmap = sideTilePixmap = TQPixmap();
        setMinimumSize( sizeHint() );
    }
}

/* A MenuHBox is supposed to be inserted into a menu.
 * You can set a special widget in the hbox which will
 * get the focus if the user moves up or down with the
 * cursor keys
 */
class MenuHBox : public TQHBox {
public:
    MenuHBox(PanelKMenu* parent) : TQHBox(parent)
    {
    }

    virtual void keyPressEvent(TQKeyEvent *e)
    {

    }
private:
    PanelKMenu *parent;
};

void PanelKMenu::initialize()
{
//    kdDebug(1210) << "PanelKMenu::initialize()" << endl;
    updateRecent();

    if (initialized())
    {
        return;
    }

    if (loadSidePixmap())
    {
        // in case we've been through here before, let's disconnect
        disconnect(kapp, TQT_SIGNAL(tdedisplayPaletteChanged()),
                   this, TQT_SLOT(paletteChanged()));
        connect(kapp, TQT_SIGNAL(tdedisplayPaletteChanged()),
                this, TQT_SLOT(paletteChanged()));
    }
    else
    {
        sidePixmap = sideTilePixmap = TQPixmap();
    }

    // SAFE RE-INITIALIZATION:
    // The search bar is NOT a menu item (not inserted via insertItem),
    // so TQPopupMenu::clear() will not delete it. We still detach it
    // from the widget tree during reinitialization for safety.
    TQHBox* hbox_to_detach = (searchEdit) ? static_cast<TQHBox*>(searchEdit->parent()) : 0;
    if (hbox_to_detach) {
        hbox_to_detach->reparent(NULL, TQPoint(0,0));
    }

    // add services
    PanelServiceMenu::initialize();


    //TQToolTip::add(clearButton, i18n("Clear Search"));
    //TQToolTip::add(searchEdit, i18n("Enter the name of an application"));

    if (KickerSettings::showMenuTitles())
    {
        int id;
        id = insertItem(new PopupMenuTitle(i18n("All Applications"), font()), -1 /* id */, 0);
        setItemEnabled( id, false );
        id = insertItem(new PopupMenuTitle(i18n("Actions"), font()), -1 /* id */, -1);
        setItemEnabled( id, false );
    }

    // create recent menu section
    createRecentMenuItems();

    bool need_separator = false;

    // insert bookmarks
    if (KickerSettings::useBookmarks() && kapp->authorizeTDEAction("bookmarks"))
    {
        // Need to create a new popup each time, it's deleted by subMenus.clear()
        TDEPopupMenu * bookmarkParent = new TDEPopupMenu( this, "bookmarks" );
        if(!bookmarkOwner)
            bookmarkOwner = new KBookmarkOwner;
        delete bookmarkMenu; // can't reuse old one, the popup has been deleted
        bookmarkMenu = new KBookmarkMenu( KonqBookmarkManager::self(), bookmarkOwner, bookmarkParent, actionCollection, true, false );

        insertItem(KickerLib::menuIconSet("bookmark"), i18n("Bookmarks"), bookmarkParent);

        subMenus.append(bookmarkParent);
        need_separator = true;
    }

    // insert quickbrowser
    if (KickerSettings::useBrowser())
    {
        PanelQuickBrowser *browserMnu = new PanelQuickBrowser(this);
        browserMnu->initialize();

        insertItem(KickerLib::menuIconSet("kdisknav"),
                   i18n("Quick Browser"),
                   KickerLib::reduceMenu(browserMnu));
        subMenus.append(browserMnu);
        need_separator = true;
    }

    // insert dynamic menus
    TQStringList menu_ext = KickerSettings::menuExtensions();
    if (!menu_ext.isEmpty())
    {
        for (TQStringList::ConstIterator it=menu_ext.begin(); it!=menu_ext.end(); ++it)
        {
            MenuInfo info(*it);
            if (!info.isValid())
               continue;

            KPanelMenu *menu = info.load();
            if (menu)
            {
                insertItem(KickerLib::menuIconSet(info.icon()), info.name(), menu);
                dynamicSubMenus.append(menu);
                need_separator = true;
            }
        }
    }

    if (need_separator)
        insertSeparator();

    // insert client menus, if any
    if (clients.count() > 0) {
        TQIntDictIterator<KickerClientMenu> it(clients);
        while (it){
            if (it.current()->text.at(0) != '.')
                insertItem(
                    it.current()->icon,
                    it.current()->text,
                    it.current(),
                    it.currentKey()
                    );
            ++it;
        }
        insertSeparator();
    }

    // run command
    if (kapp->authorize("run_command"))
    {
        insertItem(KickerLib::menuIconSet("system-run"),
                   i18n("Run Command..."),
                   this,
                   TQT_SLOT( slotRunCommand()));
        insertSeparator();
    }

    // Switch User: create sessions submenu (will be shown from sidebar icon click)
    if (DM().isSwitchable() && kapp->authorize("switch_user"))
    {
        sessionsMenu = new TQPopupMenu( this );
        sessionsMenu->installEventFilter(this);
        connect( sessionsMenu, TQT_SIGNAL(aboutToShow()), TQT_SLOT(slotPopulateSessions()) );
        connect( sessionsMenu, TQT_SIGNAL(activated(int)), TQT_SLOT(slotSessionActivated(int)) );
        sessionsMenu->setPalette(palette());
    }

    /*
      If  the user configured ksmserver to
    */
    TDEConfig ksmserver("ksmserverrc", false, false);
    ksmserver.setGroup("General");
    if (ksmserver.readEntry( "loginMode" ) == "restoreSavedSession")
    {
        insertItem(KickerLib::menuIconSet("document-save"), i18n("Save Session"), this, TQT_SLOT(slotSaveSession()));
    }

    /*
    if (kapp->authorize("lock_screen"))
    {
        insertItem(KickerLib::menuIconSet("system-lock-screen"), i18n("Lock Session"), this, TQT_SLOT(slotLock()));
    }
    */

#if 0
    // WABA: tear off handles don't work together with dynamically updated
    // menus. We can't update the menu while torn off, and we don't know
    // when it is torn off.
    if (TDEGlobalSettings::insertTearOffHandle())
      insertTearOffHandle();
#endif

    if (displayRepaired == false) {
        displayRepairTimer->start(5, false);
        displayRepaired = true;
    }

    if (KickerSettings::useSearchBar()) {
        if (!searchEdit) {
            TQHBox* hbox = new TQHBox( this );
            TDEToolBarButton *clearButton = new TDEToolBarButton( "locationbar_erase", 0, hbox );

            TQStringList cuts = TQStringList::split(";", KickerSettings::searchShortcut());
            TQString placeholder;
            switch( cuts.count() )
            {
                case 0: placeholder = i18n(" Click here to search..."); break;
                case 1: placeholder = i18n(" Press '%1' to search...").arg(cuts[0]); break;
                case 2: placeholder = i18n(" Press '%1' or '%2' to search...").arg(cuts[0], cuts[1]); break;
            }
            searchEdit = new KPIM::ClickLineEdit( hbox, placeholder );
            hbox->setFocusProxy(searchEdit);
            hbox->setSpacing( 3 );
            connect(clearButton, TQT_SIGNAL(clicked()), searchEdit, TQT_SLOT(clear()));
            connect(this, TQT_SIGNAL(aboutToHide()), this, TQT_SLOT(slotClearSearch()));
            connect(this, TQT_SIGNAL(activated(int)), this, TQT_SLOT(slotClearSearch()));
            connect(searchEdit, TQT_SIGNAL(textChanged(const TQString&)),
                this, TQT_SLOT( slotUpdateSearch( const TQString&)));
            connect(searchEdit, TQT_SIGNAL(returnPressed()),
                this, TQT_SLOT(slotSearchReturnPressed()));
            
            if (!accel) {
                accel = new TDEAccel(this);
                accel->insert("search", i18n("Search"), i18n("TDE Menu search"),
                              TDEShortcut(KickerSettings::searchShortcut()),
                              this, TQT_SLOT(slotFocusSearch()));
            }

            // FOCUS MANAGEMENT:
            // 1. Auto-focus search bar when menu opens
            connect(this, TQT_SIGNAL(aboutToShow()), this, TQT_SLOT(slotOnShow()));
            
            // 2. Deselect menu items when clicking/focusing search bar
            searchEdit->installEventFilter(this);
        }

        // Simplification: Search bar is now hidden by default (Type-to-Search)
        // if (searchEdit) {
        //     TQHBox* hbox_to_reinsert = static_cast<TQHBox*>(searchEdit->parent());
        //     if (hbox_to_reinsert) {
        //         hbox_to_reinsert->reparent(this, TQPoint(0,0));
        //         hbox_to_reinsert->show();
        //         hbox_to_reinsert->raise();
        //         insertItem(hbox_to_reinsert, searchLineID);
        //     }
        // }
        // Ensure detached but valid parent for safety
        if (searchEdit) {
             TQHBox* hbox = static_cast<TQHBox*>(searchEdit->parent());
             if (hbox) {
                 hbox->reparent(this, TQPoint(0,0)); // Keep it alive
                 hbox->hide(); // Hide it
             }
        }
    }

    setInitialized(true);
}

void PanelKMenu::repairDisplay(void) {
    if (isShown() == true) {
        displayRepairTimer->stop();

        // Now do a nasty hack to prevent search bar merging into the side image
        // This forces a layout/repaint of the qpopupmenu
        repaint();			// This ensures that the side bar image was applied
        styleChange(style());		// This forces a call to the private function updateSize(true) inside the qpopupmenu.
        update();			// This repaints the entire popup menu to apply the widget size/alignment changes made above
    }
}

int PanelKMenu::insertClientMenu(KickerClientMenu *p)
{
    int id = client_id;
    clients.insert(id, p);
    slotClear();
    return id;
}

void PanelKMenu::removeClientMenu(int id)
{
    clients.remove(id);
    removeItem(id);
    slotClear();
}

extern int kicker_screen_number;

void PanelKMenu::slotLock()
{
    TQCString appname( "kdesktop" );
    if ( kicker_screen_number ) {
        appname.sprintf("kdesktop-screen-%d", kicker_screen_number);
    }
    TQCString replyType;
    TQByteArray replyData;
    // Block here until lock is complete
    // If this is not done the desktop of the locked session will be shown after VT switch until the lock fully engages!
    kapp->dcopClient()->call(appname, "KScreensaverIface", "lock()", TQCString(""), replyType, replyData);
}

void PanelKMenu::slotLogout()
{
    hide();
    kapp->requestShutDown();
}

void PanelKMenu::slotPopulateSessions()
{
    int p = 0;
    DM dm;

    sessionsMenu->clear();
    sessionsMenu->setMinimumWidth(0);

    // Title at top
    KUser currentUser;
    TQString userName = currentUser.fullName();
    if (userName.isEmpty()) userName = currentUser.loginName();
    sessionsMenu->insertItem(new PopupMenuTitle(userName, font()), 97, -1);

    // 1 padding item after title
    TQIconSet padIcon = KickerLib::menuIconSet("search_empty");
    int pad1 = sessionsMenu->insertItem(padIcon, TQString(" "), -1, -1);
    sessionsMenu->setItemEnabled(pad1, false);

    // Lock Session (moved from main menu)
    if (kapp->authorize("lock_screen")) {
        sessionsMenu->insertItem(SmallIconSet("system-lock-screen"), i18n("Lock Session"), 99);
    }

    if (kapp->authorize("start_new_session") && (p = dm.numReserve()) >= 0)
    {
        sessionsMenu->insertItem(SmallIconSet("switchuser"), i18n("Start New Session"), 101 );
        if (!p) {
            sessionsMenu->setItemEnabled( 101, false );
        }
    }
    SessList sess;
    if (dm.localSessions( sess ))
        for (SessList::ConstIterator it = sess.begin(); it != sess.end(); ++it) {
            int id = sessionsMenu->insertItem( DM::sess2Str( *it ), (*it).vt );
            if (!(*it).vt)
                sessionsMenu->setItemEnabled( id, false );
            if ((*it).self)
                sessionsMenu->setItemChecked( id, true );
        }

    // Log Out (end session) at the bottom of the sessions list
    sessionsMenu->insertSeparator();
    sessionsMenu->insertItem(SmallIconSet("menu-logout"), i18n("Log Out"), 98);

    // Bottom padding to fill remaining height
    sessionsMenu->adjustSize();
    int targetH = height();
    int safety = 0;
    while (sessionsMenu->sizeHint().height() < targetH && safety++ < 20) {
        int id = sessionsMenu->insertItem(padIcon, TQString(" "), -1, -1);
        sessionsMenu->setItemEnabled(id, false);
        sessionsMenu->adjustSize();
    }

    // Ensure sessionsMenu fully covers main menu width
    int minW = width() - 15;
    if (sessionsMenu->sizeHint().width() < minW) {
        sessionsMenu->setMinimumWidth(minW);
    }
    sessionsMenu->adjustSize();
}

void PanelKMenu::slotInitPowerSystem()
{
    if (m_powerSystemInitialized) return;

#if defined(WITH_TDEHWLIB)
    TDERootSystemDevice* rootDevice = TDEGlobal::hardwareDevices()->rootSystemDevice();
    if (rootDevice) {
        m_canShutdown = rootDevice->canPowerOff();
        m_canFreeze = rootDevice->canFreeze();
        m_canSuspend = rootDevice->canSuspend();
        m_canHibernate = rootDevice->canHibernate();
        m_canHybridSuspend = rootDevice->canHybridSuspend();
    }
#endif
    m_powerSystemInitialized = true;
}

void PanelKMenu::slotPopulateLogout()
{
    logoutMenu->clear();
    logoutMenu->setMinimumWidth(0);

    // Title at top
    logoutMenu->insertItem(new PopupMenuTitle(i18n("Shutdown"), font()), 96, -1);

    // 1 padding item after title
    TQIconSet padIcon = KickerLib::menuIconSet("search_empty");
    int pad1 = logoutMenu->insertItem(padIcon, TQString(" "), -1, -1);
    logoutMenu->setItemEnabled(pad1, false);

    // Ensure power system is initialized (fallback if timer didn't fire yet)
    if (!m_powerSystemInitialized) {
        slotInitPowerSystem();
    }

    // Shutdown / Restart
    if (m_canShutdown) {
        logoutMenu->insertItem(SmallIconSet("kickermenu-logout"), i18n("Shutdown"), this, TQT_SLOT(slotShutdown()));
        logoutMenu->insertItem(SmallIconSet("menu-restart"), i18n("Restart"), this, TQT_SLOT(slotReboot()));
    }

    // Suspend / Hibernate options (respect power-manager settings)
    TDEConfig pmcfg("power-managerrc");
    bool disableSuspend = pmcfg.readBoolEntry("disableSuspend", false);
    bool disableHibernate = pmcfg.readBoolEntry("disableHibernate", false);

    bool anySuspend = (m_canFreeze && !disableSuspend)
                   || (m_canSuspend && !disableSuspend)
                   || (m_canHibernate && !disableHibernate)
                   || (m_canHybridSuspend && !disableSuspend && !disableHibernate);

    if (anySuspend) {
        logoutMenu->insertSeparator();
    }
    if (m_canFreeze && !disableSuspend) {
        logoutMenu->insertItem(SmallIconSet("kickermenu-logout"), i18n("Freeze"), SuspendType::Freeze);
    }
    if (m_canSuspend && !disableSuspend) {
        logoutMenu->insertItem(SmallIconSet("menu-sleep"), i18n("Suspend"), SuspendType::Suspend);
    }
    if (m_canHibernate && !disableHibernate) {
        logoutMenu->insertItem(SmallIconSet("menu-hibernate"), i18n("Hibernate"), SuspendType::Hibernate);
    }
    if (m_canHybridSuspend && !disableSuspend && !disableHibernate) {
        logoutMenu->insertItem(SmallIconSet("menu-hybrid"), i18n("Hybrid Suspend"), SuspendType::HybridSuspend);
    }

    // Bottom padding to fill remaining height
    logoutMenu->adjustSize();
    int targetH = height();
    int safety = 0;
    while (logoutMenu->sizeHint().height() < targetH && safety++ < 20) {
        int id = logoutMenu->insertItem(padIcon, TQString(" "), -1, -1);
        logoutMenu->setItemEnabled(id, false);
        logoutMenu->adjustSize();
    }

    // Ensure logoutMenu fully covers main menu width
    int minW = width() - 5;
    if (logoutMenu->sizeHint().width() < minW) {
        logoutMenu->setMinimumWidth(minW);
    }
    logoutMenu->adjustSize();
}

void PanelKMenu::slotShutdown()
{
    hide();
    TQByteArray params;
    TQDataStream stream(params, IO_WriteOnly);
    stream << (int)2 << (int)-1 << TQString("");
    kapp->dcopClient()->send("ksmserver", "default", "logoutTimed(int,int,TQString)", params);
}

void PanelKMenu::slotReboot()
{
    hide();
    TQByteArray params;
    TQDataStream stream(params, IO_WriteOnly);
    stream << (int)1 << (int)-1 << TQString();
    kapp->dcopClient()->send("ksmserver", "default", "logoutTimed(int,int,TQString)", params);
}

void PanelKMenu::slotSuspend(int id)
{
    // Ignore title (ID 96) and non-suspend IDs
    if (id == 96 || id < 0) return;

    hide();
    bool error = true;

    // Lock screen before suspend if configured
    TDEConfig pmcfg("power-managerrc");
    if (pmcfg.readBoolEntry("lockOnResume", true)) {
        DCOPRef("kdesktop", "KScreensaverIface").call("lock()");
    }

#if defined(WITH_TDEHWLIB)
    TDERootSystemDevice* rootDevice = TDEGlobal::hardwareDevices()->rootSystemDevice();
    if (rootDevice) {
        if (id == SuspendType::Freeze) {
            error = !rootDevice->setPowerState(TDESystemPowerState::Freeze);
        } else if (id == SuspendType::Suspend) {
            error = !rootDevice->setPowerState(TDESystemPowerState::Suspend);
        } else if (id == SuspendType::Hibernate) {
            error = !rootDevice->setPowerState(TDESystemPowerState::Hibernate);
        } else if (id == SuspendType::HybridSuspend) {
            error = !rootDevice->setPowerState(TDESystemPowerState::HybridSuspend);
        } else {
            return;
        }
    }
#else
    error = false;
#endif
    if (error) {
        KMessageBox::error(this, i18n("Suspend failed"));
    }
}

void PanelKMenu::slotSessionActivated( int ent )
{
    // Ignore title item (ID 97) and padding items — do nothing
    if (ent == 97 || ent < 0) return;

    // Close menus immediately to release input grabs and visual focus
    if (sessionsMenu) sessionsMenu->hide();
    hide();
    if (ent == 98)
        TQTimer::singleShot(0, this, TQT_SLOT(slotLogout()));
    else if (ent == 99)
        TQTimer::singleShot(0, this, TQT_SLOT(slotLock()));
    else if (ent == 101)
        doNewSession( true );
    else if (sessionsMenu && !sessionsMenu->isItemChecked( ent ))
        DM().lockSwitchVT( ent );
}

void PanelKMenu::doNewSession( bool lock )
{
    int result = KMessageBox::warningContinueCancel(
        kapp->desktop()->screen(kapp->desktop()->screenNumber(this)),
        i18n("<p>You have chosen to open another desktop session.<br>"
               "The current session will be hidden "
               "and a new login screen will be displayed.<br>"
               "An F-key is assigned to each session; "
               "F%1 is usually assigned to the first session, "
               "F%2 to the second session and so on. "
               "You can switch between sessions by pressing "
               "Ctrl, Alt and the appropriate F-key at the same time. "
               "Additionally, the TDE Panel and Desktop menus have "
               "actions for switching between sessions.</p>")
                           .arg(7).arg(8),
        i18n("Warning - New Session"),
        KGuiItem(i18n("&Start New Session"), "fork"),
        ":confirmNewSession",
        KMessageBox::PlainCaption | KMessageBox::Notify);

    if (result==KMessageBox::Cancel)
        return;

    if (lock) {
        slotLock();
    }

    DM().startReserve();
}

void PanelKMenu::slotSaveSession()
{
    TQByteArray data;
    kapp->dcopClient()->send( "ksmserver", "default",
                              "saveCurrentSession()", data );
}

void PanelKMenu::slotRunCommand()
{
    TQByteArray data;
    TQCString appname( "kdesktop" );
    if ( kicker_screen_number )
        appname.sprintf("kdesktop-screen-%d", kicker_screen_number);

    kapp->updateRemoteUserTimestamp( appname );
    kapp->dcopClient()->send( appname, "KDesktopIface",
                              "popupExecuteCommand()", data );
}

void PanelKMenu::slotEditUserContact()
{
}

void PanelKMenu::setMinimumSize(const TQSize & s)
{
    KPanelMenu::setMinimumSize(s.width() + sidePixmap.width(), s.height());
}

void PanelKMenu::setMaximumSize(const TQSize & s)
{
    KPanelMenu::setMaximumSize(s.width() + sidePixmap.width(), s.height());
}

void PanelKMenu::setMinimumSize(int w, int h)
{
    KPanelMenu::setMinimumSize(w + sidePixmap.width(), h);
}

void PanelKMenu::setMaximumSize(int w, int h)
{
  KPanelMenu::setMaximumSize(w + sidePixmap.width(), h);
}

void PanelKMenu::showMenu()
{
    kdDebug( 1210 ) << "PanelKMenu::showMenu()" << endl;
    PanelPopupButton *kButton = MenuManager::the()->findKButtonFor(this);
    if (kButton)
    {
        adjustSize();
        kButton->showMenu();
    }
    else
    {
        show();
    }
}

TQRect PanelKMenu::sideImageRect()
{
    return TQStyle::visualRect( TQRect( frameWidth(), frameWidth(), sidePixmap.width(),
                                      height() - 2*frameWidth() ), this );
}

void PanelKMenu::resizeEvent(TQResizeEvent * e)
{
    if (isVisible() && e->oldSize().height() > 0) {
        move(x(), y() - (e->size().height() - e->oldSize().height()));
    }

    PanelServiceMenu::resizeEvent(e);

    setFrameRect( TQStyle::visualRect( TQRect( sidePixmap.width(), 0,
                                      width() - sidePixmap.width(), height() ), this ) );


}

//Workaround Qt3.3.x sizing bug, by ensuring we're always wide enough.
void PanelKMenu::resize(int width, int height)
{
    width = kMax(width, maximumSize().width());
    PanelServiceMenu::resize(width, height);
}

TQSize PanelKMenu::sizeHint() const
{
    TQSize s = PanelServiceMenu::sizeHint();
    return s;
}


void PanelKMenu::hideEvent(TQHideEvent *e)
{
    // Reset sidebar hover state when menu closes
    // This prevents buttons from appearing selected when the menu is reopened
    if (m_hoveredSidebarBtn != -1) {
        m_hoveredSidebarBtn = -1;
        update(sideImageRect());
    }

    if (sessionsMenu) {
        sessionsMenu->hide();
    }
    if (logoutMenu) {
        logoutMenu->hide();
    }
    
    // Call parent implementation
    PanelServiceMenu::hideEvent(e);
}

void PanelKMenu::paintEvent(TQPaintEvent * e)
{
    if (sidePixmap.isNull()) {
        PanelServiceMenu::paintEvent(e);
        return;
    }

    TQPainter p(this);
    p.setClipRegion(e->region());

    TQColor bgColor = KickerSettings::classicKMenuBackgroundColor();

    style().drawPrimitive( TQStyle::PE_PanelPopup, &p,
                           TQRect( 0, 0, width(), height() ),
                           colorGroup(), TQStyle::Style_Default,
                           TQStyleOption( frameWidth(), 0 ) );

    TQRect r = sideImageRect();
    r.setBottom( r.bottom() - sidePixmap.height() );
    if ( r.intersects( e->rect() ) )
    {
        p.drawTiledPixmap( r, sideTilePixmap );
    }

    r = sideImageRect();
    r.setTop( r.bottom() - sidePixmap.height() );
    if ( r.intersects( e->rect() ) )
    {
        TQRect drawRect = r.intersect( e->rect() );
        TQRect pixRect = drawRect;
        pixRect.moveBy( -r.left(), -r.top() );
        p.drawPixmap( drawRect.topLeft(), sidePixmap, pixRect );
    }

    drawContents( &p );

    // Draw sidebar icons at the bottom of the sidebar area
    int fw = frameWidth();
    int sw = sidePixmap.width(); // 36px
    int iconSize = 22;
    int btnSize = 36;
    int iconOff = (btnSize - iconSize) / 2; // center icon in button area
    
    TQColor highlightColor = bgColor.light(110);

    // Button 0: Switch User (top of button stack)
    if (DM().isSwitchable() && kapp->authorize("switch_user")) {
        TQRect switchRect(fw, height() - fw - btnSize * 5, btnSize, btnSize);
        if (m_hoveredSidebarBtn == 0) {
            p.fillRect(switchRect, highlightColor);
        }
        TQPixmap switchIco = SmallIcon("switchuser", iconSize);
        p.drawPixmap(switchRect.x() + iconOff, switchRect.y() + iconOff, switchIco);
    }

    // Button 4: Documents (below Switch User)
    {
        TQRect documentsRect(fw, height() - fw - btnSize * 4, btnSize, btnSize);
        if (m_hoveredSidebarBtn == 4) {
            p.fillRect(documentsRect, highlightColor);
        }
        TQPixmap documentsIco = SmallIcon("menu-docs", iconSize);
        p.drawPixmap(documentsRect.x() + iconOff, documentsRect.y() + iconOff, documentsIco);
    }

    // Button 3: Pictures (middle)
    {
        TQRect picturesRect(fw, height() - fw - btnSize * 3, btnSize, btnSize);
        if (m_hoveredSidebarBtn == 3) {
            p.fillRect(picturesRect, highlightColor);
        }
        TQPixmap picturesIco = SmallIcon("menu-images", iconSize);
        p.drawPixmap(picturesRect.x() + iconOff, picturesRect.y() + iconOff, picturesIco);
    }

    // Button 2: ConfigTDE (lower middle)
    {
        TQRect configRect(fw, height() - fw - btnSize * 2, btnSize, btnSize);
        if (m_hoveredSidebarBtn == 2) {
            p.fillRect(configRect, highlightColor);
        }
        TQPixmap configIco = SmallIcon("menu-settings", iconSize);
        p.drawPixmap(configRect.x() + iconOff, configRect.y() + iconOff, configIco);
    }

    // Button 1: Log Out (bottom)
    if (kapp->authorize("logout")) {
        TQRect logoutRect(fw, height() - fw - btnSize, btnSize, btnSize);
        if (m_hoveredSidebarBtn == 1) {
            p.fillRect(logoutRect, highlightColor);
        }
        TQPixmap logoutIco = SmallIcon("kickermenu-logout", iconSize);
        p.drawPixmap(logoutRect.x() + iconOff, logoutRect.y() + iconOff, logoutIco);
    }
}

TQMouseEvent PanelKMenu::translateMouseEvent( TQMouseEvent* e )
{
    TQRect side = sideImageRect();

    if ( !side.contains( e->pos() ) )
        return *e;

    TQPoint newpos( e->pos() );
    TQApplication::reverseLayout() ?
        newpos.setX( newpos.x() - side.width() ) :
        newpos.setX( newpos.x() + side.width() );
    TQPoint newglobal( e->globalPos() );
    TQApplication::reverseLayout() ?
        newglobal.setX( newpos.x() - side.width() ) :
        newglobal.setX( newpos.x() + side.width() );

    return TQMouseEvent( e->type(), newpos, newglobal, e->button(), e->state() );
}

void PanelKMenu::mousePressEvent(TQMouseEvent * e)
{
    if (sideImageRect().contains(e->pos())) {
        // Close any open submenus (but NOT the main KMenu or sessionsMenu)
        int guard = 0;
        TQWidget *popup = TQApplication::activePopupWidget();
        while (popup && popup != this && popup != sessionsMenu && popup != logoutMenu && guard < 10) {
            popup->hide();
            popup = TQApplication::activePopupWidget();
            ++guard;
        }
        // Check if click is on a sidebar icon
        int fw = frameWidth();
        int btnSize = 36;

        // Switch User button area
        TQRect switchRect(fw, height() - fw - btnSize * 5, btnSize, btnSize);

        if (switchRect.contains(e->pos()) && sessionsMenu) {
            if (logoutMenu) logoutMenu->hide();
            if (sessionsMenu->isVisible()) {
                sessionsMenu->hide();
            } else {
                TQPoint mainPos = mapToGlobal(TQPoint(0, 0));
                TQPoint pos(mapToGlobal(switchRect.topRight()).x() - 6, mainPos.y() - 8);
                sessionsMenu->popup(pos);
            }
            return;
        }

        // Log Out button area
        TQRect logoutRect(fw, height() - fw - btnSize, btnSize, btnSize);
        if (logoutRect.contains(e->pos()) && logoutMenu) {
            if (sessionsMenu) sessionsMenu->hide();
            if (logoutMenu->isVisible()) {
                logoutMenu->hide();
            } else {
                TQPoint mainPos = mapToGlobal(TQPoint(0, 0));
                TQPoint pos(mapToGlobal(logoutRect.topRight()).x() - 6, mainPos.y() - 8);
                logoutMenu->popup(pos);
            }
            return;
        }

        // Clicked elsewhere in sidebar? Close all sidebar popups
        if (sessionsMenu) sessionsMenu->hide();
        if (logoutMenu) logoutMenu->hide();

        // Documents button area
        TQRect documentsRect(fw, height() - fw - btnSize * 4, btnSize, btnSize);
        if (documentsRect.contains(e->pos())) {
            new KRun(KURL(TDEGlobalSettings::documentPath()));
            hide();
            return;
        }

        // Pictures button area
        TQRect picturesRect(fw, height() - fw - btnSize * 3, btnSize, btnSize);
        if (picturesRect.contains(e->pos())) {
            new KRun(KURL(TDEGlobalSettings::picturesPath()));
            hide();
            return;
        }

        // ConfigTDE button area
        TQRect configRect(fw, height() - fw - btnSize * 2, btnSize, btnSize);
        if (configRect.contains(e->pos())) {
            TDEProcess *proc = new TDEProcess;
            *proc << "konqueror" << "--profile" << "ctrl_panel";
            proc->start(TDEProcess::DontCare);
            hide();
            return;
        }

        return; // eat other clicks in sidebar
    }
    TQMouseEvent newEvent = translateMouseEvent(e);
    PanelServiceMenu::mousePressEvent( &newEvent );
}



void PanelKMenu::mouseReleaseEvent(TQMouseEvent *e)
{
    if (sideImageRect().contains(e->pos())) return;
    TQMouseEvent newEvent = translateMouseEvent(e);

    int id = idAt(newEvent.pos());
    if (id == serviceMenuEndId()) {
        slotToggleRecentMode();
        return;
    }

    PanelServiceMenu::mouseReleaseEvent( &newEvent );
}

void PanelKMenu::slotToggleRecentMode()
{
    bool recent = KickerSettings::recentVsOften();
    KickerSettings::setRecentVsOften(!recent);
    KickerSettings::writeConfig();

    RecentlyLaunchedApps::the().configChanged();
    RecentlyLaunchedApps::the().m_bNeedToUpdate = true;

    updateRecent();
}

void PanelKMenu::mouseMoveEvent(TQMouseEvent *e)
{
    // Ignore mouse movements during grace period (e.g. after search clear resizing)
    if (blockMouseTimer && blockMouseTimer->isActive()) {
        return;
    }

    // Determine which sidebar popup menu is currently open (if any)
    // and its corresponding sidebar button index
    TQPopupMenu *activeSidePopup = 0;
    int activeSideBtn = -1;
    if (sessionsMenu && sessionsMenu->isVisible()) {
        activeSidePopup = sessionsMenu;
        activeSideBtn = 0;
    } else if (logoutMenu && logoutMenu->isVisible()) {
        activeSidePopup = logoutMenu;
        activeSideBtn = 1;
    }

    // Handle sidebar area: track hover over icon areas
    if (sideImageRect().contains(e->pos())) {
        // Close any open submenus (but NOT the main KMenu or our sidebar popups)
        int guard = 0;
        TQWidget *popup = TQApplication::activePopupWidget();
        while (popup && popup != this && popup != sessionsMenu && popup != logoutMenu && guard < 10) {
            popup->hide();
            popup = TQApplication::activePopupWidget();
            ++guard;
        }
        setActiveItem(-1);

        int fw = frameWidth();
        int btnSize = 36;
        int oldHover = m_hoveredSidebarBtn;
        int targetBtn = -1;

        TQRect switchRect(fw, height() - fw - btnSize * 5, btnSize, btnSize);
        TQRect documentsRect(fw, height() - fw - btnSize * 4, btnSize, btnSize);
        TQRect picturesRect(fw, height() - fw - btnSize * 3, btnSize, btnSize);
        TQRect configRect(fw, height() - fw - btnSize * 2, btnSize, btnSize);
        TQRect logoutRect(fw, height() - fw - btnSize, btnSize, btnSize);

        if (switchRect.contains(e->pos())) {
             targetBtn = 0;
        }
        else if (configRect.contains(e->pos())) {
             targetBtn = 2;
        }
        else if (picturesRect.contains(e->pos())) {
             targetBtn = 3;
        }
        else if (documentsRect.contains(e->pos())) {
             targetBtn = 4;
        }
        else if (logoutRect.contains(e->pos())) {
             targetBtn = 1;
        }

        if (targetBtn != -1) {
            // If a popup is open and we hover a DIFFERENT button
            if (activeSidePopup && activeSideBtn != targetBtn) {
                 // Delay the switch
                 m_delayedHoverBtn = targetBtn;
                 if (!popupCloseTimer->isActive()) popupCloseTimer->start(300, true);
                 // Keep the current button highlighted (visual stability)
                 if (m_hoveredSidebarBtn != activeSideBtn) {
                     m_hoveredSidebarBtn = activeSideBtn;
                     update(sideImageRect());
                 }
            } else {
                 // Immediate switch (no popup, or hovering same button)
                 m_hoveredSidebarBtn = targetBtn;
                 m_delayedHoverBtn = -1;
                 popupCloseTimer->stop();
            }
        }
        else if (activeSidePopup) {
             // Empty sidebar space: keep the active popup's button highlighted
             m_hoveredSidebarBtn = activeSideBtn;
             m_delayedHoverBtn = -1;
             popupCloseTimer->stop(); // Safe zone
        }

        if (oldHover != m_hoveredSidebarBtn) {
            // Repaint sidebar area to update hover highlight
            update(sideImageRect());
        }
        return;
    }

    // Mouse left sidebar area, clear hover (unless a sidebar popup is open)
    if (m_hoveredSidebarBtn != -1) {
        if (activeSidePopup) {
            m_hoveredSidebarBtn = activeSideBtn; // Keep active popup's button highlighted
            // Timer is handled by event filter on popup menus
        } else {
            m_hoveredSidebarBtn = -1;
            m_delayedHoverBtn = -1;
            popupCloseTimer->stop();
        }
        update(sideImageRect());
    }

    // If a sidebar popup is open, skip parent class handling entirely
    // (parent PanelServiceMenu manages submenus and could interfere with our popup)
    if (activeSidePopup) {
        return;
    }

    TQMouseEvent newEvent = translateMouseEvent(e);
    PanelServiceMenu::mouseMoveEvent( &newEvent );

    // If we are in main menu area and an item is selected, ensure sidebar popups are closed
    if (idAt(e->pos()) != -1) {
        if (sessionsMenu) sessionsMenu->hide();
        if (logoutMenu) logoutMenu->hide();
    }
}


// Build a flat cache of all KService entries from KSycoca.
// Called once on first search, then reused for all subsequent keystrokes.
// O(N) complexity, N = total services. Avoids repeated tree traversal.
void PanelKMenu::buildServiceCache()
{
    m_cachedServices.clear();
    KServiceGroup::Ptr root = KServiceGroup::root();
    if (!root) { m_servicesCached = true; return; }

    TQValueList<KServiceGroup::Ptr> stack;
    stack.append(root);
    while (!stack.isEmpty()) {
        KServiceGroup::Ptr group = stack.front();
        stack.pop_front();
        if (!group) continue;

        KServiceGroup::List list = group->entries(true, true, true,
            KickerSettings::menuEntryFormat() == KickerSettings::NameAndDescription);
        for (KServiceGroup::List::ConstIterator it = list.begin(); it != list.end(); ++it) {
            KSycocaEntry *e = *it;
            if (e && e->isType(KST_KService)) {
                KService::Ptr s(static_cast<KService*>(e));
                if (s && !s->noDisplay())
                    m_cachedServices.append(s);
            } else if (e && e->isType(KST_KServiceGroup)) {
                stack.append(KServiceGroup::Ptr(static_cast<KServiceGroup*>(e)));
            }
        }
    }
    m_servicesCached = true;
}

void PanelKMenu::slotUpdateSearch(const TQString& searchString)
{
    if (searchString.isEmpty()) {
        if (m_inFlatSearchMode) {
            m_inFlatSearchMode = false;
            setMinimumHeight(0);
            setMaximumHeight(30000);
            setMinimumWidth(0);
            setMaximumWidth(30000);
            // Invalidate cache so next search session picks up new apps
            m_servicesCached = false;
            m_cachedServices.clear();
            if (isVisible()) {
                TQTimer::singleShot(0, this, TQT_SLOT(slotReinitialize()));
            } else {
                setInitialized(false);
            }
            setActiveItem(-1);
            if (blockMouseTimer) blockMouseTimer->start(200, true);
        }
        return;
    }

    m_inFlatSearchMode = true;

    // LEAK FIX: Clear entryMap_ before rebuilding.
    // removeItem() only removes from the TQPopupMenu widget, NOT from entryMap_.
    entryMap_.clear();

    for (int i = count() - 1; i >= 0; --i) {
        int id = idAt(i);
        if (id != searchLineID) {
            removeItem(id);
        }
    }

    // Build service cache on first search of this session
    if (!m_servicesCached) {
        buildServiceCache();
    }

    // Filter from cached list instead of re-traversing KSycoca
    TQValueList<KService::Ptr> matches;
    int totalMatches = 0;
    
    for (TQValueList<KService::Ptr>::ConstIterator it = m_cachedServices.begin();
         it != m_cachedServices.end(); ++it) {
        KService::Ptr s = *it;
        if (s && (s->name().find(searchString, 0, false) != -1 ||
                  s->genericName().find(searchString, 0, false) != -1)) {
            totalMatches++;
            if (matches.count() < 10) {
                matches.append(s);
            }
            // Optimization: stop counting if we exceed 50, since we display "50+"
            if (totalMatches > 50) break;
        }
    }

    int index = 0;
    TQString titleString;
    TQString suffixString;
    
    if (totalMatches == 0) {
        titleString = i18n("No Results Found");
    } else if (totalMatches == 1) {
        titleString = i18n("Search Result: ");
        suffixString = i18n("[ENTER] To Launch");
    } else if (totalMatches > 50) {
        titleString = i18n("Search Results (50+)");
    } else {
        titleString = i18n("Search Results (%1)").arg(totalMatches);
    }
    
    PopupMenuTitle* titleItem = new PopupMenuTitle(titleString, font());
    if (!suffixString.isEmpty()) {
        titleItem->setSuffix(suffixString);
    }
    insertItem(titleItem, -1, index++);

    int id = serviceMenuStartId();
    int resultCount = 0;

    if (!matches.isEmpty()) {
        for (TQValueList<KService::Ptr>::Iterator it = matches.begin(); it != matches.end(); ++it) {
            if (*it) { insertMenuItem(*it, id++, index++); resultCount++; }
        }
    }

    // Padding with transparent icon to keep constant menu height
    if (resultCount < 10) {
        TQIconSet padIcon = KickerLib::menuIconSet("search_empty");
        for (int pad = resultCount; pad < 10; ++pad) {
            int padId = insertItem(padIcon, TQString::fromLatin1(" "), -1, index++);
            setItemEnabled(padId, false);
        }
    }

    int screenW = kapp->desktop()->availableGeometry(this).width();
    setFixedWidth((int)(screenW * 0.25));
    adjustSize();

    if (totalMatches == 1 && !matches.isEmpty()) {
        m_singleMatch = matches.first();
    } else {
        m_singleMatch = 0;
    }

    if (searchEdit) searchEdit->setFocus();
}

void PanelKMenu::slotSearchReturnPressed()
{
    if (m_inFlatSearchMode && m_singleMatch) {
        KRun::run(*m_singleMatch, KURL::List(), this);
        hideMenu();
    }
}

void PanelKMenu::slotClearSearch()
{
    if (searchEdit) {
        searchEdit->blockSignals(true);
        searchEdit->clear();
        searchEdit->blockSignals(false);
    }
    
    if (m_inFlatSearchMode) {
        m_inFlatSearchMode = false;
        loaded_ = false;
        setInitialized(false);
        setMinimumHeight(0);
        setMaximumHeight(30000); // Safe max height
        setMinimumWidth(0);
        setMaximumWidth(30000);  // Safe max width
        
        if (isVisible()) {
             TQTimer::singleShot(0, this, TQT_SLOT(slotReinitialize()));
        }
    }
}

void PanelKMenu::slotReinitialize()
{
    if (!isVisible()) {
        setInitialized(false);
        return;
    }
    
    // Explicitly hide during re-init to avoid flickering/ghosts if needed
    // setVisible(false); 
    
    setInitialized(false);
    initialize();
    
    // Force another resize to ensure search bar is positioned correctly
    adjustSize();
    
    if (searchEdit) {
        searchEdit->setFocus();
    }
}

void PanelKMenu::slotClear()
{
}


void PanelKMenu::slotFocusSearch()
{
    if (indexOf(searchLineID) >=0 ) {
        setActiveItem(indexOf(searchLineID));
    }
}

void PanelKMenu::keyPressEvent(TQKeyEvent* e)
{
    // Navigation keys always passthrough to standard menu handling
    if (e->key() == TQt::Key_Up || e->key() == TQt::Key_Down ||
        e->key() == TQt::Key_Left || e->key() == TQt::Key_Right ||
        e->key() == TQt::Key_Return || e->key() == TQt::Key_Enter)
    {
         KPanelMenu::keyPressEvent(e);
         return;
    }
    
    // Type-to-Search: Escape exits search mode if active
    if (e->key() == TQt::Key_Escape)
    {
         if (m_inFlatSearchMode) {
             slotClearSearch();
             return;
         }
         KPanelMenu::keyPressEvent(e);
         return;
    }

    if (!searchEdit) {
        KPanelMenu::keyPressEvent(e);
        return;
    }

    // '/' Shortcut (Legacy support) or Type-to-Search (Alphanumeric)
    bool isSlash = (e->text() == "/");
    bool isTypeToSearch = (!e->text().isEmpty() && e->text().length() == 1 && 
                          e->text()[0].isPrint() && 
                          !(e->state() & (TQt::ControlButton | TQt::AltButton)));

    if (isSlash || isTypeToSearch)
    {
         if (!m_inFlatSearchMode)
         {
             m_inFlatSearchMode = true;
             slotClear(); // Clear normal menu items
             
             // Insert Search Bar at bottom
             TQHBox* hbox = static_cast<TQHBox*>(searchEdit->parent());
             if (hbox) {
                 hbox->reparent(this, TQPoint(0,0));
                 hbox->show();
                 insertItem(hbox, searchLineID);
             }
         }
         
         searchEdit->setFocus();
         if (isTypeToSearch) {
             searchEdit->setText(e->text()); // Inject the typed character
         }
         return;
    }

    KPanelMenu::keyPressEvent(e);
}

void PanelKMenu::slotClosePopupTimeout()
{
    // Determine which sidebar popup menu is currently open
    TQPopupMenu *activeSidePopup = 0;
    int activeSideBtn = -1;
    if (sessionsMenu && sessionsMenu->isVisible()) {
        activeSidePopup = sessionsMenu;
        activeSideBtn = 0;
    } else if (logoutMenu && logoutMenu->isVisible()) {
        activeSidePopup = logoutMenu;
        activeSideBtn = 1;
    }

    if (activeSidePopup) {
        // Timeout expired: apply delayed hover switch and close popup
        if (m_delayedHoverBtn != -1 && m_delayedHoverBtn != activeSideBtn) {
            activeSidePopup->hide();
            m_hoveredSidebarBtn = m_delayedHoverBtn;
            m_delayedHoverBtn = -1;
            update(sideImageRect());
        } 
        else if (m_hoveredSidebarBtn != -1 && m_hoveredSidebarBtn != activeSideBtn) {
            // Fallback for direct hover check
            activeSidePopup->hide();
        }
    }
}

void PanelKMenu::slotOnShow()
{
    // Apply user-configured opacity
    // Redundancy check: PanelServiceMenu base class now handles X11 property application
    // to propagate transparency to all submenus.
    int opacityPercent = KickerSettings::classicKMenuOpacity();
    if (opacityPercent >= 0 && opacityPercent < 100) {
        long opac = (long)((opacityPercent / 100.0) * 0xffffffff);
        static Atom net_wm_opacity = XInternAtom( tqt_xdisplay(), "_NET_WM_WINDOW_OPACITY", False );
        XChangeProperty( tqt_xdisplay(), winId(), net_wm_opacity, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &opac, 1L );
    }

    // Auto-focus search bar when menu opens.
    // Use a singleShot timer to ensure the widget is fully visible and ready.
    // Auto-focus search bar only if already in search mode
    if (searchEdit && m_inFlatSearchMode) {
        TQTimer::singleShot(0, searchEdit, TQT_SLOT(setFocus()));
    }
}

bool PanelKMenu::eventFilter(TQObject *o, TQEvent *e)
{
    // Stop grace period timer if mouse enters the popup menu
    if ((o == sessionsMenu || o == logoutMenu) && 
        (e->type() == TQEvent::Enter || e->type() == TQEvent::MouseMove)) 
    {
        if (popupCloseTimer->isActive()) {
            popupCloseTimer->stop();
        }
    }

    if (o == sessionsMenu && e->type() == TQEvent::MouseButtonRelease) {
        TQMouseEvent *me = static_cast<TQMouseEvent *>(e);
        if (me->button() == TQMouseEvent::LeftButton) {
            int id = sessionsMenu->idAt(me->pos());
            if (id == 97) { // User name title ID
                hide();
                TDEProcess *proc = new TDEProcess;
                *proc << "tdecmshell" << "System/userconfig";
                proc->start(TDEProcess::DontCare);
                return true;
            }
        }
    }

    if (o == logoutMenu && e->type() == TQEvent::MouseButtonRelease) {
        TQMouseEvent *me = static_cast<TQMouseEvent *>(e);
        if (me->button() == TQMouseEvent::LeftButton) {
            int id = logoutMenu->idAt(me->pos());
            if (id == 96) { // Shutdown title ID
                slotLogout();
                return true;
            }
        }
    }

    if (o == searchEdit && e->type() == TQEvent::FocusIn) {
        // When search bar gets focus (click or tab), deselect any active menu item
        // to avoid visual confusion and keyboard navigation conflicts.
        setActiveItem(-1);
    }



    return KPanelMenu::eventFilter(o, e);
}

void PanelKMenu::configChanged()
{
    RecentlyLaunchedApps::the().m_bNeedToUpdate = false;
    RecentlyLaunchedApps::the().configChanged();

    if (!loadSidePixmap()) {
        sidePixmap = sideTilePixmap = TQPixmap();
    }

    PanelServiceMenu::configChanged();
}

// create and fill "recent" section at first
void PanelKMenu::createRecentMenuItems()
{
    RecentlyLaunchedApps::the().m_nNumMenuItems = 0;

    TQStringList RecentApps;
    RecentlyLaunchedApps::the().getRecentApps(RecentApps);

    if (RecentApps.count() > 0)
    {
        bool bNeedTitle = true;
        bool bTitleTop = KickerSettings::useTopSide();
        int nId = serviceMenuEndId() + 1;

        int nIndex;
        if( bTitleTop ) {
            nIndex = 2; // Always account for title (and optional top tile)
        } else {
            nIndex = 1; // Always account for title
        }

        for (TQValueList<TQString>::ConstIterator it =
             RecentApps.fromLast(); /*nop*/; --it)
        {
            KService::Ptr s = KService::serviceByDesktopPath(*it);
            if (!s)
            {
                RecentlyLaunchedApps::the().removeItem(*it);
            }
            else
            {
                if (bNeedTitle)
                {
                    bNeedTitle = false;
                    int id = insertItem(
                        new PopupMenuTitle(
                            RecentlyLaunchedApps::the().caption(), font()),
                        serviceMenuEndId(), 0);
                    setItemEnabled( id, false );
                    if( bTitleTop) {
                        id = insertItem(new PopupMenuTop(), serviceMenuEndId() - 1, 0);
                        setItemEnabled( id, false );
                    }
                }
                insertMenuItem(s, nId++, nIndex);
                RecentlyLaunchedApps::the().m_nNumMenuItems++;
            }

            if (it == RecentApps.begin())
            {
                break;
            }
        }

        insertSeparator(RecentlyLaunchedApps::the().m_nNumMenuItems + (bTitleTop ? 2 : 1));

    }
    else if(KickerSettings::useTopSide())
    {
        int id = insertItem(new PopupMenuTop(),serviceMenuEndId(),0);
        setItemEnabled( id, false );
    }
}

void PanelKMenu::clearSubmenus()
{
    // we don't need to delete these on the way out since the libloader
    // handles them for us
    if (TQApplication::closingDown())
    {
        return;
    }

    for (PopupMenuList::const_iterator it = dynamicSubMenus.constBegin();
            it != dynamicSubMenus.constEnd();
            ++it)
    {
        delete *it;
    }
    dynamicSubMenus.clear();

    PanelServiceMenu::clearSubmenus();
}

void PanelKMenu::updateRecent()
{
    if (!RecentlyLaunchedApps::the().m_bNeedToUpdate)
    {
        return;
    }

    RecentlyLaunchedApps::the().m_bNeedToUpdate = false;

    bool bTitleTop = KickerSettings::useTopSide();

    int nId = serviceMenuEndId() + 1;

    // remove previous items
    if (RecentlyLaunchedApps::the().m_nNumMenuItems > 0)
    {
        // Always account for our custom section title
        int i = -1;
        if(bTitleTop) {
            i = -2;
        }

        for (; i < RecentlyLaunchedApps::the().m_nNumMenuItems; i++)
        {
            removeItem(nId + i);
            entryMap_.remove(nId + i);
        }
        RecentlyLaunchedApps::the().m_nNumMenuItems = 0;

        // Remove the separator
        removeItemAt(0);
    }

    if(bTitleTop) {
        removeItemAt(0);
    }

    // insert new items
    TQStringList RecentApps;
    RecentlyLaunchedApps::the().getRecentApps(RecentApps);

    if (RecentApps.count() > 0)
    {
        bool bNeedTitle = true;
        for (TQValueList<TQString>::ConstIterator it = RecentApps.fromLast();
             /*nop*/; --it)
        {
            KService::Ptr s = KService::serviceByDesktopPath(*it);
            if (!s)
            {
                RecentlyLaunchedApps::the().removeItem(*it);
            }
            else
            {
                if (bNeedTitle)
                {
                    bNeedTitle = false;
                    int id = insertItem(new PopupMenuTitle(
                        RecentlyLaunchedApps::the().caption(),
                            font()), nId - 1, 0);
                    setItemEnabled( id, false );
                    if(bTitleTop) {
                        id = insertItem(new PopupMenuTop(),nId - 1,0);
                        setItemEnabled( id, false );
                    }
                }
                insertMenuItem(s, nId++, 1);
                RecentlyLaunchedApps::the().m_nNumMenuItems++;
            }

            if (it == RecentApps.begin())
                break;
        }
        insertSeparator(RecentlyLaunchedApps::the().m_nNumMenuItems + (bTitleTop ? 2 : 1));
    }
    else if(bTitleTop)
    {
        int id = insertItem(new PopupMenuTop(),serviceMenuEndId(),0);
        setItemEnabled( id, false );
    }
}

void PanelKMenu::clearRecentMenuItems()
{
    RecentlyLaunchedApps::the().clearRecentApps();
    RecentlyLaunchedApps::the().save();
    RecentlyLaunchedApps::the().m_bNeedToUpdate = true;
    updateRecent();
}





