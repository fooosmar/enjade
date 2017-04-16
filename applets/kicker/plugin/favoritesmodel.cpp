/***************************************************************************
 *   Copyright (C) 2014-2015 by Eike Hein <hein@kde.org>                   *
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

#include "favoritesmodel.h"
#include "appentry.h"
#include "contactentry.h"
#include "fileentry.h"
#include "systementry.h"
#include "actionlist.h"

#include <KLocalizedString>

FavoritesModel::FavoritesModel(QObject *parent) : AbstractModel(parent)
, m_enabled(true)
, m_maxFavorites(-1)
, m_dropPlaceholderIndex(-1)
{
}

FavoritesModel::~FavoritesModel()
{
    qDeleteAll(m_entryList);
}

QString FavoritesModel::description() const
{
    return i18n("Favorites");
}

QVariant FavoritesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return QVariant();
    }

    if (index.row() == m_dropPlaceholderIndex) {
        if (role == Kicker::IsDropPlaceholderRole) {
            return true;
        } else {
            return QVariant();
        }
    }

    int mappedIndex = index.row();

    if (m_dropPlaceholderIndex != -1 && mappedIndex > m_dropPlaceholderIndex) {
        --mappedIndex;
    }

    const AbstractEntry *entry = m_entryList.at(mappedIndex);

    if (role == Qt::DisplayRole) {
        return entry->name();
    } else if (role == Qt::DecorationRole) {
        return entry->icon();
    } else if (role == Kicker::DescriptionRole) {
        return entry->description();
    } else if (role == Kicker::FavoriteIdRole) {
        return entry->id();
    } else if (role == Kicker::UrlRole) {
        return entry->url();
    } else if (role == Kicker::HasActionListRole) {
        return entry->hasActions();
    } else if (role == Kicker::ActionListRole) {
        return entry->actions();
    }

    return QVariant();
}

int FavoritesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entryList.count() + (m_dropPlaceholderIndex != -1 ? 1 : 0);
}

bool FavoritesModel::trigger(int row, const QString &actionId, const QVariant &argument)
{
    if (row < 0 || row >= m_entryList.count()) {
        return false;
    }

    return m_entryList.at(row)->run(actionId, argument);
}

bool FavoritesModel::enabled() const
{
    return m_enabled;
}

void FavoritesModel::setEnabled(bool enable)
{
    if (m_enabled != enable) {
        m_enabled = enable;

        emit enabledChanged();
    }
}

QStringList FavoritesModel::favorites() const
{
    return m_favorites;
}

void FavoritesModel::setFavorites(const QStringList& favorites)
{
    QStringList _favorites(favorites);
    _favorites.removeDuplicates();

    if (_favorites != m_favorites) {
        m_favorites = _favorites;
        refresh();
    }
}

int FavoritesModel::maxFavorites() const
{
    return m_maxFavorites;
}

void FavoritesModel::setMaxFavorites(int max)
{
    if (m_maxFavorites != max)
    {
        m_maxFavorites = max;

        if (m_maxFavorites != -1 && m_favorites.count() > m_maxFavorites) {
            refresh();
        }

        emit maxFavoritesChanged();
    }
}

bool FavoritesModel::isFavorite(const QString &id) const
{
    return m_favorites.contains(id);
}

void FavoritesModel::addFavorite(const QString &id, int index)
{
    if (!m_enabled || id.isEmpty()) {
        return;
    }

    if (m_maxFavorites != -1 && m_favorites.count() == m_maxFavorites) {
        return;
    }

    AbstractEntry *entry = favoriteFromId(id);

    if (!entry || !entry->isValid()) {
        delete entry;
        return;
    }

    setDropPlaceholderIndex(-1);

    int insertIndex = (index != -1) ? index : m_entryList.count();

    beginInsertRows(QModelIndex(), insertIndex, insertIndex);

    m_entryList.insert(insertIndex, entry);
    m_favorites.insert(insertIndex, entry->id());

    endInsertRows();

    emit countChanged();
    emit favoritesChanged();
}

void FavoritesModel::removeFavorite(const QString &id)
{
    if (!m_enabled || id.isEmpty()) {
        return;
    }

    int index = m_favorites.indexOf(id);

    if (index != -1) {
        setDropPlaceholderIndex(-1);

        beginRemoveRows(QModelIndex(), index, index);

        delete m_entryList[index];
        m_entryList.removeAt(index);
        m_favorites.removeAt(index);

        endRemoveRows();

        emit countChanged();
        emit favoritesChanged();
    }
}

void FavoritesModel::moveRow(int from, int to)
{
    if (from >= m_favorites.count() || to >= m_favorites.count()) {
        return;
    }

    if (from == to) {
        return;
    }

    setDropPlaceholderIndex(-1);

    int modelTo = to + (to > from ? 1 : 0);

    bool ok = beginMoveRows(QModelIndex(), from, from, QModelIndex(), modelTo);

    if (ok) {
        m_entryList.move(from, to);
        m_favorites.move(from, to);

        endMoveRows();

        emit favoritesChanged();
    }
}

int FavoritesModel::dropPlaceholderIndex() const
{
    return m_dropPlaceholderIndex;
}

void FavoritesModel::setDropPlaceholderIndex(int index)
{
    if (index == -1 && m_dropPlaceholderIndex != -1) {
        beginRemoveRows(QModelIndex(), m_dropPlaceholderIndex, m_dropPlaceholderIndex);

        m_dropPlaceholderIndex = index;

        endRemoveRows();

        emit countChanged();
    } else if (index != -1 && m_dropPlaceholderIndex == -1) {
        beginInsertRows(QModelIndex(), index, index);

        m_dropPlaceholderIndex = index;

        endInsertRows();

        emit countChanged();
    } else if (m_dropPlaceholderIndex != index) {
        int modelTo = index + (index > m_dropPlaceholderIndex ? 1 : 0);

        bool ok = beginMoveRows(QModelIndex(), m_dropPlaceholderIndex, m_dropPlaceholderIndex, QModelIndex(), modelTo);

        if (ok) {
            m_dropPlaceholderIndex = index;

            endMoveRows();
        }
    }
}

AbstractModel *FavoritesModel::favoritesModel()
{
    return this;
}

void FavoritesModel::refresh()
{
    beginResetModel();

    setDropPlaceholderIndex(-1);

    int oldCount = m_entryList.count();

    qDeleteAll(m_entryList);
    m_entryList.clear();

    QStringList newFavorites;

    foreach(const QString &id, m_favorites) {
        AbstractEntry *entry = favoriteFromId(id);

        if (entry && entry->isValid()) {
            m_entryList << entry;
            newFavorites << entry->id();

            if (m_maxFavorites != -1 && newFavorites.count() == m_maxFavorites) {
                break;
            }
        } else if (entry) {
            delete entry;
        }
    }

    m_favorites = newFavorites;

    endResetModel();

    if (oldCount != m_entryList.count()) {
        emit countChanged();
    }

    emit favoritesChanged();
}

AbstractEntry *FavoritesModel::favoriteFromId(const QString &id)
{
    const QUrl url(id);
    const QString &s = url.scheme();

    if ((s.isEmpty() && id.contains(QStringLiteral(".desktop"))) || s == QStringLiteral("preferred")) {
        return new AppEntry(this, id);
    } else if (s == QStringLiteral("ktp")) {
        return new ContactEntry(this, id);
    } else if (url.isValid() && !url.scheme().isEmpty()) {
        return new FileEntry(this, url);
    } else {
        return new SystemEntry(this, id);
    }

    return nullptr;
}
