// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "timeformat.h"

#include <QRegularExpression>

namespace {
const QRegularExpression &hourMinuteRegex()
{
    // Quoted text is consumed as a unit so an m in a literal such as 'min'
    // cannot be mistaken for the minute field.
    static const QRegularExpression re("([hH]{1,2})((?:'[^']*'|[^hHms])*)(m{1,2})");
    return re;
}

const QRegularExpression &minuteSecondsRegex()
{
    static const QRegularExpression re("(m{1,2})((?:'[^']*'|[^ms])*)(s{1,2})((?:\\s*'(?:[^']|'')*')*)");
    return re;
}

const QRegularExpression &literalSeparatorRegex()
{
    static const QRegularExpression re("'[^']*'|[\\p{L}\\p{M}]");
    return re;
}

bool containsSecondsField(QString timeFormat)
{
    static const QRegularExpression quotedTextRegex("'[^']*'");
    static const QRegularExpression secondsRegex("s{1,2}");
    timeFormat.remove(quotedTextRegex);
    return timeFormat.contains(secondsRegex);
}
}

QString addSecondsToShortTimeFormat(const QString &shortTimeFormat, const QString &longTimeFormat)
{
    if (containsSecondsField(shortTimeFormat))
        return shortTimeFormat;

    const QRegularExpressionMatch match = hourMinuteRegex().match(shortTimeFormat);
    if (!match.hasMatch())
        return shortTimeFormat;

    const QString minuteField = match.captured(3);
    QString separator = match.captured(2);
    QString secondsSuffix;
    if (separator.isEmpty() || separator.contains(literalSeparatorRegex())) {
        const QRegularExpressionMatch longMatch = minuteSecondsRegex().match(longTimeFormat);
        if (!longMatch.hasMatch())
            return shortTimeFormat;
        separator = longMatch.captured(2);
        secondsSuffix = longMatch.captured(4);
    }

    QString result = shortTimeFormat;
    const QString secondsField(minuteField.size(), QLatin1Char('s'));
    result.insert(match.capturedEnd(3), separator + secondsField + secondsSuffix);
    return result;
}
