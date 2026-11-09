// SPDX-FileCopyrightText: 2024 Joshua Goins <josh@redstrate.com>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "kwindowstatesaverquick.h"
#include "kconfig_qml_log_settings.h"

#include <QQuickItem>
#include <QQuickWindow>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0) && QT_CONFIG(wayland)
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <qpa/qplatformwindow_p.h>
#endif

#include <KWindowStateSaver>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0) && QT_CONFIG(wayland)
namespace
{
constexpr auto restoreIdProperty = "_k_kconfig_sessionRestoreId";

class NativeSessionRestoreId : public QObject
{
public:
    NativeSessionRestoreId(QWindow *window, const QString &restoreId)
        : QObject(window)
        , m_window(window)
        , m_restoreId(restoreId)
    {
        window->installEventFilter(this);
        if (window->handle() && !window->isVisible()) {
            apply();
        }
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface) {
            const auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
                apply();
            } else if (m_window->property(restoreIdProperty).toString() == m_restoreId) {
                m_window->setProperty(restoreIdProperty, QVariant());
            }
        }
        return false;
    }

private:
    void apply()
    {
        const auto windows = QGuiApplication::topLevelWindows();
        for (const auto *other : windows) {
            if (other != m_window && other->handle() && other->property(restoreIdProperty).toString() == m_restoreId) {
                qCWarning(KCONFIG_QML_LOG) << "WindowStateSaver requires distinct configGroupName values for native session restoration:" << m_restoreId;
                return;
            }
        }

        // SurfaceCreated precedes the xdg_toplevel, without forcing native
        // creation before the application has finished configuring its window.
        if (auto *native = m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
            native->setSessionRestoreId(m_restoreId);
            m_window->setProperty(restoreIdProperty, m_restoreId);
        }
    }

    QWindow *const m_window;
    const QString m_restoreId;
};
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
    if (QGuiApplication::platformName() == QLatin1String("wayland") && !QCoreApplication::testAttribute(Qt::AA_DisableSessionManager)
        && !m_configGroupName.isEmpty()) {
        new NativeSessionRestoreId(window, m_configGroupName);
    }
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
