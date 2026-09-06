#include "include/ui/widget/ThronedWindowChrome.h"

#include "include/ui/widget/ThronedTitleBar.h"

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QWidget>

#include <QWKWidgets/widgetwindowagent.h>

namespace {
// Every window that got the treatment, so a theme change can reach all of them.
QList<QPointer<QWK::WidgetWindowAgent>> &agents() {
    static QList<QPointer<QWK::WidgetWindowAgent>> list;
    return list;
}

// The theme is applied before the first window has an agent, so the wanted effect
// is remembered here and handed to every agent as it appears.
QString &wantedBackdrop() {
    static QString value;
    return value;
}

// Mica renders the desktop behind the window, which would make every captured
// screenshot depend on the machine that took it.
bool previewRun() {
    static const bool preview = [] {
        for (const QString &argument: QCoreApplication::arguments())
            if (argument.startsWith(QStringLiteral("-ui-preview")) || argument.endsWith(QStringLiteral("-preview")))
                return true;
        return false;
    }();
    return preview;
}

const QStringList &knownBackdrops() {
    static const QStringList names{QStringLiteral("mica"), QStringLiteral("mica-alt"),
                                   QStringLiteral("acrylic-material"), QStringLiteral("dwm-blur")};
    return names;
}

void applyTo(QWK::WidgetWindowAgent *agent) {
    if (agent == nullptr) return;
    const QString wanted = wantedBackdrop();
    // Every other effect comes off first, or Windows composes the last two
    // together and the result belongs to neither.
    for (const QString &name: knownBackdrops())
        if (name != wanted) agent->setWindowAttribute(name, false);
    bool applied = false;
    if (!wanted.isEmpty()) applied = agent->setWindowAttribute(wanted, true);
    if (auto *window = qobject_cast<QWidget *>(agent->parent())) {
        // The stylesheet drops its opaque ground only when the effect really took;
        // a transparent window with nothing behind it is a hole, not a backdrop.
        window->setProperty("custom-style", applied);
        window->style()->polish(window);
        window->update();
    }
}
} // namespace

namespace ThronedChrome {
ThronedTitleBar *install(QWidget *window, const QString &context) {
    auto *titleBar = new ThronedTitleBar(context, window);
    if (window == nullptr) return titleBar;

    auto *agent = new QWK::WidgetWindowAgent(window);
    // A refused setup leaves the ordinary native title bar visible above ours,
    // which is ugly but usable -- unlike a frameless window with no way to move it.
    if (!agent->setup(window)) return titleBar;

    agent->setTitleBar(titleBar);
    // Naming the buttons by role is what earns the Snap Layout flyout on Windows 11;
    // without it the window is merely frameless again.
    agent->setSystemButton(QWK::WindowAgentBase::Minimize, titleBar->minimizeButton());
    agent->setSystemButton(QWK::WindowAgentBase::Maximize, titleBar->maximizeButton());
    agent->setSystemButton(QWK::WindowAgentBase::Close, titleBar->closeButton());
    agents().append(agent);
    applyTo(agent);
    return titleBar;
}

void setBackdrop(const QString &attribute) {
    wantedBackdrop() = previewRun() ? QString() : attribute;
    for (auto it = agents().begin(); it != agents().end();) {
        if (it->isNull()) {
            it = agents().erase(it);
            continue;
        }
        applyTo(*it);
        ++it;
    }
}
} // namespace ThronedChrome
