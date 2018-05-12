#include "../src/mtg/mtgalogparser.h"
#include "macros_test.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>

#include <QSignalSpy>
#include <QtTest/QtTest>

Q_DECLARE_METATYPE(Deck)
Q_DECLARE_METATYPE(Match)
Q_DECLARE_METATYPE(MatchPlayer)
Q_DECLARE_METATYPE(MatchStateDiff)
Q_DECLARE_METATYPE(MatchZone)
Q_DECLARE_METATYPE(PlayerInventory)

class TestMtgaLogParser: public QObject
{
    Q_OBJECT
private:
    MtgCards *mtgCards;
    MtgaLogParser *mtgaLogParser;

public:
    TestMtgaLogParser()
    {
        mtgCards = new MtgCards(this);
        mtgaLogParser = new MtgaLogParser(this, mtgCards);
    }

private slots:
    void testParsePlayerInventory()
    {
        qRegisterMetaType<PlayerInventory>();
        QString log;
        READ_LOG("PlayerInventory.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerInventory);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        PlayerInventory playerInventory = args.first().value<PlayerInventory>();
        QCOMPARE(playerInventory.wcMythic, 6);
    }

    void testParsePlayerInventoryUpdate()
    {
        QString log;
        READ_LOG("PlayerInventoryUpdate.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerInventoryUpdate);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QList<int> newCards = args.first().value<QList<int>>();
        QCOMPARE(newCards.first(), 65963);
    }

    void testParsePlayerCollection()
    {
        qRegisterMetaType<QMap<int, int>>();
        QString log;
        READ_LOG("PlayerCollection.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerCollection);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QMap<int, int> playerCollection = args.first().value<QMap<int, int>>();
        QCOMPARE(playerCollection.size(), 421);
        QCOMPARE(playerCollection[66041], 3);
    }

    void testParsePlayerDecks()
    {
        qRegisterMetaType<QList<Deck>>();
        QString log;
        READ_LOG("PlayerDecks.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerDecks);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QList<Deck> playerDecks = args.first().value<QList<Deck>>();
        QCOMPARE(playerDecks.size(), 3);
        Card* vraskasContemptCard = mtgCards->findCard(66223);
        QCOMPARE(playerDecks.first().cards[vraskasContemptCard], 3);
    }

    void testParseMatchCreated()
    {
        qRegisterMetaType<Match>();
        QString log;
        READ_LOG("MatchCreated.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchCreated);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        Match match = args.first().value<Match>();
        QCOMPARE(match.opponentRankClass(), "Beginner");
        QCOMPARE(match.opponentRankTier(), 1);
    }

    void testParseMatchInfoSeats()
    {
        qRegisterMetaType<QList<MatchPlayer>>();
        QString log;
        READ_LOG("MatchInfoSeats.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchInfoSeats);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QList<MatchPlayer> matchPlayers = args.first().value<QList<MatchPlayer>>();
        MatchPlayer matchPlayer = matchPlayers.first();
        QCOMPARE(matchPlayer.name(), "Edipo2s");
        QCOMPARE(matchPlayer.seatId(), 1);
    }

    void testParseMatchInfoMatchResult()
    {
        QString log;
        READ_LOG("MatchInfoResult.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchInfoResultMatch);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        int matchWinningTeamId = args.first().toInt();
        QCOMPARE(matchWinningTeamId, 1);
    }

    void testParsePlayerRankInfo()
    {
        qRegisterMetaType<QPair<QString, int>>();
        QString log;
        READ_LOG("PlayerRankInfo.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerRankInfo);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QPair<QString, int> playerRankInfo = args.first().value<QPair<QString, int>>();
        QCOMPARE(playerRankInfo.first, "Intermediate");
        QCOMPARE(playerRankInfo.second, 1);
    }

    void testParsePlayerDeckSelected()
    {
        qRegisterMetaType<Deck>();
        QString log;
        READ_LOG("PlayerDeckSelected.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerDeckSelected);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        Deck playerDeckSelected = args.first().value<Deck>();
        Card* forerunnerOfTheEmpireCard = mtgCards->findCard(66821);
        QCOMPARE(playerDeckSelected.cards[forerunnerOfTheEmpireCard], 2);
    }

    void testParsePlayerAcceptsHand()
    {
        QString log;
        READ_LOG("PlayerAcceptHand.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerAcceptsHand);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
    }

    void testParsePlayerTakesMulligan()
    {
        QString log;
        READ_LOG("PlayerTakesMulligan.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerTakesMulligan);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
    }

    void testParsePlayerCardHoverStarts()
    {
        QString log;
        READ_LOG("PlayerCardHoverStarts.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerCardHoverStarts);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
    }

    void testParsePlayerCardHoverEnds()
    {
        QString log;
        READ_LOG("PlayerCardHoverEnds.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnPlayerCardHoverEnds);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
    }

    void testParseSeatIdThatGoFirst()
    {
        QString log;
        READ_LOG("DieRollResultsResp.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnSeatIdThatGoFirst);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        int seatIdThatGoFirst = args.first().toInt();
        QCOMPARE(seatIdThatGoFirst, 1);
    }

    void testParseSeatIdThatGoFirst2()
    {
        QString log;
        READ_LOG("DieRollResultsResp2.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnSeatIdThatGoFirst);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        int seatIdThatGoFirst = args.first().toInt();
        QCOMPARE(seatIdThatGoFirst, 2);
    }

    void testParseMatchStartZones()
    {
        qRegisterMetaType<QList<MatchZone>>();
        QString log;
        READ_LOG("GameStateFull.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchStartZones);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QList<MatchZone> zones = args.first().value<QList<MatchZone>>();
        QCOMPARE(zones.size(), 10);
        for (MatchZone zone : zones) {
            if (zone.type() == ZoneType_LIBRARY) {
                QCOMPARE(zone.objectIds.size(), 60);
            }
        }
    }

    void testParseMatchStateDiffObjectIds()
    {
        qRegisterMetaType<MatchStateDiff>();
        QString log;
        READ_LOG("GameStateDiffObjectIds.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchStateDiff);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        MatchStateDiff matchStateDiff = args.first().value<MatchStateDiff>();
        QCOMPARE(matchStateDiff.zones().size(), 4);
        MatchZone zonePlayerHand;
        for (MatchZone zone : matchStateDiff.zones()) {
            if (zone.id() == 31) {
                zonePlayerHand = zone;
                break;
            }
        }
        QCOMPARE(zonePlayerHand.objectIds[103], 65889);
        QCOMPARE(zonePlayerHand.objectIds[104], 65575);
        QCOMPARE(zonePlayerHand.objectIds[105], 65769);
        QCOMPARE(zonePlayerHand.objectIds[106], 66707);
        QCOMPARE(zonePlayerHand.objectIds[107], 64913);
        QCOMPARE(zonePlayerHand.objectIds[108], 65889);
        QCOMPARE(zonePlayerHand.objectIds[109], 66223);
    }

    void testParseMatchStateDiffZoneTransfer()
    {
        qRegisterMetaType<MatchStateDiff>();
        QString log;
        READ_LOG("GameStateDiffZoneTransfer.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnMatchStateDiff);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        MatchStateDiff matchStateDiff = args.first().value<MatchStateDiff>();
        QCOMPARE(matchStateDiff.idsChanged()[287], 343);
        QCOMPARE(matchStateDiff.idsZoneChanged()[343], qMakePair(35, 28));
    }

    void testParseMatchStateDiffNewTurn()
    {
        QString log;
        READ_LOG("GameStateDiffNewTurn.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnNewTurnStarted);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        int turnNumber = args.first().toInt();
        QCOMPARE(turnNumber, 2);
    }

    void testParseOpponentTakesMulligan()
    {
        QString log;
        READ_LOG("OpponentTakesMulligan.txt", log);
        QSignalSpy spy(mtgaLogParser, &MtgaLogParser::sgnOpponentTakesMulligan);
        mtgaLogParser->parse(log);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        int opponentSeatId = args.first().toInt();
        QCOMPARE(opponentSeatId, 2);
    }

};
