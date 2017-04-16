/***************************************************************************
*   Copyright (C) 2009 by Trever Fischer <wm161@wm161.net>                *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
***************************************************************************/

#include "DeviceAutomounter.h"

#include <KPluginFactory>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <QTimer>

K_PLUGIN_FACTORY_WITH_JSON(DeviceAutomounterFactory,
                           "device_automounter.json",
                           registerPlugin<DeviceAutomounter>();)

DeviceAutomounter::DeviceAutomounter(QObject *parent, const QVariantList &args)
    : KDEDModule(parent)
{
    Q_UNUSED(args);
    QTimer::singleShot(0, this, &DeviceAutomounter::init);
}

DeviceAutomounter::~DeviceAutomounter()
{
}

void DeviceAutomounter::init()
{
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, &DeviceAutomounter::deviceAdded);
    QList<Solid::Device> volumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    foreach(Solid::Device volume, volumes) {
        // sa can be 0 (e.g. for the swap partition)
        if (Solid::StorageAccess *sa = volume.as<Solid::StorageAccess>()) {
            connect(sa, &Solid::StorageAccess::accessibilityChanged, this, &DeviceAutomounter::deviceMountChanged);
        }
        automountDevice(volume, AutomounterSettings::Login);
    }
    AutomounterSettings::self()->save();
}

void DeviceAutomounter::deviceMountChanged(bool accessible, const QString &udi)
{
    AutomounterSettings::setDeviceLastSeenMounted(udi, accessible);
    AutomounterSettings::self()->save();
}

void DeviceAutomounter::automountDevice(Solid::Device &dev, AutomounterSettings::AutomountType type)
{
    if (dev.is<Solid::StorageVolume>() && dev.is<Solid::StorageAccess>()) {
        Solid::StorageAccess *sa = dev.as<Solid::StorageAccess>();
        if (sa->isIgnored()) {
            return;
        }

        AutomounterSettings::setDeviceLastSeenMounted(dev.udi(), sa->isAccessible());
        AutomounterSettings::saveDevice(dev);

        if (AutomounterSettings::shouldAutomountDevice(dev.udi(), type)) {
            Solid::StorageVolume *sv = dev.as<Solid::StorageVolume>();
            if (!sv->isIgnored()) {
                sa->setup();
            }
        }
    }
}

void DeviceAutomounter::deviceAdded(const QString &udi)
{
    AutomounterSettings::self()->load();

    Solid::Device dev(udi);
    automountDevice(dev, AutomounterSettings::Attach);
    AutomounterSettings::self()->save();

    if (dev.is<Solid::StorageAccess>()) {
        Solid::StorageAccess *sa = dev.as<Solid::StorageAccess>();
        if (sa) {
            connect(sa, &Solid::StorageAccess::accessibilityChanged, this, &DeviceAutomounter::deviceMountChanged);
        }
    }
}

#include "DeviceAutomounter.moc"
