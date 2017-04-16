/*
 *  Copyright 2013 Marco Martin <mart@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  2.010-1301, USA.
 */

import QtQuick 2.0
import org.kde.plasma.configuration 2.0
import QtQuick.Controls 1.0 as QtControls
import QtQuick.Layouts 1.1

ColumnLayout {
    id: root

    property int formAlignment: wallpaperComboBox.x + (units.largeSpacing/2)
    property string currentWallpaper: ""
    property string containmentPlugin: ""
    signal configurationChanged

//BEGIN functions
    function saveConfig() {
        if (main.currentItem.saveConfig) {
            main.currentItem.saveConfig()
        }
        for (var key in configDialog.wallpaperConfiguration) {
            if (main.currentItem["cfg_"+key] !== undefined) {
                configDialog.wallpaperConfiguration[key] = main.currentItem["cfg_"+key]
            }
        }
        configDialog.currentWallpaper = root.currentWallpaper;
        configDialog.applyWallpaper()
        configDialog.containmentPlugin = root.containmentPlugin
    }
//END functions

    Component.onCompleted: {
        for (var i = 0; i < configDialog.containmentPluginsConfigModel.count; ++i) {
            var data = configDialog.containmentPluginsConfigModel.get(i);
            if (configDialog.containmentPlugin == data.pluginName) {
                pluginComboBox.currentIndex = i
                break;
            }
        }

        for (var i = 0; i < configDialog.wallpaperConfigModel.count; ++i) {
            var data = configDialog.wallpaperConfigModel.get(i);
            if (configDialog.currentWallpaper == data.pluginName) {
                wallpaperComboBox.currentIndex = i
                break;
            }
        }
    }

    Row {
        spacing: units.largeSpacing / 2
        anchors.right: wallpaperRow.right
        Item {
            width: units.largeSpacing
            height: parent.height
        }
        QtControls.Label {
            anchors.verticalCenter: pluginComboBox.verticalCenter
            text: i18nd("plasma_shell_org.kde.plasma.desktop", "Layout:")
        }
        QtControls.ComboBox {
            id: pluginComboBox
            enabled: !plasmoid.immutable
            model: configDialog.containmentPluginsConfigModel
            width: theme.mSize(theme.defaultFont).width * 24
            textRole: "name"
            onCurrentIndexChanged: {
                var model = configDialog.containmentPluginsConfigModel.get(currentIndex)
                root.containmentPlugin = model.pluginName
                root.configurationChanged()
            }
        }
    }

    ColumnLayout {
        id: switchContainmentWarning
        Layout.fillWidth: true
        visible: configDialog.containmentPlugin != root.containmentPlugin
        QtControls.Label {
            Layout.fillWidth: true
            text: i18nd("plasma_shell_org.kde.plasma.desktop", "Layout changes must be applied before other changes can be made")
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        QtControls.Button {
            Layout.alignment: Qt.AlignHCenter
            text: i18nd("plasma_shell_org.kde.plasma.desktop", "Apply now")
            onClicked: saveConfig()
        }

        Binding {
            target: categoriesScroll //from parent scope AppletConfiguration
            property: "enabled"
            value: !switchContainmentWarning.visible
        }
        Item {
            Layout.fillHeight: true
        }
    }

    QtControls.Label {
        Layout.fillWidth: true

        visible: plasmoid.immutable

        text: i18nd("plasma_shell_org.kde.plasma.desktop", "Layout cannot be changed whilst widgets are locked")
        wrapMode: Text.Wrap
    }

    Row {
        visible: !switchContainmentWarning.visible
        id: wallpaperRow
        spacing: units.largeSpacing / 2
        Item {
            width: units.largeSpacing
            height: parent.height
        }
        QtControls.Label {
            anchors.verticalCenter: wallpaperComboBox.verticalCenter
            text: i18nd("plasma_shell_org.kde.plasma.desktop", "Wallpaper Type:")
        }
        QtControls.ComboBox {
            id: wallpaperComboBox
            model: configDialog.wallpaperConfigModel
            width: theme.mSize(theme.defaultFont).width * 24
            textRole: "name"
            onCurrentIndexChanged: {
                var model = configDialog.wallpaperConfigModel.get(currentIndex)
                root.currentWallpaper = model.pluginName
                main.sourceFile = model.source
                configDialog.currentWallpaper = model.pluginName
                root.configurationChanged()
            }
        }
    }

    Item {
        id: emptyConfig
    }

    QtControls.StackView {
        id: main

        Layout.fillHeight: true;
        anchors {
            left: parent.left;
            right: parent.right;
        }
        visible: !switchContainmentWarning.visible

        // Bug 360862: if wallpaper has no config, sourceFile will be ""
        // so we wouldn't load emptyConfig and break all over the place
        // hence set it to some random value initially
        property string sourceFile: "tbd"
        onSourceFileChanged: {
            if (sourceFile) {
                var props = {}

                var wallpaperConfig = configDialog.wallpaperConfiguration
                for (var key in wallpaperConfig) {
                    props["cfg_" + key] = wallpaperConfig[key]
                }

                var newItem = push({
                    item: Qt.resolvedUrl(sourceFile),
                    replace: true,
                    properties: props
                })

                for (var key in wallpaperConfig) {
                    var changedSignal = newItem["cfg_" + key + "Changed"]
                    if (changedSignal) {
                        changedSignal.connect(root.configurationChanged)
                    }
                }
            } else {
                replace(emptyConfig)
            }
        }
    }
}
