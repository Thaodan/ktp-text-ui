/***************************************************************************
 *   Copyright (C) 2010 by David Edmundson <kde@davidedmundson.co.uk>      *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "chat-widget.h"

#include "ui_chat-widget.h"
#include "adium-theme-header-info.h"
#include "adium-theme-content-info.h"
#include "adium-theme-message-info.h"
#include "adium-theme-status-info.h"
#include "channel-contact-list.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QAction>

#include <KColorDialog>
#include <KNotification>
#include <KAboutData>
#include <KComponentData>
#include <KDebug>
#include <KColorScheme>

#include <TelepathyQt4/Message>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/Presence>


class MessageBoxEventFilter : public QObject
{
    Q_OBJECT
public:
    MessageBoxEventFilter(QObject* parent = 0) : QObject(parent) {}

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event)
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
                if (!keyEvent->modifiers()) {
                    Q_EMIT returnKeyPressed();
                    return true;
                }
            }
        }
        // standard event processing
        return QObject::eventFilter(obj, event);
    }

Q_SIGNALS:
    void returnKeyPressed();
};

class WindowEventFilter : public QObject
{
    Q_OBJECT
public:
    WindowEventFilter(QObject* parent = 0) : QObject(parent) {}

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event)
    {
        if (event->type() == QEvent::ActivationChange) {
            Q_EMIT windowActivated();
        }
        return QObject::eventFilter(obj, event);
    }

Q_SIGNALS:
    void windowActivated();
};

class ChatWidgetPrivate
{
public:
    ChatWidgetPrivate()
    {
        isGroupChat = false;
        remoteContactIsTyping = false;
        unreadMessages = 0;
    }
    /** Stores whether the channel is ready with all contacts upgraded*/
    bool chatviewlInitialised;
    bool remoteContactIsTyping;
    QAction *showFormatToolbarAction;
    bool isGroupChat;
    int unreadMessages;
    QString title;
    Tp::TextChannelPtr channel;
    Ui::ChatWidget ui;
    KIcon icon;

    KComponentData telepathyComponentData();
};


//FIXME I would like this to be part of the main KDE Telepathy library as a static function somewhere.
KComponentData ChatWidgetPrivate::telepathyComponentData()
{
    KAboutData telepathySharedAboutData("ktelepathy",0,KLocalizedString(),0);
    return KComponentData(telepathySharedAboutData);
}

ChatWidget::ChatWidget(const Tp::TextChannelPtr & channel, QWidget *parent)
    : QWidget(parent),
      d(new ChatWidgetPrivate)
{
    d->channel = channel;

    d->chatviewlInitialised = false;
    d->showFormatToolbarAction = new QAction(i18n("Show format options"), this);
    d->isGroupChat = false;

    d->ui.setupUi(this);

    d->ui.formatColor->setText(QString());
    d->ui.formatColor->setIcon(KIcon("format-text-color"));

    d->ui.formatBold->setText(QString());
    d->ui.formatBold->setIcon(KIcon("format-text-bold"));

    d->ui.formatItalic->setText(QString());
    d->ui.formatItalic->setIcon(KIcon("format-text-italic"));

    d->ui.formatUnderline->setText(QString());
    d->ui.formatUnderline->setIcon(KIcon("format-text-underline"));

    d->ui.insertEmoticon->setText(QString());
    d->ui.insertEmoticon->setIcon(KIcon("face-smile"));

    //channel is now valid, start keeping track of contacts.
    ChannelContactList* contactList = new ChannelContactList(d->channel, this);
    connect(contactList, SIGNAL(contactPresenceChanged(Tp::ContactPtr,Tp::Presence)),
            SLOT(onContactPresenceChange(Tp::ContactPtr,Tp::Presence)));
    connect(contactList, SIGNAL(contactAliasChanged(Tp::ContactPtr,QString)),
            SLOT(onContactAliasChanged(Tp::ContactPtr,QString)));

    AdiumThemeHeaderInfo info;
    Tp::Contacts allContacts = d->channel->groupContacts();
    //normal chat - self and one other person.
    if (allContacts.size() == 2) {
        //find the other contact which isn't self.
        foreach(const Tp::ContactPtr & it, allContacts) {
            if (it == d->channel->groupSelfContact()) {
                continue;
            } else {
                info.setDestinationDisplayName(it->alias());
                info.setDestinationName(it->id());
                info.setChatName(it->alias());
                info.setIncomingIconPath(it->avatarData().fileName);
            }
        }
    } else {
        //some sort of group chat scenario.. Not sure how to create this yet.
        info.setChatName("Group Chat");
        d->isGroupChat = true;
    }

    info.setSourceName(d->channel->connection()->protocolName());

    //set up anything related to 'self'
    info.setOutgoingIconPath(d->channel->groupSelfContact()->avatarData().fileName);
    info.setTimeOpened(QDateTime::currentDateTime());
    connect(d->ui.chatArea, SIGNAL(loadFinished(bool)), SLOT(chatViewReady()), Qt::QueuedConnection);
    d->ui.chatArea->initialise(info);

    //set the title of this chat.
    d->title = info.chatName();

    //format toolbar visibility
    d->showFormatToolbarAction->setCheckable(true);
    connect(d->showFormatToolbarAction, SIGNAL(toggled(bool)),
            d->ui.formatToolbar, SLOT(setVisible(bool)));
    d->ui.sendMessageBox->addAction(d->showFormatToolbarAction);

    //FIXME load whether to show/hide by default from config file (do per account)
    bool formatToolbarIsVisible = false;
    d->ui.formatToolbar->setVisible(formatToolbarIsVisible);
    d->showFormatToolbarAction->setChecked(formatToolbarIsVisible);

    //connect signals/slots from format toolbar
    connect(d->ui.formatColor, SIGNAL(released()), SLOT(onFormatColorReleased()));
    connect(d->ui.formatBold, SIGNAL(toggled(bool)), d->ui.sendMessageBox, SLOT(setFontBold(bool)));
    connect(d->ui.formatItalic, SIGNAL(toggled(bool)), d->ui.sendMessageBox, SLOT(setFontItalic(bool)));
    connect(d->ui.formatUnderline, SIGNAL(toggled(bool)), d->ui.sendMessageBox, SLOT(setFontUnderline(bool)));

    connect(d->channel.data(), SIGNAL(messageReceived(Tp::ReceivedMessage)),
            SLOT(handleIncomingMessage(Tp::ReceivedMessage)));
    connect(d->channel.data(), SIGNAL(messageReceived(Tp::ReceivedMessage)),
            SLOT(notifyAboutIncomingMessage(Tp::ReceivedMessage)));
    connect(d->channel.data(), SIGNAL(messageSent(Tp::Message,Tp::MessageSendingFlags,QString)),
            SLOT(handleMessageSent(Tp::Message,Tp::MessageSendingFlags,QString)));
    connect(d->channel.data(), SIGNAL(chatStateChanged(Tp::ContactPtr,Tp::ChannelChatState)),
            SLOT(onChatStatusChanged(Tp::ContactPtr,Tp::ChannelChatState)));

    connect(d->ui.sendMessageBox, SIGNAL(textChanged()), SLOT(onInputBoxChanged()));

    // make the sendMessageBox a focus proxy for the chatview
    d->ui.chatArea->setFocusProxy(d->ui.sendMessageBox);

    MessageBoxEventFilter *messageBoxEventFilter = new MessageBoxEventFilter(this);
    d->ui.sendMessageBox->installEventFilter(messageBoxEventFilter);
    connect(messageBoxEventFilter, SIGNAL(returnKeyPressed()), SLOT(sendMessage()));
    connect(d->ui.sendButton, SIGNAL(clicked()), SLOT(sendMessage()));

    WindowEventFilter *windowEventFilter = new WindowEventFilter(this);
    this->window()->installEventFilter(windowEventFilter);
    connect(windowEventFilter, SIGNAL(windowActivated()), SLOT(windowActivated()));

}

ChatWidget::~ChatWidget()
{
    d->channel->requestClose(); // ensure closing; does nothing, if already closed
    delete d;
}

void ChatWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        d->ui.retranslateUi(this);
        break;
    default:
        break;
    }
}

void ChatWidget::resizeEvent(QResizeEvent *e)
{
    //set the maximum height of a text box to a third of the total window height (but no smaller than the minimum size)
    int textBoxHeight = e->size().height() / 3;
    if (textBoxHeight < d->ui.sendMessageBox->minimumSizeHint().height()) {
        textBoxHeight = d->ui.sendMessageBox->minimumSizeHint().height();
    }
    d->ui.sendMessageBox->setMaximumHeight(textBoxHeight);
    QWidget::resizeEvent(e);
}

Tp::TextChannelPtr ChatWidget::textChannel() const
{
    return d->channel;
}

void ChatWidget::showEvent(QShowEvent* e)
{
    kDebug();

    resetUnreadMessageCount();

    QWidget::showEvent(e);
}


QString ChatWidget::title() const
{
    return d->title;
}

KIcon ChatWidget::icon() const
{
    //normal chat - self and one other person.
    if (!d->isGroupChat) {
        //find the other contact which isn't self.
        foreach(const Tp::ContactPtr & contact, d->channel->groupContacts()) {
            if (contact != d->channel->groupSelfContact()) {
                return iconForPresence(contact->presence().type());
            }
        }
    }

    //group chat
    return iconForPresence(Tp::ConnectionPresenceTypeAvailable);
}

QColor ChatWidget::titleColor() const
{
    /*return a color to set the tab text as in order of importance

    typing
    unread messages
    user offline

    */

    KColorScheme scheme(QPalette::Active, KColorScheme::Window);

    if (d->remoteContactIsTyping) {
        kDebug() << "remote is typing";
        return scheme.foreground(KColorScheme::PositiveText).color();
    }

    if (d->unreadMessages > 0 && !isOnTop()) {
        kDebug() << "unread messages";
        return scheme.foreground(KColorScheme::ActiveText).color();
    }

    //normal chat - self and one other person.
    if (!d->isGroupChat) {
        //find the other contact which isn't self.
        foreach(const Tp::ContactPtr & contact, d->channel->groupContacts()) {
            if (contact != d->channel->groupSelfContact()) {
                if (contact->presence().type() == Tp::ConnectionPresenceTypeOffline ||
                    contact->presence().type() == Tp::ConnectionPresenceTypeHidden) {
                    return scheme.foreground(KColorScheme::InactiveText).color();
                }
            }
        }
    }

    return scheme.foreground(KColorScheme::NormalText).color();
}

int ChatWidget::unreadMessageCount() const
{
    kDebug() << title() << d->unreadMessages;

    return d->unreadMessages;
}

void ChatWidget::incrementUnreadMessageCount()
{
    kDebug();

    d->unreadMessages++;

    kDebug() << "emit" << d->unreadMessages;
    Q_EMIT unreadMessagesChanged(d->unreadMessages);
}

void ChatWidget::windowActivated()
{
    kDebug();
    if (isOnTop()) {
        resetUnreadMessageCount();
    }
}

void ChatWidget::resetUnreadMessageCount()
{
    kDebug();

    if(d->unreadMessages > 0) {
        d->unreadMessages = 0;
        Q_EMIT unreadMessagesChanged(d->unreadMessages);
    }
}


bool ChatWidget::isOnTop() const
{
    kDebug() << ( isActiveWindow() && isVisible() );
    return ( isActiveWindow() && isVisible() );
}

void ChatWidget::handleIncomingMessage(const Tp::ReceivedMessage &message)
{
    kDebug() << title() << message.text();
    if (d->chatviewlInitialised) {
        AdiumThemeContentInfo messageInfo(AdiumThemeMessageInfo::RemoteToLocal);

        //debug the message parts (looking for HTML etc)
//        foreach(Tp::MessagePart part, message.parts())
//        {
//            qDebug() << "***";
//            foreach(QString key, part.keys())
//            {
//                qDebug() << key << part.value(key).variant();
//            }
//        }
//      turns out we have no HTML, because no CM supports it yet

        messageInfo.setMessage(message.text());
        messageInfo.setTime(message.received());
        messageInfo.setUserIconPath(message.sender()->avatarData().fileName);
        messageInfo.setSenderDisplayName(message.sender()->alias());
        messageInfo.setSenderScreenName(message.sender()->id());

        d->ui.chatArea->addContentMessage(messageInfo);
        d->channel->acknowledge(QList<Tp::ReceivedMessage>() << message);

        if(!isOnTop()) {
            incrementUnreadMessageCount();
        }

        Q_EMIT messageReceived();
    }

    //if the window isn't ready, we don't acknowledge the mesage. We process them as soon as we are ready.
}

void ChatWidget::notifyAboutIncomingMessage(const Tp::ReceivedMessage & message)
{
    //send the correct notification:
    QString notificationType;
    //choose the correct notification type:
    //options are:
    // kde_telepathy_contact_incoming
    // kde_telepathy_contact_incoming_active_window - TODO - requires information not available yet.
    //FIXME: until the above is available, simply deactivate the event
    if(isOnTop()) {
        kDebug() << "Widget is on top, not doing anything";
        return;
    }
    // kde_telepathy_contact_highlight (contains your name)
    // kde_telepathy_info_event

    //if the message text contains sender name, it's a "highlighted message"
    //TODO DrDanz suggested this could be a configurable list of words that make it highlighted.(seems like a good idea to me)
    if(message.text().contains(d->channel->connection()->selfContact()->alias())) {
        notificationType = QLatin1String("kde_telepathy_contact_highlight");
    } else if(message.messageType() == Tp::ChannelTextMessageTypeNotice) {
        notificationType = QLatin1String("kde_telepathy_info_event");
    } else {
        notificationType = QLatin1String("kde_telepathy_contact_incoming");
    }


    KNotification *notification = new KNotification(notificationType, this);
    notification->setComponentData(d->telepathyComponentData());
    notification->setTitle(i18n("%1 has sent you a message", message.sender()->alias()));

    QPixmap notificationPixmap;
    if (notificationPixmap.load(message.sender()->avatarData().fileName)) {
        notification->setPixmap(notificationPixmap);
    }

    notification->setText(message.text());
    //allows per contact notifications
    notification->addContext("contact", message.sender()->id());
    //TODO notification->addContext("group",... Requires KDE Telepathy Contact to work out which group they are in.

    notification->setActions(QStringList(i18n("View")));
    connect(notification, SIGNAL(activated(unsigned int)), notification, SLOT(raiseWidget()));
    connect(notification, SIGNAL(activated(unsigned int)), this, SLOT(showOnTop()));

    notification->sendEvent();
}

void ChatWidget::handleMessageSent(const Tp::Message &message, Tp::MessageSendingFlags, const QString&) /*Not sure what these other args are for*/
{
    AdiumThemeContentInfo messageInfo(AdiumThemeMessageInfo::LocalToRemote);
    messageInfo.setMessage(message.text());
    messageInfo.setTime(message.sent());

    Tp::ContactPtr sender = d->channel->connection()->selfContact();
    messageInfo.setSenderDisplayName(sender->alias());
    messageInfo.setSenderScreenName(sender->id());
    messageInfo.setUserIconPath(sender->avatarData().fileName);
    d->ui.chatArea->addContentMessage(messageInfo);


    //send the notification that a message has been sent
    KNotification *notification = new KNotification(QLatin1String("kde_telepathy_outgoing"), this);
    notification->setComponentData(d->telepathyComponentData());
    notification->setTitle(i18n("You have sent a message"));
    QPixmap notificationPixmap;
    if (notificationPixmap.load(sender->avatarData().fileName)) {
        notification->setPixmap(notificationPixmap);
    }
    notification->setText(message.text());
    notification->sendEvent();
}

void ChatWidget::chatViewReady()
{
    d->chatviewlInitialised = true;

    //process any messages we've 'missed' whilst initialising.
    foreach(Tp::ReceivedMessage message, d->channel->messageQueue()) {
        handleIncomingMessage(message);
    }
}


void ChatWidget::sendMessage()
{
    if (!d->ui.sendMessageBox->toPlainText().isEmpty()) {
        d->channel->send(d->ui.sendMessageBox->toPlainText());
        d->ui.sendMessageBox->clear();
    }
}

void ChatWidget::onChatStatusChanged(const Tp::ContactPtr & contact, Tp::ChannelChatState state)
{
    //don't show our own status changes.
    if (contact == d->channel->connection()->selfContact()) {
        return;
    }

    bool contactIsTyping = false;

    switch (state) {
    case Tp::ChannelChatStateGone:
      {
        AdiumThemeStatusInfo statusMessage;
        statusMessage.setMessage(i18n("%1 has left the chat", contact->alias()));
        statusMessage.setService(d->channel->connection()->protocolName());
        statusMessage.setStatus("away");
        statusMessage.setTime(QDateTime::currentDateTime());
        d->ui.chatArea->addStatusMessage(statusMessage);
        break;
      }
    case Tp::ChannelChatStateInactive:
        //FIXME send a 'chat timed out' message to chatview
        break;
    case Tp::ChannelChatStateActive:
    case Tp::ChannelChatStatePaused:
        break;
    case Tp::ChannelChatStateComposing:
        contactIsTyping = true;
        break;
    default:
        qDebug() << QString("Unknown case %1").arg(state);
    }

    if (!contactIsTyping) {
        //In a multiperson chat just because this user is no longer typing it doesn't mean that no-one is.
        //loop through each contact, check no-one is in composing mode.
        foreach (const Tp::ContactPtr & contact, d->channel->groupContacts()) {
            if (contact == d->channel->connection()->selfContact()) {
                continue;
            }

            if (d->channel->chatState(contact) == Tp::ChannelChatStateComposing) {
                contactIsTyping = true;
            }
        }
    }

    if (contactIsTyping != d->remoteContactIsTyping) {
        d->remoteContactIsTyping = contactIsTyping;
        Q_EMIT userTypingChanged(contactIsTyping);
    }
}



void ChatWidget::onContactPresenceChange(const Tp::ContactPtr & contact, const Tp::Presence & presence)
{
    QString message;
    bool isYou = (contact == d->channel->groupSelfContact());

    switch (presence.type()) {
    case Tp::ConnectionPresenceTypeOffline:
        if (!isYou) {
            message = i18n("%1 is offline", contact->alias());
        } else {
            message = i18n("You are now marked as offline");
        }
        break;
    case Tp::ConnectionPresenceTypeAvailable:
        if (!isYou) {
            message = i18n("%1 is online", contact->alias());
        } else {
            message = i18n("You are now marked as online");
        }
        break;
    case Tp::ConnectionPresenceTypeBusy:
        if (!isYou) {
            message = i18n("%1 is busy", contact->alias());
        } else {
            message = i18n("You are now marked as busy");
        }
        break;
    case Tp::ConnectionPresenceTypeAway:
    case Tp::ConnectionPresenceTypeExtendedAway:
        if (!isYou) {
            message = i18n("%1 is away", contact->alias());
        } else {
            message = i18n("You are now marked as away");
        }
        break;
    default:
        /*Do nothing*/
        ;
    }

    if (!message.isNull()) {
        AdiumThemeStatusInfo statusMessage;
        statusMessage.setMessage(message);
        statusMessage.setService(d->channel->connection()->protocolName());
        statusMessage.setTime(QDateTime::currentDateTime());
        d->ui.chatArea->addStatusMessage(statusMessage);
    }

    //if in a non-group chat situation, and the other contact has changed state...
    if (!d->isGroupChat && !isYou) {
        d->icon = iconForPresence(presence.type());
        Q_EMIT iconChanged(d->icon);
    }

    Q_EMIT contactPresenceChanged(presence);
}

void ChatWidget::onContactAliasChanged(const Tp::ContactPtr & contact, const QString& alias)
{
    QString message;
    bool isYou = (contact == d->channel->groupSelfContact());

    if (isYou) {
        message = i18n("You are now known as %1", alias);
    } else if (!d->isGroupChat) {
        //HACK the title is the contact alias on non-groupchats,
        //but we should have a better way of keeping the previous
        //aliases of all contacts
        message = i18n("%1 is now known as %2", d->title, alias);
    }

    if (!message.isEmpty()) {
        AdiumThemeStatusInfo statusMessage;
        statusMessage.setMessage(message);
        statusMessage.setService(d->channel->connection()->protocolName());
        statusMessage.setTime(QDateTime::currentDateTime());
        d->ui.chatArea->addStatusMessage(statusMessage);
    }

    //if in a non-group chat situation, and the other contact has changed alias...
    if (!d->isGroupChat && !isYou) {
        Q_EMIT titleChanged(alias);
    }
}

void ChatWidget::onInputBoxChanged()
{
    //if the box is empty
    bool currentlyTyping = !d->ui.sendMessageBox->toPlainText().isEmpty();

    //FIXME buffer what we've sent to telepathy, make this more efficient.
    //FIXME check spec (with olly) as to whether we have to handle idle state etc.
    if(currentlyTyping) {
        d->channel->requestChatState(Tp::ChannelChatStateComposing);
    } else {
        d->channel->requestChatState(Tp::ChannelChatStateActive);
    }
}

void ChatWidget::onFormatColorReleased()
{
    QColor color;
    KColorDialog::getColor(color,this);
    d->ui.sendMessageBox->setTextColor(color);
}

KIcon ChatWidget::iconForPresence(Tp::ConnectionPresenceType presence)
{
    QString iconName;

    switch (presence) {
        case Tp::ConnectionPresenceTypeAvailable:
            iconName = QLatin1String("user-online");
            break;
        case Tp::ConnectionPresenceTypeAway:
            iconName = QLatin1String("user-away");
            break;
        case Tp::ConnectionPresenceTypeExtendedAway:
            iconName = QLatin1String("user-away-extended");
            break;
        case Tp::ConnectionPresenceTypeHidden:
            iconName = QLatin1String("user-invisible");
            break;
        case Tp::ConnectionPresenceTypeBusy:
            iconName = QLatin1String("user-busy");
            break;
        default:
            iconName = QLatin1String("user-offline");
            break;
    }

    return KIcon(iconName);
}

#include "chat-widget.moc" //for MessageBoxEventFilter
#include "moc_chat-widget.cpp" //for ChatWidget