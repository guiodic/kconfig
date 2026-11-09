// SPDX-FileCopyrightText: 2024 Joshua Goins <josh@redstrate.com>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "kwindowstatesaverquick.h"
#include "kconfig_qml_log_settings.h"

#include <QQuickItem>
#include <QQuickWindow>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0) && QT_CONFIG(wayland)
#include <QGuiApplication>
#include <qpa/qplatformwindow_p.h>
#endif

#include <KWindowStateSaver>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0) && QT_CONFIG(wayland)
namespace
{
void setNativeSessionRestoreId(QWindow *window, const QString &restoreId)
{
    if (QGuiApplication::platformName() != QLatin1String("wayland") || QCoreApplication::testAttribute(Qt::AA_DisableSessionManager)
        || window->isVisible() || restoreId.isEmpty()) {
        return;
    }

    constexpr auto restoreIdProperty = "_k_kconfig_sessionRestoreId";
    const auto windows = QGuiApplication::topLevelWindows();
    for (const auto *other : windows) {
        if (other != window && other->property(restoreIdProperty).toString() == restoreId) {
            qCWarning(KCONFIG_QML_LOG) << "WindowStateSaver requires distinct configGroupName values for native session restoration:" << restoreId;
            return;
        }
    }

    // Qt needs a stable per-window name before creating the xdg_toplevel.
    // The config group identifies this window across application launches.
    window->create();
    if (auto *native = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        native->setSessionRestoreId(restoreId);
        window->setProperty(restoreIdProperty, restoreId);
    }
}
}
#endif

void KWindowStateSaverQuick::classBegin()
{
}

void KWindowStateSaverQuick::componentComplete()
{
    const auto parentItem = qobject_cast<QQuickItem *>(parent());
    if (!parentItem) {
        qCWarning(KCONFIG_QML_LOG) << "WindowStateSaver requires a parent item";
        return;
    }

    const auto window = qobject_cast<QWindow *>(parentItem->window());
    if (!window) {
        qCWarning(KCONFIG_QML_LOG) << "WindowStateSaver requires the parent to be a type that inherits QWindow";
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0) && QT_CONFIG(wayland)
    setNativeSessionRestoreId(window, m_configGroupName);
#endif

    new KWindowStateSaver(window, m_configGroupName);

    // To work around oddities in QtQuick window visibility handling.
    // If we do not set the window visible now, then our window state is
    // overwritten during QQuickWindowQmlImpl::setWindowVisibility() because
    // QQuickWindow is AutomaticVisibility by default.
    if (window->windowState() == Qt::WindowMaximized) {
        window->setVisibility(QWindow::Visibility::Maximized);
    }
}

void KWindowStateSaverQuick::setConfigGroupName(const QString &name)
{
    if (m_configGroupName != name) {
        m_configGroupName = name;
        Q_EMIT configGroupNameChanged();
    }
}

QString KWindowStateSaverQuick::configGroupName() const
{
    return m_configGroupName;
}

#include "moc_kwindowstatesaverquick.cpp"
