/***************************************************************************
 *   Copyright (C) 2012-2016 by Eike Hein <hein@kde.org>                   *
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

#include "backend.h"

#include <KConfigGroup>
#include <KDesktopFile>
#include <KFileItem>
#include <KLocalizedString>
#include <KRun>
#include <KService>
#include <kwindoweffects.h>
#include <KWindowSystem>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QJsonArray>
#include <QQuickItem>
#include <QQuickWindow>

#include <KActivities/Consumer>
#include <KActivities/Stats/Cleaning>
#include <KActivities/Stats/ResultSet>
#include <KActivities/Stats/Terms>

namespace KAStats = KActivities::Stats;

using namespace KAStats;
using namespace KAStats::Terms;

Backend::Backend(QObject* parent) : QObject(parent)
    , m_taskManagerItem(0)
    , m_toolTipItem(0)
    , m_panelWinId(0)
    , m_highlightWindows(false)
    , m_actionGroup(new QActionGroup(this))
{
}

Backend::~Backend()
{
}

QQuickItem *Backend::taskManagerItem() const
{
    return m_taskManagerItem;
}

void Backend::setTaskManagerItem(QQuickItem* item)
{
    if (item != m_taskManagerItem) {
        m_taskManagerItem = item;

        emit taskManagerItemChanged();
    }
}

QQuickItem *Backend::toolTipItem() const
{
    return m_toolTipItem;
}

void Backend::setToolTipItem(QQuickItem *item)
{
    if (item != m_toolTipItem) {
        m_toolTipItem = item;

        connect(item, SIGNAL(windowChanged(QQuickWindow*)), this, SLOT(toolTipWindowChanged(QQuickWindow*)));

        emit toolTipItemChanged();
    }
}

QQuickWindow *Backend::groupDialog() const
{
    return m_groupDialog;
}

void Backend::setGroupDialog(QQuickWindow *dialog)
{
    if (dialog != m_groupDialog) {
        m_groupDialog = dialog;

        emit groupDialogChanged();
    }
}

bool Backend::highlightWindows() const
{
    return m_highlightWindows;
}

void Backend::setHighlightWindows(bool highlight)
{
    if (highlight != m_highlightWindows) {
        m_highlightWindows = highlight;

        updateWindowHighlight();

        emit highlightWindowsChanged();
    }
}

QVariantList Backend::jumpListActions(const QUrl &launcherUrl, QObject *parent)
{
    QVariantList actions;

    if (!parent || !launcherUrl.isValid() || !launcherUrl.isLocalFile()
        || !KDesktopFile::isDesktopFile(launcherUrl.toLocalFile())) {
        return actions;
    }

    KDesktopFile desktopFile(launcherUrl.toLocalFile());

    const QStringList &jumpListActions = desktopFile.readActions();

    const QLatin1String kde("KDE");

    foreach (const QString &actionName, jumpListActions) {
        const KConfigGroup &actionGroup = desktopFile.actionGroup(actionName);

        if (!actionGroup.isValid() || !actionGroup.exists()) {
            continue;
        }

        const QStringList &notShowIn = actionGroup.readXdgListEntry(QStringLiteral("NotShowIn"));
        if (notShowIn.contains(kde)) {
            continue;
        }

        const QStringList &onlyShowIn = actionGroup.readXdgListEntry(QStringLiteral("OnlyShowIn"));
        if (!onlyShowIn.isEmpty() && !onlyShowIn.contains(kde)) {
            continue;
        }

        const QString &name = actionGroup.readEntry(QStringLiteral("Name"));
        const QString &exec = actionGroup.readEntry(QStringLiteral("Exec"));
        if (name.isEmpty() || exec.isEmpty()) {
            continue;
        }

        QAction *action = new QAction(parent);
        action->setText(name);
        action->setIcon(QIcon::fromTheme(actionGroup.readEntry("Icon")));
        action->setProperty("exec", exec);
        // so we can show the proper application name and icon when it launches
        action->setProperty("applicationName", desktopFile.readName());
        action->setProperty("applicationIcon", desktopFile.readIcon());
        connect(action, &QAction::triggered, this, &Backend::handleJumpListAction);

        actions << QVariant::fromValue<QAction *>(action);
    }

    return actions;
}

QVariantList Backend::recentDocumentActions(const QUrl &launcherUrl, QObject *parent)
{
    QVariantList actions;

    if (!parent || !launcherUrl.isValid() || !launcherUrl.isLocalFile()
        || !KDesktopFile::isDesktopFile(launcherUrl.toLocalFile())) {
        return actions;
    }

    QString desktopName = launcherUrl.fileName();
    QString storageId = desktopName;

    if (storageId.startsWith(QLatin1String("org.kde."))) {
        storageId = storageId.right(storageId.length() - 8);
    }

    if (storageId.endsWith(QLatin1String(".desktop"))) {
        storageId = storageId.left(storageId.length() - 8);
    }

    auto query = UsedResources
        | RecentlyUsedFirst
        | Agent(storageId)
        | Type::any()
        | Activity::current()
        | Url::file();

    ResultSet results(query);

    ResultSet::const_iterator resultIt = results.begin();

    int actionCount = 0;

    while (actionCount < 5 && resultIt != results.end()) {
        const QString resource = (*resultIt).resource();
        const QUrl url(resource);

        if (!url.isValid()) {
            continue;
        }

        const KFileItem fileItem(url);

        if (!fileItem.isFile()) {
            continue;
        }

        QAction *action = new QAction(parent);
        action->setText(url.fileName());
        action->setIcon(QIcon::fromTheme(fileItem.iconName(), QIcon::fromTheme("unknown")));
        action->setProperty("agent", storageId);
        action->setProperty("entryPath", launcherUrl);
        action->setData(resource);
        connect(action, &QAction::triggered, this, &Backend::handleRecentDocumentAction);

        actions << QVariant::fromValue<QAction *>(action);

        ++resultIt;
        ++actionCount;
    }

    if (actionCount > 0) {
        QAction *action = new QAction(parent);
        action->setText(i18n("Forget Recent Documents"));
        action->setProperty("agent", storageId);
        connect(action, &QAction::triggered, this, &Backend::handleRecentDocumentAction);

        actions << QVariant::fromValue<QAction *>(action);
    }

    return actions;
}

void Backend::toolTipWindowChanged(QQuickWindow *window)
{
    Q_UNUSED(window)

    updateWindowHighlight();
}

void Backend::handleJumpListAction() const
{
    const QAction *action = qobject_cast<QAction* >(sender());

    if (!action) {
        return;
    }

    KRun::run(action->property("exec").toString(), {}, nullptr,
              action->property("applicationName").toString(),
              action->property("applicationIcon").toString());
}

void Backend::handleRecentDocumentAction() const
{
    const QAction *action = qobject_cast<QAction* >(sender());

    if (!action) {
        return;
    }

    const QString agent = action->property("agent").toString();

    if (agent.isEmpty()) {
        return;
    }

    const QString desktopPath = action->property("entryPath").toUrl().toLocalFile();
    const QString resource = action->data().toString();

    if (desktopPath.isEmpty() || resource.isEmpty()) {
        auto query = UsedResources
            | Agent(agent)
            | Type::any()
            | Activity::current()
            | Url::file();

        KAStats::forgetResources(query);

        return;
    }

    KService::Ptr service = KService::serviceByDesktopPath(desktopPath);

        qDebug() << service;

    if (!service) {
        return;
    }

    KRun::runService(*service, QList<QUrl>() << QUrl(resource), QApplication::activeWindow());
}

void Backend::setActionGroup(QAction *action) const
{
    if (action) {
        action->setActionGroup(m_actionGroup);
    }
}

QRect Backend::globalRect(QQuickItem *item) const
{
    if (!item || !item->window()) {
        return QRect();
    }

    QRect iconRect(item->x(), item->y(), item->width(), item->height());
    iconRect.moveTopLeft(item->parentItem()->mapToScene(iconRect.topLeft()).toPoint());
    iconRect.moveTopLeft(item->window()->mapToGlobal(iconRect.topLeft()));

    return iconRect;
}

void Backend::ungrabMouse(QQuickItem *item) const
{
    if (item && item->window() &&  item->window()->mouseGrabberItem()) {
        item->window()->mouseGrabberItem()->ungrabMouse();
    }
}

bool Backend::canPresentWindows() const
{
    return (KWindowSystem::compositingActive() && KWindowEffects::isEffectAvailable(KWindowEffects::PresentWindowsGroup));
}

void Backend::presentWindows(const QVariant &_winIds)
{
    if (!m_taskManagerItem || !m_taskManagerItem->window()) {
        return;
    }

    QList<WId> winIds;

    const QVariantList &_winIdsList = _winIds.toList();

    foreach(const QVariant &_winId, _winIdsList) {
        bool ok = false;
        qlonglong winId = _winId.toLongLong(&ok);

        if (ok) {
            winIds.append(winId);
        }
    }

    if (!winIds.count()) {
        return;
    }

    if (m_windowsToHighlight.count()) {
        m_windowsToHighlight.clear();
        updateWindowHighlight();
    }

    KWindowEffects::presentWindows(m_taskManagerItem->window()->winId(), winIds);
}

bool Backend::isApplication(const QUrl &url) const
{
    if (!url.isValid() || !url.isLocalFile()) {
        return false;
    }

    const QString &localPath = url.toLocalFile();

    if (!KDesktopFile::isDesktopFile(localPath)) {
        return false;
    }

    KDesktopFile desktopFile(localPath);
    return desktopFile.hasApplicationType();
}

QList<QUrl> Backend::jsonArrayToUrlList(const QJsonArray &array) const
{
    QList<QUrl> urls;
    urls.reserve(array.count());

    for (auto it = array.constBegin(), end = array.constEnd(); it != end; ++it) {
        urls << QUrl(it->toString());
    }

    return urls;
}

void Backend::cancelHighlightWindows()
{
    m_windowsToHighlight.clear();
    updateWindowHighlight();
}

void Backend::windowsHovered(const QVariant &_winIds, bool hovered)
{
    m_windowsToHighlight.clear();

    if (hovered) {
        const QVariantList &winIds = _winIds.toList();

        foreach(const QVariant &_winId, winIds) {
            bool ok = false;
            qlonglong winId = _winId.toLongLong(&ok);

            if (ok) {
                m_windowsToHighlight.append(winId);
            }
        }
    }

    updateWindowHighlight();
}

void Backend::updateWindowHighlight()
{
    if (!m_highlightWindows) {
        if (m_panelWinId) {
            KWindowEffects::highlightWindows(m_panelWinId, QList<WId>());

            m_panelWinId = 0;
        }

        return;
    }

    if (m_taskManagerItem && m_taskManagerItem->window()) {
        m_panelWinId = m_taskManagerItem->window()->winId();
    } else {
        return;
    }

    QList<WId> windows = m_windowsToHighlight;

    if (windows.count() && m_toolTipItem && m_toolTipItem->window()) {
        windows.append(m_toolTipItem->window()->winId());
    }

    if (windows.count() && m_groupDialog) {
        windows.append(m_groupDialog->winId());
    }

    KWindowEffects::highlightWindows(m_panelWinId, windows);
}
