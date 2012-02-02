/*
    Copyright (C) 2011  Lasath Fernando <kde@lasath.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "conversations-model.h"
#include "conversation.h"
#include "telepathy-text-observer.h"
#include <KDebug>
#include "conversation-target.h"
#include "messages-model.h"

class ConversationsModel::ConversationsModelPrivate
{
public:
    TelepathyTextObserver watcher;
    QList<Conversation*> conversations;
};

ConversationsModel::ConversationsModel(QObject *parent) :
        QAbstractListModel(parent),
        d(new ConversationsModelPrivate)
{
    QHash<int, QByteArray> roles;
    roles[ConversationRole] = "conversation";
    setRoleNames(roles);

    QObject::connect(&d->watcher, SIGNAL(newConversation(Conversation*)), SLOT(onInconmingConversation(Conversation*)));
}

QVariant ConversationsModel::data(const QModelIndex& index, int role) const
{
    QVariant result;
    if (index.isValid()) {
        if (role == ConversationRole) {
            result = QVariant::fromValue<Conversation*>(d->conversations[index.row()]);
            kDebug() << "returning value " << result;
        }
    }
    return result;
}

int ConversationsModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return d->conversations.count();
}

void ConversationsModel::onInconmingConversation(Conversation *newConvo)
{
    //check if conversation's channel is already being handled, if so replace it
    bool handled = false;
    Tp::TextChannelPtr newChannel = newConvo->messages()->textChannel();
    if (!newChannel->targetHandleType() == Tp::HandleTypeNone) {

        //loop through all conversations checking for matches
        Q_FOREACH(Conversation *convo, d->conversations) {
            if (convo->target()->id() == newChannel->targetId() &&
                    convo->messages()->textChannel()->targetHandleType() == newChannel->targetHandleType()) {

                convo->messages()->setTextChannel(newChannel);
                newConvo->deleteLater();
                handled = true;
                break;
            }
        }
    }

    if (!handled) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        d->conversations.append(newConvo);
        connect(newConvo, SIGNAL(validityChanged(bool)), SLOT(handleValidityChange(bool)));
        endInsertRows();
    }
}

void ConversationsModel::handleValidityChange(bool valid)
{
    if(!valid) {
        Conversation* sender = qobject_cast<Conversation*>(QObject::sender());
        int index = d->conversations.indexOf(sender);
        if(index != -1) {
            beginRemoveRows(QModelIndex(), index, index);

            d->conversations.removeAt(index);
            sender->deleteLater();
            endRemoveRows();
        } else {
            kError() << "attempting to delete non-existant conversation";
        }
    }
}

ConversationsModel::~ConversationsModel()
{
    delete d;
}