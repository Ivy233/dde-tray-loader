// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "soundview.h"
#include "constants.h"
#include "tipswidget.h"
#include "imageutil.h"
#include "utils.h"
#include "soundmodel.h"

#include <DApplication>
#include <DDBusSender>
#include <DGuiApplicationHelper>

#include <QPainter>
#include <QIcon>
#include <QMouseEvent>
#include <QApplication>
#include <QDBusInterface>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

// menu actions
#define MUTE     "mute"
#define SETTINGS "settings"

using namespace Dock;

SoundView::SoundView(QWidget *parent)
    : QWidget(parent)
    , m_tipsLabel(new TipsWidget(this))
    , m_applet(new SoundApplet)
    , m_iconWidget(new CommonIconButton(this))
{
    m_tipsLabel->setAccessibleName("soundtips");
    m_tipsLabel->setVisible(false);
    m_applet->setVisible(false);
    m_iconWidget->setFixedSize(Dock::DOCK_PLUGIN_ITEM_FIXED_SIZE);
    m_iconWidget->installEventFilter(this);

    connect(&SoundModel::ref(), &SoundModel::volumeChanged, this, &SoundView::refresh, Qt::QueuedConnection);
    connect(&SoundModel::ref(), &SoundModel::muteStateChanged, this, &SoundView::refresh, Qt::QueuedConnection);
    connect(&SoundModel::ref(), &SoundModel::cardsInfoChanged, this, &SoundView::refresh);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [ = ] {
        refreshIcon();
    });
    connect(m_applet.data(), &SoundApplet::requestHideApplet, this, &SoundView::requestHideApplet);

    refresh();
}

QWidget *SoundView::tipsWidget()
{
    refreshTips(true);

    return m_tipsLabel;
}

QWidget *SoundView::popupApplet()
{
    return m_applet.get();
}

const QString SoundView::contextMenu()
{
    QList<QVariant> items;
    items.reserve(2);

    QMap<QString, QVariant> open;
    open["itemId"] = MUTE;
    if (!SoundController::ref().existActiveOutputDevice()) {
        open["itemText"] = tr("Unmute");
        open["isActive"] = false;
    } else {
        if (SoundModel::ref().isMute()) {
            open["itemText"] = tr("Unmute");
        } else {
            open["itemText"] = tr("Mute");
        }
        open["isActive"] = true;
    }
    items.push_back(open);

    if (!QFile::exists(ICBC_CONF_FILE)) {
        QMap<QString, QVariant> settings;
        settings["itemId"] = SETTINGS;
        settings["itemText"] = tr("Sound settings");
        settings["isActive"] = true;
        items.push_back(settings);
    }

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void SoundView::invokeMenuItem(const QString menuId, const bool checked)
{
    Q_UNUSED(checked);

    if (menuId == MUTE) {
        SoundController::ref().SetMuteQueued(!SoundModel::ref().isMute());
    } else if (menuId == SETTINGS) {
        DDBusSender()
        .service("org.deepin.dde.ControlCenter1")
        .interface("org.deepin.dde.ControlCenter1")
        .path("/org/deepin/dde/ControlCenter1")
        .method(QString("ShowModule"))
        .arg(QString("sound"))
        .call();
        Q_EMIT requestHideApplet();
    }
}

void SoundView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    // 保持横纵比
    if (position == Dock::Bottom || position == Dock::Top) {
        setMaximumWidth(height());
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setMaximumHeight(width());
        setMaximumWidth(QWIDGETSIZE_MAX);
    }

    refreshIcon();
}

bool SoundView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_iconWidget && event->type() == QEvent::Wheel) {
        auto e = dynamic_cast<QWheelEvent*>(event);
        if (e) {
            qApp->postEvent(m_applet->mainSlider(), e->clone());
            e->accept();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SoundView::refresh()
{
    refreshIcon();
    refreshTips(false);
}

void SoundView::refreshIcon()
{
    const double volume = SoundModel::ref().volume();
    const double maxVolume = SoundModel::ref().maxVolumeUI();
    const bool mute = !SoundController::ref().existActiveOutputDevice() ? true : SoundModel::ref().isMute();

    QString iconString;
    QString volumeString;
    if (mute)
        volumeString = "muted";
    else if (int(volume) == 0)
        volumeString = "off";
    else if (volume / maxVolume > 0.6)
        volumeString = "high";
    else if (volume / maxVolume > 0.3)
        volumeString = "medium";
    else
        volumeString = "low";

    iconString = QString("audio-volume-%1-symbolic").arg(volumeString);

    // 获取图标对象
    QIcon icon = QIcon::fromTheme(iconString);

    // 获取当前主题信息
    auto themeType = DGuiApplicationHelper::instance()->themeType();
    QString themeTypeName = (themeType == DGuiApplicationHelper::LightType) ? "Light" : "Dark";

    // 获取当前图标主题名称
    QString currentIconTheme = QIcon::themeName();

    // 获取设备像素比
    qreal devicePixelRatio = qApp->devicePixelRatio();

    // 尝试通过 QIcon 获取实际使用的图标文件路径
    QString actualIconPath = "Not found";
    if (!icon.isNull()) {
        // 获取一个 pixmap 来触发图标加载
        QPixmap pixmap = icon.pixmap(QSize(48, 48));

        // 尝试通过 QIcon::name() 获取图标名称（如果可用）
        QString iconName = icon.name();
        if (!iconName.isEmpty()) {
            actualIconPath = QString("Icon name from QIcon: %1").arg(iconName);
        }
    }

    // 手动查找图标文件
    QStringList foundIconPaths;
    QStringList searchPaths = QIcon::themeSearchPaths();

    // 获取主题继承链（包括当前主题和继承的主题）
    QStringList themeNames;
    themeNames << currentIconTheme;

    // 读取主题继承关系
    for (const QString &basePath : searchPaths) {
        if (basePath.startsWith(":/")) continue;
        QString indexFile = basePath + "/" + currentIconTheme + "/index.theme";
        if (QFileInfo::exists(indexFile)) {
            QFile file(indexFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith("Inherits=")) {
                        QString inherits = line.mid(9); // 去掉 "Inherits="
                        QStringList inheritList = inherits.split(',', Qt::SkipEmptyParts);
                        for (QString &theme : inheritList) {
                            theme = theme.trimmed();
                            if (!themeNames.contains(theme)) {
                                themeNames << theme;
                            }
                        }
                        break;
                    }
                }
                file.close();
            }
            break;
        }
    }

    // 在所有主题中查找图标文件
    for (const QString &themeName : themeNames) {
        for (const QString &basePath : searchPaths) {
            if (basePath.startsWith(":/") || !QDir(basePath).exists()) {
                continue;
            }

            QString themePath = basePath + "/" + themeName;
            if (QDir(themePath).exists()) {
                QDirIterator it(themePath, QStringList() << (iconString + ".*"),
                              QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString path = it.next();
                    if (!foundIconPaths.contains(path)) {
                        foundIconPaths.append(path);
                    }
                }
            }
        }
    }

    qWarning() << "[SOUND_ICON_DEBUG] ========================================";
    qWarning() << "[SOUND_ICON_DEBUG] Volume:" << volume << "MaxVolume:" << maxVolume;
    qWarning() << "[SOUND_ICON_DEBUG] Ratio:" << (volume / maxVolume) << "Mute:" << mute;
    qWarning() << "[SOUND_ICON_DEBUG] Icon name:" << iconString;
    qWarning() << "[SOUND_ICON_DEBUG] Theme type:" << themeTypeName;
    qWarning() << "[SOUND_ICON_DEBUG] Icon theme:" << currentIconTheme;
    qWarning() << "[SOUND_ICON_DEBUG] Device pixel ratio:" << devicePixelRatio;
    qWarning() << "[SOUND_ICON_DEBUG] QIcon info:" << actualIconPath;
    qWarning() << "[SOUND_ICON_DEBUG] Icon search paths:" << searchPaths.join(", ");

    if (foundIconPaths.isEmpty()) {
        qWarning() << "[SOUND_ICON_DEBUG] WARNING: No icon files found for" << iconString;
    } else {
        qWarning() << "[SOUND_ICON_DEBUG] Found" << foundIconPaths.size() << "icon file(s):";
        for (const QString &path : foundIconPaths) {
            qWarning() << "[SOUND_ICON_DEBUG]   -" << path;
        }
    }
    qWarning() << "[SOUND_ICON_DEBUG] ========================================";

    m_iconWidget->setIcon(icon);
}

void SoundView::refreshTips(const bool force)
{
    if (!force && !m_tipsLabel->isVisible())
        return;

    if (!SoundController::ref().existActiveOutputDevice()) {
        m_tipsLabel->setText(QString(tr("No output devices")));
    } else {
        if (SoundModel::ref().isMute()) {
            m_tipsLabel->setText(QString(tr("Mute")));
        } else {
            auto volume = std::min(150, SoundModel::ref().volume());
            m_tipsLabel->setText(QString(tr("Volume %1").arg(QString::number(volume) + '%')));
        }
    }
}

void SoundView::setAppletMinHeight(int minHeight)
{
    if (m_applet) {
        m_applet->setMinHeight(minHeight);
    }
}
