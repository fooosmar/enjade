/**************************************************************************
*   Copyright (C) 2009-2010 Trever Fischer <tdfischer@fedoraproject.org>  *
*   Copyright (C) 2015 Kai UWe Broulik <kde@privat.broulik.de>            *
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

#include "DeviceAutomounterKCM.h"

#include <QStandardItem>
#include <QStandardItemModel>
#include <QItemSelectionModel>

#include <KAboutData>
#include <KConfigGroup>
#include <Solid/DeviceNotifier>
#include <Solid/StorageVolume>

#include <KPluginFactory>

#include "AutomounterSettings.h"
#include "LayoutSettings.h"
#include "DeviceModel.h"

K_PLUGIN_FACTORY(DeviceAutomounterKCMFactory, registerPlugin<DeviceAutomounterKCM>();)
K_EXPORT_PLUGIN(DeviceAutomounterKCMFactory("kcm_device_automounter"))

DeviceAutomounterKCM::DeviceAutomounterKCM(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)//DeviceAutomounterKCMFactory::componentData(), parent)
{
    KAboutData *about = new KAboutData("kcm_device_automounter",
                                       i18n("Device Automounter"),
                                       "2.0",
                                       QString(),
                                       KAboutLicense::GPL_V2,
                                       i18n("(c) 2009 Trever Fischer, (c) 2015 Kai Uwe Broulik"));
    about->addAuthor(i18n("Trever Fischer"), i18n("Original Author"));
    about->addAuthor(i18n("Kai Uwe Broulik"), i18n("Plasma 5 Port"), "kde@privat.broulik.de");

    setAboutData(about);
    setupUi(this);

    m_devices = new DeviceModel(this);
    deviceView->setModel(m_devices);

    auto emitChanged = [this] {
        emit changed();
    };

    connect(automountOnLogin, &QCheckBox::stateChanged, this, emitChanged);
    connect(automountOnPlugin, &QCheckBox::stateChanged, this, emitChanged);
    connect(automountEnabled, &QCheckBox::stateChanged, this, emitChanged);
    connect(automountUnknownDevices, &QCheckBox::stateChanged, this, emitChanged);
    connect(m_devices, &DeviceModel::dataChanged, this, emitChanged);

    connect(automountEnabled, &QCheckBox::stateChanged, this, &DeviceAutomounterKCM::enabledChanged);

    connect(deviceView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DeviceAutomounterKCM::updateForgetDeviceButton);

    connect(forgetDevice, &QAbstractButton::clicked, this, &DeviceAutomounterKCM::forgetSelectedDevices);

    forgetDevice->setEnabled(false);
}

DeviceAutomounterKCM::~DeviceAutomounterKCM()
{
    saveLayout();
}

void DeviceAutomounterKCM::updateForgetDeviceButton()
{
    foreach (const QModelIndex &idx, deviceView->selectionModel()->selectedIndexes()) {
		if (idx.data(DeviceModel::TypeRole) == DeviceModel::Detatched) {
			forgetDevice->setEnabled(true);
			return;
		}
	}
	forgetDevice->setEnabled(false);
}

void DeviceAutomounterKCM::forgetSelectedDevices()
{
    QItemSelectionModel *selected = deviceView->selectionModel();
	int offset = 0;
    while (!selected->selectedIndexes().isEmpty() && selected->selectedIndexes().size() > offset) {
        if (selected->selectedIndexes()[offset].data(DeviceModel::TypeRole) == DeviceModel::Attached) {
			offset++;
        } else {
            m_devices->forgetDevice(selected->selectedIndexes()[offset].data(DeviceModel::UdiRole).toString());
        }
    }
    changed();
}

void DeviceAutomounterKCM::enabledChanged()
{
    automountOnLogin->setEnabled(automountEnabled->isChecked());
    automountOnPlugin->setEnabled(automountEnabled->isChecked());
    automountUnknownDevices->setEnabled(automountEnabled->isChecked());
    deviceView->setEnabled(automountEnabled->isChecked());
}

void DeviceAutomounterKCM::load()
{
    automountEnabled->setChecked(AutomounterSettings::automountEnabled());
    automountUnknownDevices->setChecked(AutomounterSettings::automountUnknownDevices());
    automountOnLogin->setChecked(AutomounterSettings::automountOnLogin());
    automountOnPlugin->setChecked(AutomounterSettings::automountOnPlugin());

    m_devices->reload();
    enabledChanged();
    loadLayout();
}

void DeviceAutomounterKCM::save()
{
    saveLayout();

    AutomounterSettings::setAutomountEnabled(automountEnabled->isChecked());
    AutomounterSettings::setAutomountUnknownDevices(automountUnknownDevices->isChecked());
    AutomounterSettings::setAutomountOnLogin(automountOnLogin->isChecked());
    AutomounterSettings::setAutomountOnPlugin(automountOnPlugin->isChecked());

    QStringList validDevices;
    for (int i = 0; i < m_devices->rowCount(); ++i) {
        const QModelIndex &idx = m_devices->index(i, 0);

        for (int j = 0; j < m_devices->rowCount(idx); ++j) {
            QModelIndex dev = m_devices->index(j, 1, idx);
            const QString device = dev.data(DeviceModel::UdiRole).toString();
            validDevices << device;

            if (dev.data(Qt::CheckStateRole).toInt() == Qt::Checked) {
                AutomounterSettings::deviceSettings(device).writeEntry("ForceLoginAutomount", true);
            } else {
                AutomounterSettings::deviceSettings(device).writeEntry("ForceLoginAutomount", false);
            }

            dev = dev.sibling(j, 2);

            if (dev.data(Qt::CheckStateRole).toInt() == Qt::Checked) {
                AutomounterSettings::deviceSettings(device).writeEntry("ForceAttachAutomount", true);
            } else {
                AutomounterSettings::deviceSettings(device).writeEntry("ForceAttachAutomount", false);
            }
        }
    }

    foreach (const QString &possibleDevice, AutomounterSettings::knownDevices()) {
        if (!validDevices.contains(possibleDevice)) {
            AutomounterSettings::deviceSettings(possibleDevice).deleteGroup();
        }
    }

    AutomounterSettings::self()->save();
}

void DeviceAutomounterKCM::saveLayout()
{
    QList<int> widths;
    const int nbColumn = m_devices->columnCount();
    widths.reserve(nbColumn);

    for (int i = 0; i < nbColumn; ++i) {
        widths << deviceView->columnWidth(i);
    }

    LayoutSettings::setHeaderWidths(widths);
    //Check DeviceModel.cpp, thats where the magic row numbers come from.
    LayoutSettings::setAttachedExpanded(deviceView->isExpanded(m_devices->index(0,0)));
    LayoutSettings::setDetatchedExpanded(deviceView->isExpanded(m_devices->index(1,0)));
    LayoutSettings::self()->save();
}

void DeviceAutomounterKCM::loadLayout()
{
    LayoutSettings::self()->load();
    //Reset it first, just in case there isn't any layout saved for a particular column.
    int nbColumn = m_devices->columnCount();
    for (int i = 0; i < nbColumn; ++i) {
        deviceView->resizeColumnToContents(i);
    }

    QList<int> widths = LayoutSettings::headerWidths();
    nbColumn = m_devices->columnCount();
    for (int i = 0; i < nbColumn && i < widths.size(); ++i) {
        deviceView->setColumnWidth(i, widths[i]);
    }

    deviceView->setExpanded(m_devices->index(0,0), LayoutSettings::attachedExpanded());
    deviceView->setExpanded(m_devices->index(1,0), LayoutSettings::detatchedExpanded());
}

#include "DeviceAutomounterKCM.moc"
