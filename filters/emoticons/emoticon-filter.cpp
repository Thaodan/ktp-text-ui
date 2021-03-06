/*
    Copyright (C) 2012  Lasath Fernando <kde@lasath.org>

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

#include "emoticon-filter.h"
#include "emoticons-manager.h"

#include <KPluginFactory>
#include <KEmoticons>

EmoticonFilter::EmoticonFilter(QObject *parent, const QVariantList &)
    : KTp::AbstractMessageFilter(parent)
{
}

void EmoticonFilter::filterMessage(KTp::Message &message, const KTp::MessageContext &context)
{
    Q_UNUSED(context)
    message.setMainMessagePart(EmoticonsManager::themeForAccount(context.account()).parseEmoticons(message.mainMessagePart()));
}

EmoticonFilter::~EmoticonFilter()
{
}

K_PLUGIN_FACTORY(MessageFilterFactory, registerPlugin<EmoticonFilter>();)

#include "emoticon-filter.moc"
