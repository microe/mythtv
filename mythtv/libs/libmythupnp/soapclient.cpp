//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QBuffer>

#include "soapclient.h"

#include "mythverbose.h"
#include "httprequest.h"

#define LOC      QString("SOAPClient: ")
#define LOC_WARN QString("SOAPClient, Warning: ")
#define LOC_ERR  QString("SOAPClient, Error: ")


/** \brief Full SOAPClient constructor. After constructing the client
 *         with this constructor it is ready for SendSOAPRequest().
 *  \param url          The host and port portion of the command URL
 *  \param sNamespace   The part of the action before the # character
 *  \param sControlPath The path portion of the command URL
 */
SOAPClient::SOAPClient(const QUrl    &url,
                       const QString &sNamespace,
                       const QString &sControlPath) :
    m_url(url), m_sNamespace(sNamespace), m_sControlPath(sControlPath)
{
}


/** \brief SOAPClient Initializer. After constructing the client
 *         with the empty constructor call this before calling
 *         SendSOAPRequest() the first time.
 */
bool SOAPClient::Init(const QUrl    &url,
                      const QString &sNamespace,
                      const QString &sControlPath)
{
    bool ok = true;
    if (sNamespace.isEmpty())
    {
        ok = false;
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Init() given blank namespace");
    }

    QUrl test(url);
    test.setPath(sControlPath);
    if (!test.isValid())
    {
        ok = false;
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Init() given invalid control URL %1")
                .arg(test.toString()));
    }

    if (ok)
    {
        m_url          = url;
        m_sNamespace   = sNamespace;
        m_sControlPath = sControlPath;
    }
    else
    {
        m_url = QUrl();
        m_sNamespace.clear();
        m_sControlPath.clear();
    }

    return ok;
}

/// Used by GeNodeValue() methods to find the named node.
QDomNode SOAPClient::FindNode(
    const QString &sName, const QDomNode &baseNode) const
{
    QStringList parts = sName.split('/', QString::SkipEmptyParts);
    return FindNodeInternal(parts, baseNode);
}

/// This is an internal function used to implement FindNode
QDomNode SOAPClient::FindNodeInternal(
    QStringList &sParts, const QDomNode &curNode) const
{
    if (sParts.empty())
        return curNode;

    QString sName = sParts.front();
    sParts.pop_front();

    QDomNode child = curNode.namedItem(sName);

    if (child.isNull() )
        sParts.clear();

    return FindNodeInternal(sParts, child);
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
int SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, int nDefault) const
{
    QString sValue = GetNodeValue(node, sName, QString::number(nDefault));
    return sValue.toInt();
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
bool SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, bool bDefault) const
{
    QString sDefault = (bDefault) ? "true" : "false";
    QString sValue   = GetNodeValue(node, sName, sDefault);
    if (sValue.isEmpty())
        return bDefault;

    char ret = sValue[0].toAscii();
    switch (ret)
    {
        case 't': case 'T': case 'y': case 'Y': case '1':
            return true;
        case 'f': case 'F': case 'n': case 'N': case '0':
            return false;
        default:
            return bDefault;
    }
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
QString SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, const QString &sDefault) const
{
    if (node.isNull())
        return sDefault;

    QString  sValue  = "";
    QDomNode valNode = FindNode(sName, node);

    if (!valNode.isNull())
    {
        // -=>TODO: Assumes first child is Text Node.

        QDomText oText = valNode.firstChild().toText();

        if (!oText.isNull())
            sValue = oText.nodeValue();

        return QUrl::fromPercentEncoding(sValue.toUtf8());
    }

    return sDefault;
}

/** Actually sends the sMethod action to the command URL specified
 * in the constructor (url+[/]+sControlPath). list is parsed as
 * a series of key value pairs for the input params and then
 * cleared and used for the output params.
 *
 * \note nErrCode and sErrDesc are only set when this method returns false.
 *
 * \param bInQtThread May be set to true if this is run from within
 *                    a QThread with a running an event loop.
 * \return true on success, false otherwise.
 */
bool SOAPClient::SendSOAPRequest(const QString &sMethod, 
                                 QStringMap    &list, 
                                 int           &nErrCode, 
                                 QString       &sErrDesc,
                                 bool           bInQtThread)
{
    QUrl url(m_url);

    url.setPath(m_sControlPath);

    // TODO handle this approriately
    if (m_sNamespace.isEmpty())
    {
        nErrCode = 0;
        sErrDesc = "No namespace given";
        return false;
    }

    // --------------------------------------------------------------
    // Add appropriate headers
    // --------------------------------------------------------------

    QHttpRequestHeader header("POST", sMethod, 1, 0);

    header.setValue("CONTENT-TYPE", "text/xml; charset=\"utf-8\"" );
    header.setValue("SOAPACTION",
                    QString("\"%1#%2\"").arg(m_sNamespace).arg(sMethod));

    // --------------------------------------------------------------
    // Build request payload
    // --------------------------------------------------------------

    QByteArray  aBuffer;
    QTextStream os( &aBuffer );

    os.setCodec("UTF-8");

    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"; 
    os << "<s:Envelope "
        " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\""
        " xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n";
    os << " <s:Body>\r\n";
    os << "  <u:" << sMethod << " xmlns:u=\"" << m_sNamespace << "\">\r\n";

    // --------------------------------------------------------------
    // Add parameters from list
    // --------------------------------------------------------------

    for (QStringMap::iterator it = list.begin(); it != list.end(); ++it)
    {                                                               
        os << "   <" << it.key() << ">";
        os << HTTPRequest::Encode( *it );
        os << "</"   << it.key() << ">\r\n";
    }

    os << "  </u:" << sMethod << ">\r\n";
    os << " </s:Body>\r\n";
    os << "</s:Envelope>\r\n";

    os.flush();

    // --------------------------------------------------------------
    // Perform Request
    // --------------------------------------------------------------

    QBuffer buff(&aBuffer);

    VERBOSE(VB_UPNP|VB_EXTRA,
            QString("SOAPClient(%1) sending:\n").arg(url.toString()) +
            header.toString() +
            QString("\n%1\n").arg(aBuffer.constData()));

    QString sXml = HttpComms::postHttp(
        url,
        &header,
        &buff, // QIODevice*
        10000, // ms -- Technically we should allow 30,000 ms per spec
        3,     // retries
        0,     // redirects
        false, // allow gzip
        NULL,  // login
        bInQtThread,
        QString() // userAgent, UPnP/1.0 very strict on format if set
        );

    // --------------------------------------------------------------
    // Parse response
    // --------------------------------------------------------------

    VERBOSE(VB_UPNP|VB_EXTRA, "SOAPClient response:\n" +QString("%1\n")
            .arg(sXml));

    // TODO handle timeout without response correctly.

    list.clear();

    QDomDocument doc;

    if (!doc.setContent(sXml, true, &sErrDesc, &nErrCode))
    {
        VERBOSE(VB_UPNP,
                QString("MythXMLClient::SendSOAPRequest( %1 ) - "
                        "Invalid response from %2")
                .arg(sMethod).arg(url.toString()) + 
                QString("%1: %2").arg(nErrCode).arg(sErrDesc));

        return false;
    }

    // --------------------------------------------------------------
    // Is this a valid response?
    // --------------------------------------------------------------

    QString      sResponseName = sMethod + "Response";
    QDomNodeList oNodeList     =
        doc.elementsByTagNameNS(m_sNamespace, sResponseName);

    if (oNodeList.count() == 0)
    {
        // --------------------------------------------------------------
        // Must be a fault... parse it to return reason
        // --------------------------------------------------------------

        nErrCode = GetNodeValue(
            doc, "Envelope/Body/Fault/detail/UPnPError/errorCode", 500);
        sErrDesc = GetNodeValue(
            doc, "Envelope/Body/Fault/detail/UPnPError/errorDescription",
            QString("Unknown"));

        return false;
    }

    QDomNode oMethod = oNodeList.item(0);
    if (oMethod.isNull())
        return true;

    QDomNode oNode = oMethod.firstChild(); 
    for (; !oNode.isNull(); oNode = oNode.nextSibling())
    {
        QDomElement e = oNode.toElement();
        if (e.isNull())
            continue;

        QString sName  = e.tagName();
        QString sValue = "";
    
        QDomText  oText = oNode.firstChild().toText();
    
        if (!oText.isNull())
            sValue = oText.nodeValue();

        list.insert(QUrl::fromPercentEncoding(sName.toUtf8()),
                    QUrl::fromPercentEncoding(sValue.toUtf8()));
    }

    return true;
}
