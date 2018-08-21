#include "database.h"
#include "macros.h"
#include "../apikeys.h"
#include "rqtplayerdeck.h"
#include "rqtplayerdeckupdate.h"
#include "rqtupdateplayercollection.h"
#include "rqtupdateplayerinventory.h"
#include "rqtuploadmatch.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

#define HEADER_AUTHORIZATION "Authorization"
#define UPDATE_DELAY 30 //minutes between each player inventory and collection update

LotusTrackerAPI::LotusTrackerAPI(QObject *parent, FirebaseAuth *firebaseAuth)
{
    UNUSED(parent);
    this->firebaseAuth = firebaseAuth;
    connect(firebaseAuth, &FirebaseAuth::sgnTokenRefreshed,
            this, &LotusTrackerAPI::onTokenRefreshed);
}

LotusTrackerAPI::~LotusTrackerAPI()
{

}

void LotusTrackerAPI::onTokenRefreshed()
{
    for(QString requestUrl : requestsToRecall.keys()) {
        QPair<QString, RequestData> methodRequest = requestsToRecall[requestUrl];
        LOGD(QString("Recalling request"));
        if (methodRequest.first == "POST") {
            sendPost(methodRequest.second);
        }
        if (methodRequest.first == "PATCH") {
            sendPatch(methodRequest.second);
        }
    }
}

void LotusTrackerAPI::updatePlayerCollection(QMap<int, int> ownedCards)
{
    QDateTime now = QDateTime::currentDateTime();
    if (!lastUpdatePlayerCollectionDate.isNull() &&
            lastUpdatePlayerCollectionDate.secsTo(now) <= UPDATE_DELAY*60) {
        return;
    }
    lastUpdatePlayerCollectionDate = now;
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }
    sendPost(RqtUpdatePlayerCollection(userSettings.userId, ownedCards));
}

void LotusTrackerAPI::updatePlayerInventory(PlayerInventory playerInventory)
{
    QDateTime now = QDateTime::currentDateTime();
    if (!lastUpdatePlayerInventoryDate.isNull() &&
            lastUpdatePlayerInventoryDate.secsTo(now) <= UPDATE_DELAY*60) {
        return;
    }
    lastUpdatePlayerInventoryDate = now;
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }
    sendPatch(RqtUpdatePlayerInventory(userSettings.userId, playerInventory));
}

void LotusTrackerAPI::createPlayerDeck(Deck deck)
{
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }
    sendPost(RqtPlayerDeck(userSettings.userId, deck));
}

void LotusTrackerAPI::updatePlayerDeck(Deck deck)
{
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }
    paramDeck = deck;
    getPlayerDeckToUpdate(deck.id);
}

void LotusTrackerAPI::getPlayerDeckToUpdate(QString deckID)
{
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }

    QUrl url(QString("%1/users/decks?userId=%2&deckId=%3").arg(ApiKeys::API_BASE_URL())
             .arg(userSettings.userId).arg(deckID));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QString(HEADER_AUTHORIZATION).toUtf8(),
                         QString("Bearer %1").arg(userSettings.userToken).toUtf8());
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Get Request: %1").arg(url.toString()));
    }
    QNetworkReply *reply = networkManager.get(request);
    connect(reply, &QNetworkReply::finished,
            this, &LotusTrackerAPI::getPlayerDeckToUpdateRequestOnFinish);
}

void LotusTrackerAPI::getPlayerDeckToUpdateRequestOnFinish()
{
    QNetworkReply *reply = static_cast<QNetworkReply*>(sender());
    QJsonObject jsonRsp = Transformations::stringToJsonObject(reply->readAll());
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString(QJsonDocument(jsonRsp).toJson()));
    }
    emit sgnRequestFinished();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode > 299) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        LOGW(QString("Error: %1 - %2").arg(reply->errorString()).arg(reason));
        if (statusCode == 404) {    //Not found
            createPlayerDeck(paramDeck);
            return;
        }
        QString message = jsonRsp["error"].toString();
        ARENA_TRACKER->showMessage(message);
        return;
    }

    Deck oldDeck = jsonToDeck(jsonRsp);
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    //Update deck data
    sendPatch(RqtPlayerDeck(userSettings.userId, paramDeck));
    //Create deck update
    RqtPlayerDeckUpdate rqtPlayerDeckUpdate(userSettings.userId, paramDeck, oldDeck);
    if (rqtPlayerDeckUpdate.isValid()) {
        sendPost(rqtPlayerDeckUpdate);
    }
}

Deck LotusTrackerAPI::jsonToDeck(QJsonObject deckJson)
{
    QString id = deckJson["id"].toString();
    QString name = deckJson["name"].toString();
    QJsonObject cardsFields = deckJson["cards"].toObject();
    QMap<Card*, int> cards;
    for (QString key : cardsFields.keys()) {
        Card* card = ARENA_TRACKER->mtgCards->findCard(key.toInt());
        cards[card] = cardsFields[key].toInt();
    }
    QJsonObject sideboardCardsFields = deckJson["sideboard"].toObject();
    QMap<Card*, int> sideboard;
    for (QString key : sideboardCardsFields.keys()) {
        Card* card = ARENA_TRACKER->mtgCards->findCard(key.toInt());
        sideboard[card] = sideboardCardsFields[key].toInt();
    }
    return Deck(id, name, cards, sideboard);
}

QNetworkRequest LotusTrackerAPI::prepareRequest(RequestData requestData,
                                                bool checkUserAuth, QString method)
{
    QUrl url(QString("%1/%2").arg(ApiKeys::API_BASE_URL()).arg(requestData.path()));
    QNetworkRequest request(url);

    requestsToRecall[url.toString()] = qMakePair(method, requestData);
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (checkUserAuth && !userSettings.isAuthValid()) {
        LOGD(QString("User token expired.").arg(url.toString()));
        firebaseAuth->refreshToken(userSettings.refreshToken);
        return request;
    }

    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Request: %1").arg(url.toString()));
    }
    QString userToken = APP_SETTINGS->getUserSettings().userToken;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QString(HEADER_AUTHORIZATION).toUtf8(),
                         QString("Bearer %1").arg(userToken).toUtf8());
    return request;
}

QBuffer* LotusTrackerAPI::prepareBody(RequestData requestData)
{
    QByteArray bodyJson = requestData.body().toJson();
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Body: %1").arg(QString(bodyJson)));
    }
    QBuffer* buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(bodyJson);
    buffer->seek(0);
    return buffer;
}

void LotusTrackerAPI::sendPatch(RequestData requestData)
{
    QNetworkRequest request = prepareRequest(requestData, true, "PATCH");
    if (!request.hasRawHeader(QString(HEADER_AUTHORIZATION).toUtf8())) {
        return;
    }
    QBuffer* buffer = prepareBody(requestData);
    QNetworkReply *reply = networkManager.sendCustomRequest(request, "PATCH", buffer);
    connect(reply, &QNetworkReply::finished,
            this, &LotusTrackerAPI::requestOnFinish);
}

void LotusTrackerAPI::sendPost(RequestData requestData)
{
    QNetworkRequest request = prepareRequest(requestData, true, "POST");
    if (!request.hasRawHeader(QString(HEADER_AUTHORIZATION).toUtf8())) {
        return;
    }
    QBuffer* buffer = prepareBody(requestData);
    QNetworkReply *reply = networkManager.post(request, buffer);
    connect(reply, &QNetworkReply::finished,
            this, &LotusTrackerAPI::requestOnFinish);
}

void LotusTrackerAPI::requestOnFinish()
{
    QNetworkReply *reply = static_cast<QNetworkReply*>(sender());
    QString requestUrl = reply->request().url().toString();
    QJsonObject jsonRsp = Transformations::stringToJsonObject(reply->readAll());
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Request response: %1").arg(requestUrl));
        LOGD(QString(QJsonDocument(jsonRsp).toJson()));
    }
    emit sgnRequestFinished();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode > 299) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        LOGW(QString("Error: %1 - %2").arg(reply->errorString()).arg(reason));
        if (statusCode == 401) {    //Token expired
            UserSettings userSettings = APP_SETTINGS->getUserSettings();
            firebaseAuth->refreshToken(userSettings.refreshToken);
            return;
        }
        QString error = jsonRsp["error"].toString();
        ARENA_TRACKER->showMessage(error);
        return;
    }

    requestsToRecall.remove(requestUrl);
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Database updated"));
    }
}

void LotusTrackerAPI::uploadMatch(MatchInfo matchInfo, Deck playerDeck,
                                   QString playerRankClass)
{
    rqtRegisterPlayerMatch = RqtRegisterPlayerMatch(matchInfo, playerDeck);
    RqtUploadMatch rqtUploadMatch(matchInfo, playerDeck, playerRankClass);
    QNetworkRequest request = prepareRequest(rqtUploadMatch, false);
    QBuffer* buffer = prepareBody(rqtUploadMatch);
    QNetworkReply *reply = networkManager.post(request, buffer);
    connect(reply, &QNetworkReply::finished,
            this, &LotusTrackerAPI::uploadMatchRequestOnFinish);
}

void LotusTrackerAPI::uploadMatchRequestOnFinish()
{
    QNetworkReply *reply = static_cast<QNetworkReply*>(sender());
    QJsonObject jsonRsp = Transformations::stringToJsonObject(reply->readAll());
    if (LOG_REQUEST_ENABLED) {
        LOGD(QString("Request response: %1").arg(reply->request().url().url()));
        LOGD(QString(QJsonDocument(jsonRsp).toJson()));
    }
    emit sgnRequestFinished();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode > 299) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        LOGW(QString("Error: %1 - %2").arg(reply->errorString()).arg(reason));
        QString error = jsonRsp["error"].toString();
        ARENA_TRACKER->showMessage(error);
        return;
    }

    LOGD(QString("Match uploaded anonymously"));
    QString matchID = jsonRsp["id"].toString();
    registerPlayerMatch(matchID);
}

void LotusTrackerAPI::registerPlayerMatch(QString matchID)
{
    UserSettings userSettings = APP_SETTINGS->getUserSettings();
    if (userSettings.userToken.isEmpty()) {
        return;
    }

    rqtRegisterPlayerMatch.createPath(userSettings.userId, matchID);
    sendPost(rqtRegisterPlayerMatch);
}
