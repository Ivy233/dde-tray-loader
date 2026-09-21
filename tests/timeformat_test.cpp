// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "timeformat.h"

#include <QLocale>
#include <QTest>
#include <QTime>

class TimeFormatTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addSeconds_data();
    void addSeconds();
    void allLocalesShowSeconds();
};

void TimeFormatTest::addSeconds_data()
{
    QTest::addColumn<QString>("shortFormat");
    QTest::addColumn<QString>("longFormat");
    QTest::addColumn<QString>("expected");

    QTest::newRow("two-digit colon") << "HH:mm" << "HH:mm:ss tttt" << "HH:mm:ss";
    QTest::newRow("single-digit fields") << "H:m" << "HH:mm:ss tttt" << "H:m:s";
    QTest::newRow("dot separator") << "H.mm" << "H.mm.ss tttt" << "H.mm.ss";
    QTest::newRow("dot and am-pm suffix") << "h.mm. Ap" << "h.mm.ss Ap tttt" << "h.mm.ss. Ap";
    QTest::newRow("am-pm suffix") << QString::fromUtf8("h:mm\u202fAp") << QString::fromUtf8("h:mm:ss\u202fAp tttt") << QString::fromUtf8("h:mm:ss\u202fAp");
    QTest::newRow("am-pm prefix") << "Ap h:mm" << "Ap h:mm:ss tttt" << "Ap h:mm:ss";
    QTest::newRow("localized am-pm prefix") << QString::fromUtf8("Ap नि h:mm") << "Ap h:mm:ss tttt" << QString::fromUtf8("Ap नि h:mm:ss");
    QTest::newRow("no separator") << "Aph:mm" << "Aph:mm:ss [tttt]" << "Aph:mm:ss";
    QTest::newRow("quoted prefix") << "'Kl'. H.mm" << "'Klock' H.mm:ss (tttt)" << "'Kl'. H.mm.ss";
    QTest::newRow("localized suffix") << QString::fromUtf8("H:mm 'hodź'.") << "H:mm:ss tttt" << QString::fromUtf8("H:mm:ss 'hodź'.");
    QTest::newRow("canadian french") << "HH 'h' mm" << "HH 'h' mm 'min' ss 's' tttt" << "HH 'h' mm 'min' ss 's'";
    QTest::newRow("occitan hour separator") << "H'h'mm" << "HH:mm:ss tttt" << "H'h'mm:ss";
    QTest::newRow("localized hour separator")
        << QString::fromUtf8("ཆུ་ཚོད་ h སྐར་མ་ mm Ap")
        << QString::fromUtf8("ཆུ་ཚོད་ h སྐར་མ་ mm:ss Ap tttt")
        << QString::fromUtf8("ཆུ་ཚོད་ h སྐར་མ་ mm:ss Ap");
    QTest::newRow("already has seconds") << "HH:mm:ss" << "HH:mm:ss tttt" << "HH:mm:ss";
    QTest::newRow("invalid custom format") << "HH" << "HH:mm:ss tttt" << "HH";
    QTest::newRow("ordinary format does not need long seconds") << "HH:mm" << "HH:mm tttt" << "HH:mm:ss";
    QTest::newRow("localized separator needs long seconds") << "HH 'h' mm" << "HH 'h' mm tttt" << "HH 'h' mm";
}

void TimeFormatTest::addSeconds()
{
    QFETCH(QString, shortFormat);
    QFETCH(QString, longFormat);
    QFETCH(QString, expected);

    QCOMPARE(addSecondsToShortTimeFormat(shortFormat, longFormat), expected);
}

void TimeFormatTest::allLocalesShowSeconds()
{
    const auto locales = QLocale::matchingLocales(QLocale::AnyLanguage,
                                                   QLocale::AnyScript,
                                                   QLocale::AnyTerritory);
    for (const QLocale &locale : locales) {
        const QString shortFormat = locale.timeFormat(QLocale::ShortFormat);
        const QString longFormat = locale.timeFormat(QLocale::LongFormat);
        const QString format = addSecondsToShortTimeFormat(shortFormat, longFormat);
        const QString first = locale.toString(QTime(12, 34, 1), format);
        const QString second = locale.toString(QTime(12, 34, 2), format);
        QVERIFY2(first != second,
                 qPrintable(QStringLiteral("Locale %1 with short format %2 did not show seconds")
                                .arg(locale.name(), shortFormat)));
    }
}

QTEST_APPLESS_MAIN(TimeFormatTest)

#include "timeformat_test.moc"
