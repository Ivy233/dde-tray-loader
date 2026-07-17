// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "datetimewidget.h"
#include "constants.h"
#include "regionFormat.h"

#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <DFontSizeManager>
#include <QRegularExpression>

#define PLUGIN_STATE_KEY    "enable"
#define DEFAULT_WEEK_FORMAT "dddd"
#define SIMPLE_WEEK_FORMAT "ddd"

DWIDGET_USE_NAMESPACE

namespace {
// 两个正则都用 static 局部对象，避免每次调用重新编译。
// 1) 半角/全角括号、方括号整体包裹的时区: (tttt)、（tttt）、[tttt]
// 2) 裸时区 token，连同前导分隔符(空格/逗号/分号，但不含句点)一并删除
//    分隔符不含句点，避免误吞 bg_BG "'ч'." 这类以缩写句点结尾的字面量；
//    引号字面量本身不匹配，fr_CA 的 'h'/'min'/'s' 与 bg_BG 的 'ch'.
//    都交由 Qt 的 toString 自行渲染，保证它们不会被剥离。
const QRegularExpression &tzTokenRegex()
{
    static const QRegularExpression re(
        "(?:\\([tT]+\\)|（[tT]+）|\\[[tT]+\\])"
        "|[\\s,，;；]*[tT]+"
    );
    return re;
}

const QRegularExpression &trailingSepRegex()
{
    static const QRegularExpression re("[\\s,，;；]+\\s*$");
    return re;
}

// 从 Qt 时间格式串中剥离时区 token，保留 locale 特定的文本分隔符与引号字面量。
// 与控制中心 shared-utils/DCCLocale::stripTimezoneFromTimeFormat 保持一致。
QString stripTimezoneFromTimeFormat(const QString &timeFormat)
{
    QString s = timeFormat;
    s.remove(tzTokenRegex());
    s.remove(trailingSepRegex());
    return s;
}
}

DatetimeWidget::DatetimeWidget(RegionFormat* regionFormat, QWidget *parent)
    : QWidget(parent)
    , m_timeLabel(new QLabel(this))
    , m_dateLabel(new QLabel(this))
    , m_apLabel(new QLabel(this))
    , m_spacerItem(new QWidget(this))
    , m_24HourFormat(false)
    , m_weekdayFormatType(0)
    , m_shortDateFormat("yyyy-MM-dd")
    , m_weekFormat(DEFAULT_WEEK_FORMAT)
    , m_dockSize(QSize(1920, 37))
    , m_timedateInter(new Timedate1Inter("org.deepin.dde.Timedate1", "/org/deepin/dde/Timedate1", QDBusConnection::sessionBus(), this))
    , m_regionFormat(regionFormat)
    , m_dconfig(Dtk::Core::DConfig::create("org.deepin.dde.tray-loader", "org.deepin.dde.dock.plugin.datetime", "", this))
{
    initUI();

    if (m_dconfig && m_dconfig->isValid()) {
        m_showSeconds = m_dconfig->value("showSeconds", false).toBool();
        connect(m_dconfig, &Dtk::Core::DConfig::valueChanged, this, [this](const QString &key) {
            if (key == "showSeconds") {
                m_showSeconds = m_dconfig->value("showSeconds", false).toBool();
                updateDateTime();
            }
        });
    }

    setWeekdayFormat(m_timedateInter->weekdayFormat());
    connect(m_timedateInter, &Timedate1Inter::WeekdayFormatChanged, this, &DatetimeWidget::setWeekdayFormat);

    m_24HourFormat = m_regionFormat->is24HourFormat();
    adjustFontSize();
    updateDateTimeString();
    installEventFilter(this);

    connect(m_regionFormat, &RegionFormat::longDateFormatChanged, this, &DatetimeWidget::updateDateTime);
    connect(m_regionFormat, &RegionFormat::shortTimeFormatChanged, this, &DatetimeWidget::updateDateTime);
    connect(m_regionFormat, &RegionFormat::shortDateFormatChanged, this, &DatetimeWidget::updateDateTime);
    connect(m_regionFormat, &RegionFormat::longTimeFormatChanged, this, &DatetimeWidget::updateDateTime);
    connect(m_regionFormat, &RegionFormat::localeNameChanged, this, &DatetimeWidget::updateDateTime);
}

void DatetimeWidget::set24HourFormat(const bool value)
{
    if (m_24HourFormat == value) {
        return;
    }

    m_24HourFormat = value;
    m_regionFormat->sync24HourFormatConfig(value);
    update();

    if (isVisible()) {
        emit requestUpdateGeometry();
    }
}

/**
 * @brief DatetimeWidget::setWeekdayFormat 根据类型设置周显示格式
 * @param type 自定义类型
 */
void DatetimeWidget::setWeekdayFormat(int type)
{
    if (m_weekdayFormatType == type)
        return;

    m_weekdayFormatType = type;
    updateWeekdayFormat();
    updateDateTimeString();
}

/**
 * @brief DatetimeWidget::updateWeekdayFormat 更新周的显示格式
 */
void DatetimeWidget::updateWeekdayFormat()
{
    if (1 == m_weekdayFormatType) {
        m_weekFormat = SIMPLE_WEEK_FORMAT;
    } else {
        m_weekFormat = DEFAULT_WEEK_FORMAT;
    }
}

QString DatetimeWidget::effectiveTimeFormat() const
{
    if (!m_showSeconds)
        return m_regionFormat->getShortTimeFormat();

    // 显示秒时采用长时间格式，并剥离时区 token，与控制中心"时间和日期"页顶部时间显示一致
    return stripTimezoneFromTimeFormat(m_regionFormat->getLongTimeFormat());
}

void DatetimeWidget::setRegionFormat(RegionFormat *newRegionFormat)
{
    m_regionFormat = newRegionFormat;
}

/**
 * @brief DatetimeWidget::updateWeekdayTimeString 更新任务栏时间标签的显示
 */
void DatetimeWidget::updateDateTimeString()
{
    QLocale locale(m_regionFormat->getLocaleName());

    QString longDateFormat = m_regionFormat->getLongDateFormat();
    longDateFormat.replace(DEFAULT_WEEK_FORMAT, m_weekFormat, Qt::CaseInsensitive);
    m_dateTime = locale.toString(QDateTime::currentDateTime(), longDateFormat + " " + m_regionFormat->getLongTimeFormat());

    QDateTime current = QDateTime::currentDateTime();

    const auto position = qApp->property(PROP_POSITION).value<Dock::Position>();
    QString timeStr, dateString;
    if (position == Dock::Bottom || position == Dock::Top) {
        QString timeFormat = effectiveTimeFormat();
        timeStr = locale.toString(current, timeFormat);
        dateString = locale.toString(current.date(), m_regionFormat->getShortDateFormat());

        m_timeLabel->setText(timeStr);
        m_dateLabel->setText(dateString);
    } else {
        if (!m_24HourFormat) {
            QString apText = locale.toString(current, "AP");
            m_apLabel->setText(apText);

            QString timeFormat = effectiveTimeFormat();
            timeFormat.replace("AP", "");
            timeFormat.replace(" ", "");
            timeStr = locale.toString(current.time(), timeFormat);
        } else {
            timeStr = locale.toString(current.time(), effectiveTimeFormat());
        }

        m_timeLabel->setText(timeStr);
        dateString = locale.toString(current.date(), m_regionFormat->getShortDateFormat());
        m_dateLabel->setText(dateString);
    }
}

void DatetimeWidget::updateDateTime()
{
    m_24HourFormat = m_regionFormat->is24HourFormat();
    adjustUI();
    updateDateTimeString();
    update();

    if (isVisible()) {
        emit requestUpdateGeometry();
    }
}

void DatetimeWidget::adjustFontSize()
{
    const int MAX_DISTANCE = 999;
    const auto position = qApp->property(PROP_POSITION).value<Dock::Position>();
    int validDistance = m_dockSize.height() / devicePixelRatioF();
    if (position == Dock::Left || position == Dock::Right) {
        validDistance = m_dockSize.width() / devicePixelRatioF();
    }

    // dock position changed(from bottom to left), new dock size is not update, use bottom width to adjust font size,
    // then assert in timeFontSize != 0 && dateFontSize != 0
    if (validDistance > MAX_DISTANCE) {
        return;
    }

    // 根据时间和日期字体大小的跨度，将dock栏大小分为不同的区间，每个区域对应不同的字体大小，然后通过判断dock栏大小所在的区间来设置字体大小
    // 如果任务栏小于37，则字体始终取最小值；如果任务栏大于61，则字体始终取最大值；如果任务栏在37和61之间，则字体大小随任务栏大小线性变化
    static const QMap<int, QPair<int, int>> fontSizeMap {
        {0, {12, 9}},
        {37, {12, 9}},
        {40, {13, 10}},
        {43, {14, 10}},
        {46, {15, 11}},
        {49, {16, 11}},
        {52, {17, 12}},
        {55, {18, 12}},
        {58, {19, 13}},
        {61, {20, 14}},
        {MAX_DISTANCE, {20, 14}}
    };

    int timeFontSize = 0;
    int dateFontSize = 0;
    QList<int> distances = fontSizeMap.keys();
    for (int i = 0; i < distances.size() - 1; ++i) {
        if (validDistance >= distances.at(i) && validDistance < distances.at(i + 1)) {
            timeFontSize = fontSizeMap.value(distances.at(i)).first;
            dateFontSize = fontSizeMap.value(distances.at(i)).second;
            break;
        }
    }

    Q_ASSERT(timeFontSize != 0 && dateFontSize != 0);

    QFont timeFont = m_timeLabel->font();
    timeFont.setPixelSize(timeFontSize);
    m_timeLabel->setFont(timeFont);
    m_apLabel->setFont(timeFont);

    QFont dateFont = m_dateLabel->font();
    dateFont.setPixelSize(dateFontSize);
    m_dateLabel->setFont(dateFont);
}

void DatetimeWidget::resizeEvent(QResizeEvent *event)
{
    if (isVisible())
        emit requestUpdateGeometry();

    QWidget::resizeEvent(event);
}

bool DatetimeWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::DevicePixelRatioChange && watched == this) {
        adjustFontSize();
    }

    return QWidget::eventFilter(watched, event);
}

void DatetimeWidget::setDockPanelSize(const QSize &dockSize)
{
    // 任务栏高度最小是37，小于37说明在隐藏和显示动画中
    if (dockSize.width() < 37) {
        return;
    }
    if (dockSize.height() < 37) {
        return;
    }

    if (m_dockSize != dockSize) {
        m_dockSize = dockSize;
        adjustFontSize();
        update();
    }

    Q_EMIT requestUpdateGeometry();
}

void DatetimeWidget::dockPositionChanged()
{
    // 等待位置变换完成后再更新
    QTimer::singleShot(300, this, [this]{
        updateDateTime();
        adjustFontSize();
    });

    adjustUI();
}

void DatetimeWidget::initUI()
{
    setContentsMargins(0, 0, 0, 0);

    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_apLabel->setAlignment(Qt::AlignCenter);

    m_timeLabel->setContentsMargins(0, 0, 0, 0);
    m_dateLabel->setContentsMargins(0, 0, 0, 0);
    m_apLabel->setContentsMargins(0, 0, 0, 0);

    // 当任务栏在左/右时，时间文本和日期的间距
    m_spacerItem->setFixedSize(10, 5);

    m_timeLabel->setForegroundRole(QPalette::BrightText);
    m_apLabel->setForegroundRole(QPalette::BrightText);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_apLabel);
    layout->addWidget(m_timeLabel);
    layout->addWidget(m_spacerItem);
    layout->addWidget(m_dateLabel);

    setLayout(layout);

    adjustUI();
}

void DatetimeWidget::adjustUI()
{
    const auto position = qApp->property(PROP_POSITION).value<Dock::Position>();
    if (position == Dock::Left || position == Dock::Right) {
        if (!m_24HourFormat) {
            m_spacerItem->setVisible(true);
            m_apLabel->setVisible(true);
            return;
        }
    }

    m_spacerItem->setVisible(false);
    m_apLabel->setVisible(false);
}