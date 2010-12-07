#include "adiumthemecontentinfo.h"
#include <QString>

class AdiumThemeContentInfoPrivate
{
public:
    QString userIconPath;
    QString senderScreenName;
    QString sender;
    QString senderColor;
    QString senderStatusIcon;
    QString senderDisplayName;
    QString textbackgroundcolor;
};

AdiumThemeContentInfo::AdiumThemeContentInfo(AdiumThemeMessageInfo::MessageType type) :
  AdiumThemeMessageInfo(type),
  d(new AdiumThemeContentInfoPrivate)
{

}

AdiumThemeContentInfo::AdiumThemeContentInfo(const AdiumThemeContentInfo &other) :
    AdiumThemeMessageInfo(other),
    d(new AdiumThemeContentInfoPrivate(*other.d))
{

}

AdiumThemeContentInfo::~AdiumThemeContentInfo()
{
    delete d;
}

AdiumThemeContentInfo& AdiumThemeContentInfo::operator=(const AdiumThemeContentInfo& other)
{
    *d = *other.d;
    return *this;
}


QString AdiumThemeContentInfo::userIconPath() const
{
    return d->userIconPath;
}

void AdiumThemeContentInfo::setUserIconPath(const QString &userIconPath)
{
    d->userIconPath = userIconPath;
}

QString AdiumThemeContentInfo::senderScreenName() const
{
    return d->senderScreenName;
}

void AdiumThemeContentInfo::setSenderScreenName(const QString senderScreenName)
{
    d->senderScreenName = senderScreenName;
}

QString AdiumThemeContentInfo::sender() const
{
    return d->senderDisplayName;
}

QString AdiumThemeContentInfo::senderColor() const
{
    return d->senderColor;
}

void AdiumThemeContentInfo::setSenderColor(const QString &senderColor)
{
    d->senderColor = senderColor;
}

QString AdiumThemeContentInfo::senderStatusIcon() const
{
    return d->senderStatusIcon;
}

void AdiumThemeContentInfo::setSenderStatusIcon(const QString &senderStatusIcon)
{
    d->senderStatusIcon = senderStatusIcon;
}

QString AdiumThemeContentInfo::messageDirection() const
{
    switch(type())
    {
    case AdiumThemeMessageInfo::RemoteToLocal:
        return "trl";
        break;
    default:
        return "ltr";
    }
}

QString AdiumThemeContentInfo::senderDisplayName() const
{
    return d->senderDisplayName;
}

void AdiumThemeContentInfo::setSenderDisplayName(const QString &senderDisplayName)
{
    d->senderDisplayName = senderDisplayName;
}
