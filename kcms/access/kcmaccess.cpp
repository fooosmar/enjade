/*
    Copyright 2000 Matthias Hölzer-Klüpfel <hoelzer@kde.org>
    Copyright 2014 Frederik Gladhorn <gladhorn@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "kcmaccess.h"

#include <stdlib.h>
#include <math.h>

#include <QtWidgets/qfiledialog.h>
#include <QStandardPaths>
#include <QtDBus/QtDBus>
#include <QX11Info>

#include <kaboutdata.h>
#include <KConfigGroup>
#include <KSharedConfig>
#include <kkeyserver.h>
#include <knotifyconfigwidget.h>
#include <KPluginFactory>
#include <ktoolinvocation.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <X11/keysymdef.h>

K_PLUGIN_FACTORY(KAccessConfigFactory, registerPlugin<KAccessConfig>();)
K_EXPORT_PLUGIN(KAccessConfigFactory("kcmaccess"))

QString mouseKeysShortcut(Display *display)
{
    // Calculate the keycode
    KeySym sym = XK_MouseKeys_Enable;
    KeyCode code = XKeysymToKeycode(display, sym);
    if (code == 0) {
        sym = XK_Pointer_EnableKeys;
        code = XKeysymToKeycode(display, sym);
        if (code == 0)
            return QString(); // No shortcut available?
    }

    // Calculate the modifiers by searching the keysym in the X keyboard mapping
    XkbDescPtr xkbdesc = XkbGetMap(display, XkbKeyTypesMask | XkbKeySymsMask, XkbUseCoreKbd);
    if (!xkbdesc)
        return QString(); // Failed to obtain the mapping from server

    bool found = false;
    unsigned char modifiers = 0;
    int groups = XkbKeyNumGroups(xkbdesc, code);
    for (int grp = 0; grp < groups && !found; grp++) {
        int levels = XkbKeyGroupWidth(xkbdesc, code, grp);
        for (int level = 0; level < levels && !found; level++) {
            if (sym == XkbKeySymEntry(xkbdesc, code, level, grp)) {
                // keysym found => determine modifiers
                int typeIdx = xkbdesc->map->key_sym_map[code].kt_index[grp];
                XkbKeyTypePtr type = &(xkbdesc->map->types[typeIdx]);
                for (int i = 0; i < type->map_count && !found; i++) {
                    if (type->map[i].active && (type->map[i].level == level)) {
                        modifiers = type->map[i].mods.mask;
                        found = true;
                    }
                }
            }
        }
    }
    XkbFreeClientMap(xkbdesc, 0, true);

    if (!found)
        return QString(); // Somehow the keycode -> keysym mapping is flawed

    XEvent ev;
    ev.type = KeyPress;
    ev.xkey.display = display;
    ev.xkey.keycode = code;
    ev.xkey.state = 0;
    int key;
    KKeyServer::xEventToQt(&ev, &key);
    QString keyname = QKeySequence(key).toString();

    unsigned int AltMask   = KKeyServer::modXAlt();
    unsigned int WinMask   = KKeyServer::modXMeta();
    unsigned int NumMask   = KKeyServer::modXNumLock();
    unsigned int ScrollMask = KKeyServer::modXScrollLock();

    unsigned int MetaMask  = XkbKeysymToModifiers(display, XK_Meta_L);
    unsigned int SuperMask = XkbKeysymToModifiers(display, XK_Super_L);
    unsigned int HyperMask = XkbKeysymToModifiers(display, XK_Hyper_L);
    unsigned int AltGrMask = XkbKeysymToModifiers(display, XK_Mode_switch)
                             | XkbKeysymToModifiers(display, XK_ISO_Level3_Shift)
                             | XkbKeysymToModifiers(display, XK_ISO_Level3_Latch)
                             | XkbKeysymToModifiers(display, XK_ISO_Level3_Lock);

    unsigned int mods = ShiftMask | ControlMask | AltMask | WinMask
                        | LockMask | NumMask | ScrollMask;

    AltGrMask &= ~mods;
    MetaMask  &= ~(mods | AltGrMask);
    SuperMask &= ~(mods | AltGrMask | MetaMask);
    HyperMask &= ~(mods | AltGrMask | MetaMask | SuperMask);

    if ((modifiers & AltGrMask) != 0)
        keyname = i18n("AltGraph") + '+' + keyname;
    if ((modifiers & HyperMask) != 0)
        keyname = i18n("Hyper") + '+' + keyname;
    if ((modifiers & SuperMask) != 0)
        keyname = i18n("Super") + '+' + keyname;
    if ((modifiers & WinMask) != 0)
        keyname = QKeySequence(Qt::META).toString() + '+' + keyname;
    if ((modifiers & AltMask) != 0)
        keyname = QKeySequence(Qt::ALT).toString() + '+' + keyname;
    if ((modifiers & ControlMask) != 0)
        keyname = QKeySequence(Qt::CTRL).toString() + '+' + keyname;
    if ((modifiers & ShiftMask) != 0)
        keyname = QKeySequence(Qt::SHIFT).toString() + '+' + keyname;

    QString result;
    if ((modifiers & ScrollMask) != 0)
        if ((modifiers & LockMask) != 0)
            if ((modifiers & NumMask) != 0)
                result = i18n("Press %1 while NumLock, CapsLock and ScrollLock are active", keyname);
            else
                result = i18n("Press %1 while CapsLock and ScrollLock are active", keyname);
        else if ((modifiers & NumMask) != 0)
            result = i18n("Press %1 while NumLock and ScrollLock are active", keyname);
        else
            result = i18n("Press %1 while ScrollLock is active", keyname);
    else if ((modifiers & LockMask) != 0)
        if ((modifiers & NumMask) != 0)
            result = i18n("Press %1 while NumLock and CapsLock are active", keyname);
        else
            result = i18n("Press %1 while CapsLock is active", keyname);
    else if ((modifiers & NumMask) != 0)
        result = i18n("Press %1 while NumLock is active", keyname);
    else
        result = i18n("Press %1", keyname);

    return result;
}

KAccessConfig::KAccessConfig(QWidget *parent, const QVariantList& args)
    : KCModule(parent, args)
{
    KAboutData *about =
        new KAboutData(QStringLiteral("kcmaccess"), i18n("KDE Accessibility Tool"), QStringLiteral("1.0"),
                       QString(), KAboutLicense::GPL, i18n("(c) 2000, Matthias Hoelzer-Kluepfel"));

    about->addAuthor(i18n("Matthias Hoelzer-Kluepfel"), i18n("Author") , QStringLiteral("hoelzer@kde.org"));
    setAboutData(about);

    ui.setupUi(this);

    connect(ui.soundButton, &QPushButton::clicked, this, &KAccessConfig::selectSound);
    connect(ui.customBell, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);
    connect(ui.systemBell, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.customBell, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.soundEdit, &QLineEdit::textChanged, this, &KAccessConfig::configChanged);

    connect(ui.invertScreen, &QRadioButton::clicked, this, &KAccessConfig::configChanged);
    connect(ui.flashScreen, &QRadioButton::clicked, this, &KAccessConfig::configChanged);
    connect(ui.visibleBell, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.visibleBell, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);
    connect(ui.colorButton, &KColorButton::clicked, this, &KAccessConfig::changeFlashScreenColor);

    connect(ui.invertScreen, &QRadioButton::clicked, this, &KAccessConfig::invertClicked);
    connect(ui.flashScreen, &QRadioButton::clicked, this, &KAccessConfig::flashClicked);

    connect(ui.duration, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KAccessConfig::configChanged);


    // modifier key settings -------------------------------

    connect(ui.stickyKeys, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.stickyKeysLock, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.stickyKeysAutoOff, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.stickyKeys, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);

    connect(ui.stickyKeysBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.toggleKeysBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.kNotifyModifiers, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.kNotifyModifiers, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);
    connect(ui.kNotifyModifiersButton, &QPushButton::clicked, this, &KAccessConfig::configureKNotify);

    // key filter settings ---------------------------------

    connect(ui.slowKeysDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KAccessConfig::configChanged);
    connect(ui.slowKeys, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.slowKeys, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);

    connect(ui.slowKeysPressBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.slowKeysAcceptBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.slowKeysRejectBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);

    connect(ui.bounceKeysDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KAccessConfig::configChanged);
    connect(ui.bounceKeys, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.bounceKeysRejectBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.bounceKeys, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);

    // gestures --------------------------------------------

    QString shortcut = mouseKeysShortcut(QX11Info::display());
    if (shortcut.isEmpty())
        ui.gestures->setToolTip(i18n("Here you can activate keyboard gestures that turn on the following features: \n"
                                    "Sticky keys: Press Shift key 5 consecutive times\n"
                                    "Slow keys: Hold down Shift for 8 seconds"));
    else
        ui.gestures->setToolTip(i18n("Here you can activate keyboard gestures that turn on the following features: \n"
                                    "Mouse Keys: %1\n"
                                    "Sticky keys: Press Shift key 5 consecutive times\n"
                                    "Slow keys: Hold down Shift for 8 seconds", shortcut));

    connect(ui.gestures, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.timeout, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.timeout, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);
    connect(ui.timeoutDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KAccessConfig::configChanged);
    connect(ui.accessxBeep, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.gestureConfirmation, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.kNotifyAccess, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
    connect(ui.kNotifyAccess, &QCheckBox::clicked, this, &KAccessConfig::checkAccess);
    connect(ui.kNotifyAccessButton, &QPushButton::clicked, this, &KAccessConfig::configureKNotify);

    // screen reader
    connect(ui.screenReaderEnabled, &QCheckBox::clicked, this, &KAccessConfig::configChanged);
}


KAccessConfig::~KAccessConfig()
{
}

void KAccessConfig::configureKNotify()
{
    KNotifyConfigWidget::configure(this, QStringLiteral("kaccess"));
}

void KAccessConfig::changeFlashScreenColor()
{
    ui.invertScreen->setChecked(false);
    ui.flashScreen->setChecked(true);
    configChanged();
}

void KAccessConfig::selectSound()
{
    QStringList list = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("sound/"));
    QString start;
    if (list.count() > 0)
        start = list[0];
    QString fname = QFileDialog::getOpenFileName(this, QString(), start);
    if (!fname.isEmpty())
        ui.soundEdit->setText(fname);
}


void KAccessConfig::configChanged()
{
    emit changed(true);
}


void KAccessConfig::load()
{
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "Bell");

    ui.systemBell->setChecked(cg.readEntry("SystemBell", true));
    ui.customBell->setChecked(cg.readEntry("ArtsBell", false));
    ui.soundEdit->setText(cg.readPathEntry("ArtsBellFile", QString()));

    ui.visibleBell->setChecked(cg.readEntry("VisibleBell", false));
    ui.invertScreen->setChecked(cg.readEntry("VisibleBellInvert", true));
    ui.flashScreen->setChecked(!ui.invertScreen->isChecked());
    ui.colorButton->setColor(cg.readEntry("VisibleBellColor", QColor(Qt::red)));

    ui.duration->setValue(cg.readEntry("VisibleBellPause", 500));

    KConfigGroup keyboardGroup(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "Keyboard");

    ui.stickyKeys->setChecked(keyboardGroup.readEntry("StickyKeys", false));
    ui.stickyKeysLock->setChecked(keyboardGroup.readEntry("StickyKeysLatch", true));
    ui.stickyKeysAutoOff->setChecked(keyboardGroup.readEntry("StickyKeysAutoOff", false));
    ui.stickyKeysBeep->setChecked(keyboardGroup.readEntry("StickyKeysBeep", true));
    ui.toggleKeysBeep->setChecked(keyboardGroup.readEntry("ToggleKeysBeep", false));
    ui.kNotifyModifiers->setChecked(keyboardGroup.readEntry("kNotifyModifiers", false));

    ui.slowKeys->setChecked(keyboardGroup.readEntry("SlowKeys", false));
    ui.slowKeysDelay->setValue(keyboardGroup.readEntry("SlowKeysDelay", 500));
    ui.slowKeysPressBeep->setChecked(keyboardGroup.readEntry("SlowKeysPressBeep", true));
    ui.slowKeysAcceptBeep->setChecked(keyboardGroup.readEntry("SlowKeysAcceptBeep", true));
    ui.slowKeysRejectBeep->setChecked(keyboardGroup.readEntry("SlowKeysRejectBeep", true));

    ui.bounceKeys->setChecked(keyboardGroup.readEntry("BounceKeys", false));
    ui.bounceKeysDelay->setValue(keyboardGroup.readEntry("BounceKeysDelay", 500));
    ui.bounceKeysRejectBeep->setChecked(keyboardGroup.readEntry("BounceKeysRejectBeep", true));

    ui.gestures->setChecked(keyboardGroup.readEntry("Gestures", false));
    ui.timeout->setChecked(keyboardGroup.readEntry("AccessXTimeout", false));
    ui.timeoutDelay->setValue(keyboardGroup.readEntry("AccessXTimeoutDelay", 30));

    ui.accessxBeep->setChecked(keyboardGroup.readEntry("AccessXBeep", true));
    ui.gestureConfirmation->setChecked(keyboardGroup.readEntry("GestureConfirmation", false));
    ui.kNotifyAccess->setChecked(keyboardGroup.readEntry("kNotifyAccess", false));

    KConfigGroup screenReaderGroup(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "ScreenReader");
    ui.screenReaderEnabled->setChecked(screenReaderGroup.readEntry("Enabled", false));

    checkAccess();

    emit changed(false);
}


void KAccessConfig::save()
{
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "Bell");

    cg.writeEntry("SystemBell", ui.systemBell->isChecked());
    cg.writeEntry("ArtsBell", ui.customBell->isChecked());
    cg.writePathEntry("ArtsBellFile", ui.soundEdit->text());

    cg.writeEntry("VisibleBell", ui.visibleBell->isChecked());
    cg.writeEntry("VisibleBellInvert", ui.invertScreen->isChecked());
    cg.writeEntry("VisibleBellColor", ui.colorButton->color());

    cg.writeEntry("VisibleBellPause", ui.duration->value());
    cg.sync();

    KConfigGroup keyboardGroup(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "Keyboard");

    keyboardGroup.writeEntry("StickyKeys", ui.stickyKeys->isChecked());
    keyboardGroup.writeEntry("StickyKeysLatch", ui.stickyKeysLock->isChecked());
    keyboardGroup.writeEntry("StickyKeysAutoOff", ui.stickyKeysAutoOff->isChecked());
    keyboardGroup.writeEntry("StickyKeysBeep", ui.stickyKeysBeep->isChecked());
    keyboardGroup.writeEntry("ToggleKeysBeep", ui.toggleKeysBeep->isChecked());
    keyboardGroup.writeEntry("kNotifyModifiers", ui.kNotifyModifiers->isChecked());

    keyboardGroup.writeEntry("SlowKeys", ui.slowKeys->isChecked());
    keyboardGroup.writeEntry("SlowKeysDelay", ui.slowKeysDelay->value());
    keyboardGroup.writeEntry("SlowKeysPressBeep", ui.slowKeysPressBeep->isChecked());
    keyboardGroup.writeEntry("SlowKeysAcceptBeep", ui.slowKeysAcceptBeep->isChecked());
    keyboardGroup.writeEntry("SlowKeysRejectBeep", ui.slowKeysRejectBeep->isChecked());


    keyboardGroup.writeEntry("BounceKeys", ui.bounceKeys->isChecked());
    keyboardGroup.writeEntry("BounceKeysDelay", ui.bounceKeysDelay->value());
    keyboardGroup.writeEntry("BounceKeysRejectBeep", ui.bounceKeysRejectBeep->isChecked());

    keyboardGroup.writeEntry("Gestures", ui.gestures->isChecked());
    keyboardGroup.writeEntry("AccessXTimeout", ui.timeout->isChecked());
    keyboardGroup.writeEntry("AccessXTimeoutDelay", ui.timeoutDelay->value());

    keyboardGroup.writeEntry("AccessXBeep", ui.accessxBeep->isChecked());
    keyboardGroup.writeEntry("GestureConfirmation", ui.gestureConfirmation->isChecked());
    keyboardGroup.writeEntry("kNotifyAccess", ui.kNotifyAccess->isChecked());


    keyboardGroup.sync();

    KConfigGroup screenReaderGroup(KSharedConfig::openConfig(QStringLiteral("kaccessrc")), "ScreenReader");
    screenReaderGroup.writeEntry("Enabled", ui.screenReaderEnabled->isChecked());

    if (ui.systemBell->isChecked() ||
        ui.customBell->isChecked() ||
        ui.visibleBell->isChecked()) {
        KConfig _cfg(QStringLiteral("kdeglobals"), KConfig::NoGlobals);
        KConfigGroup cfg(&_cfg, "General");
        cfg.writeEntry("UseSystemBell", true);
        cfg.sync();
    }

    // make kaccess reread the configuration
    // turning a11y features off needs to be done by kaccess
    // so run it to clear any enabled features and it will exit if it should
    KToolInvocation::startServiceByDesktopName(QStringLiteral("kaccess"));

    emit changed(false);
}


void KAccessConfig::defaults()
{
    ui.systemBell->setChecked(true);
    ui.customBell->setChecked(false);
    ui.soundEdit->setText(QString());

    ui.visibleBell->setChecked(false);
    ui.invertScreen->setChecked(true);
    ui.flashScreen->setChecked(false);
    ui.colorButton->setColor(QColor(Qt::red));

    ui.duration->setValue(500);

    ui.slowKeys->setChecked(false);
    ui.slowKeysDelay->setValue(500);
    ui.slowKeysPressBeep->setChecked(true);
    ui.slowKeysAcceptBeep->setChecked(true);
    ui.slowKeysRejectBeep->setChecked(true);

    ui.bounceKeys->setChecked(false);
    ui.bounceKeysDelay->setValue(500);
    ui.bounceKeysRejectBeep->setChecked(true);

    ui.stickyKeys->setChecked(false);
    ui.stickyKeysLock->setChecked(true);
    ui.stickyKeysAutoOff->setChecked(false);
    ui.stickyKeysBeep->setChecked(true);
    ui.toggleKeysBeep->setChecked(false);
    ui.kNotifyModifiers->setChecked(false);

    ui.gestures->setChecked(false);
    ui.timeout->setChecked(false);
    ui.timeoutDelay->setValue(30);

    ui.accessxBeep->setChecked(true);
    ui.gestureConfirmation->setChecked(true);
    ui.kNotifyAccess->setChecked(false);

    ui.screenReaderEnabled->setChecked(false);

    checkAccess();

    emit changed(true);
}


void KAccessConfig::invertClicked()
{
    ui.flashScreen->setChecked(false);
}


void KAccessConfig::flashClicked()
{
    ui.invertScreen->setChecked(false);
}


void KAccessConfig::checkAccess()
{
    bool custom = ui.customBell->isChecked();
    ui.soundEdit->setEnabled(custom);
    ui.soundButton->setEnabled(custom);
    ui.soundLabel->setEnabled(custom);

    bool visible = ui.visibleBell->isChecked();
    ui.invertScreen->setEnabled(visible);
    ui.flashScreen->setEnabled(visible);
    ui.colorButton->setEnabled(visible);
    ui.duration->setEnabled(visible);

    bool sticky = ui.stickyKeys->isChecked();
    ui.stickyKeysLock->setEnabled(sticky);
    ui.stickyKeysAutoOff->setEnabled(sticky);
    ui.stickyKeysBeep->setEnabled(sticky);

    bool slow = ui.slowKeys->isChecked();
    ui.slowKeysDelay->setEnabled(slow);
    ui.slowKeysPressBeep->setEnabled(slow);
    ui.slowKeysAcceptBeep->setEnabled(slow);
    ui.slowKeysRejectBeep->setEnabled(slow);

    bool bounce = ui.bounceKeys->isChecked();
    ui.bounceKeysDelay->setEnabled(bounce);
    ui.bounceKeysRejectBeep->setEnabled(bounce);

    bool useTimeout = ui.timeout->isChecked();
    ui.timeoutDelay->setEnabled(useTimeout);
}

extern "C"
{
    /* This one gets called by kcminit

     */
    Q_DECL_EXPORT void kcminit_access()
    {
        KConfig config(QStringLiteral("kaccessrc"), KConfig::NoGlobals);
        KToolInvocation::startServiceByDesktopName(QStringLiteral("kaccess"));
    }
}

#include "kcmaccess.moc"

