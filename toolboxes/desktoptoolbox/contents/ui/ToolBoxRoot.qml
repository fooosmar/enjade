/*
 *   Copyright 2011 Sebastian Kügler <sebas@kde.org>
 *   Copyright 2011 Marco Martin <mart@kde.org>
 *   Copyright 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.2
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.kquickcontrolsaddons 2.0 as KQuickControlsAddons
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.plasmoid 2.0


Item {
    id: main
    objectName: "org.kde.desktoptoolbox"

    z: 999
    anchors.fill: parent

    Connections {
        target: plasmoid
        onAvailableScreenRegionChanged: placeToolBoxTimer.restart();
    }

    signal minimumWidthChanged
    signal minimumHeightChanged
    signal maximumWidthChanged
    signal maximumHeightChanged
    signal preferredWidthChanged
    signal preferredHeightChanged

    property int iconSize: units.iconSizes.small
    property int iconWidth: units.iconSizes.smallMedium
    property int iconHeight: iconWidth
    property bool dialogWasVisible: false

    onWidthChanged: placeToolBoxTimer.restart();
    onHeightChanged: placeToolBoxTimer.restart();

    LayoutMirroring.enabled: (Qt.application.layoutDirection === Qt.RightToLeft)
    LayoutMirroring.childrenInherit: true

    onVisibleChanged: {
        if (!visible && toolBoxLoader.item) {
            toolBoxLoader.item.visible = false
        }
    }

    ToolBoxButton {
        id: toolBoxButton
        visible: false
        Component.onCompleted: {
            placeToolBox(plasmoid.configuration.ToolBoxButtonState);
            toolBoxButton.visible = true
        }

        Behavior on x {
            XAnimator { duration: units.longDuration; easing.type: Easing.InOutQuad }
            // avoid the toolbox sliding over the screen on startup
            // placeToolBox is initially called before the toolBoxButton is visible
            // so those Behaviors won't fire on startup
            enabled: toolBoxButton.visible
        }
        Behavior on y {
            YAnimator { duration: units.longDuration; easing.type: Easing.InOutQuad }
            enabled: toolBoxButton.visible
        }
    }

    Timer {
        id: placeToolBoxTimer
        interval: 100
        repeat: false
        running: false
        onTriggered: {
            placeToolBox(plasmoid.configuration.ToolBoxButtonState);
        }
    }

    Loader {
        id: toolBoxLoader
        active: false
        sourceComponent: PlasmaCore.Dialog {
            flags: Qt.WindowStaysOnTopHint
            location: PlasmaCore.Types.Floating
            visualParent: toolBoxButton
            hideOnWindowDeactivate: true
            mainItem: ToolBoxItem {
                id: dialog

                LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
                LayoutMirroring.childrenInherit: true

                Timer {
                    id: visibleTimer
                    interval: 300
                    onTriggered: main.dialogWasVisible = dialog.visible
                }
            }
            onVisibleChanged: visibleTimer.restart();
        }
    }

    function placeToolBox(ts) {
        var tx = Plasmoid.configuration.ToolBoxButtonX
        var ty = Plasmoid.configuration.ToolBoxButtonY
        var pos;

        switch (ts) {
        case "top":
            ty = main.y;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.width, toolBoxButton.height);
            break;
        case "left":
            tx = main.x;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.width, toolBoxButton.height);
            break;
        case "right":
            tx = main.width + main.x - toolBoxButton.width;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.width, toolBoxButton.height);
            break;
        case "bottom":
            ty = main.height + main.y - toolBoxButton.height;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.width, toolBoxButton.height);
            break;
        case "topleft":
            tx = main.x;
            ty = main.y;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.height, toolBoxButton.height);
            break;
        case "topright":
            tx = main.width + main.x - toolBoxButton.height;
            ty = main.y;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.height, toolBoxButton.height);
            break;
        case "bottomleft":
            tx = main.x;
            ty = main.height + main.y - toolBoxButton.height;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.height, toolBoxButton.height);
            break;
        case "bottomright":
        default:
            tx = main.width + main.x - toolBoxButton.height;
            ty = main.height + main.y - toolBoxButton.height;
            pos = plasmoid.adjustToAvailableScreenRegion(tx, ty, toolBoxButton.height, toolBoxButton.height);
            break;
        }
        //print("XXXY Setting toolbox to: " + ts + " " + tx + "x" + ty + " screen: " + main.width+ "x" + main.height+"");

        toolBoxButton.x = pos.x;
        toolBoxButton.y = pos.y;
    }
}
