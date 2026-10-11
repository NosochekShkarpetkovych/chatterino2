// SPDX-License-Identifier: MIT
//
// Optional Ukrainian translation of the Chatterino UI.
//
// Chatterino has no translation system (strings are hard-coded), so this
// works as a dictionary "English text -> Ukrainian text" that is applied to
// widgets when they are shown (labels, buttons, check boxes, group boxes,
// tool tips, menu actions, table headers, drop-down items).
//
// The whole thing is header-only on purpose: no new .cpp has to be added to
// the build system.
//
// Extra translations can be added WITHOUT rebuilding, in the file
//   %APPDATA%\Chatterino2\uk_extra.txt
// one entry per line:  English text<TAB>Ukrainian text   (use \n for newlines).
// Texts that were shown but not translated are collected in
//   %APPDATA%\Chatterino2\untranslated.txt

#pragma once

#include "common/UiTranslationData.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QVariant>
#include <QWidget>
#include <QWidgetAction>

#include <algorithm>
#include <utility>
#include <vector>

namespace chatterino::uitr {

/// Item data role where the original (English) text of a translated combo box
/// item is kept. Some settings use the item text as their value.
inline constexpr int ORIGINAL_TEXT_ROLE = Qt::UserRole + 100;

namespace detail {

struct Pattern {
    QRegularExpression re;
    QString replacement;
    QList<int> placeholders;
};

struct State {
    bool loaded = false;
    QHash<QString, QString> exact;
    QHash<QString, QString> normalized;
    std::vector<std::pair<QString, QString>> fragments;
    std::vector<Pattern> patterns;
    QStringList missing;
    QSet<QString> missingSet;
    bool flushPending = false;
};

inline State &state()
{
    static State s;
    return s;
}

/// Removes "&" keyboard accelerators ("&Open" -> "Open"), but keeps "A & B".
inline QString stripAccel(const QString &s)
{
    static const QRegularExpression re(
        QStringLiteral("&(?=[\\p{L}\\p{N}])"));
    QString out = s;
    out.remove(re);
    return out;
}

inline QString normalize(const QString &s)
{
    return stripAccel(s).simplified().toLower();
}

inline QString dataDir()
{
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty())
    {
        base = QDir::homePath();
    }
    return QDir(base).filePath(QStringLiteral("Chatterino2"));
}

inline void addEntry(State &s, const QString &en, const QString &uk)
{
    if (en.isEmpty() || uk.isEmpty())
    {
        return;
    }

    s.exact.insert(en, uk);
    s.normalized.insert(normalize(en), uk);

    static const QRegularExpression placeholder(QStringLiteral("%(\\d)"));
    if (en.contains(placeholder))
    {
        QString pat = QStringLiteral("^");
        QList<int> nums;
        qsizetype last = 0;
        auto it = placeholder.globalMatch(en);
        while (it.hasNext())
        {
            const auto m = it.next();
            pat += QRegularExpression::escape(
                en.mid(last, m.capturedStart() - last));
            pat += QStringLiteral("(.+?)");
            nums.push_back(m.captured(1).toInt());
            last = m.capturedEnd();
        }
        pat += QRegularExpression::escape(en.mid(last));
        pat += QStringLiteral("$");

        QRegularExpression re(pat,
                              QRegularExpression::DotMatchesEverythingOption);
        if (re.isValid())
        {
            s.patterns.push_back({re, uk, nums});
        }
    }

    if (en.size() >= 12)
    {
        s.fragments.emplace_back(en, uk);
    }
}

inline void loadExternal(State &s)
{
    QFile f(QDir(dataDir()).filePath(QStringLiteral("uk_extra.txt")));
    if (!f.open(QIODevice::ReadOnly))
    {
        return;
    }

    const QString all = QString::fromUtf8(f.readAll());
    const auto lines = all.split(QLatin1Char('\n'));
    for (QString line : lines)
    {
        if (line.endsWith(QLatin1Char('\r')))
        {
            line.chop(1);
        }
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const auto tab = line.indexOf(QLatin1Char('\t'));
        if (tab <= 0)
        {
            continue;
        }
        QString en = line.left(tab);
        QString uk = line.mid(tab + 1);
        en.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        uk.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        addEntry(s, en, uk);
    }
}

inline State &loaded()
{
    auto &s = state();
    if (!s.loaded)
    {
        s.loaded = true;
        for (const auto &e : UK_ENTRIES)
        {
            addEntry(s, QString::fromUtf8(e.en), QString::fromUtf8(e.uk));
        }
        loadExternal(s);

        // longest first, so that long phrases win over their parts
        std::stable_sort(s.fragments.begin(), s.fragments.end(),
                         [](const auto &a, const auto &b) {
                             return a.first.size() > b.first.size();
                         });
    }
    return s;
}

inline bool hasCyrillic(const QString &s)
{
    for (const QChar ch : s)
    {
        if (ch.unicode() >= 0x0400 && ch.unicode() <= 0x04FF)
        {
            return true;
        }
    }
    return false;
}

inline void flushMissing()
{
    auto &s = state();
    s.flushPending = false;

    QDir().mkpath(dataDir());
    QFile f(QDir(dataDir()).filePath(QStringLiteral("untranslated.txt")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return;
    }
    for (QString line : s.missing)
    {
        line.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
        f.write(line.toUtf8());
        f.write("\n");
    }
}

inline void recordMissing(const QString &text)
{
    auto &s = state();
    if (text.size() < 3 || text.size() > 300 || s.missing.size() > 3000)
    {
        return;
    }
    if (hasCyrillic(text))
    {
        return;
    }

    // at least three ASCII letters in a row
    int run = 0;
    bool letters = false;
    for (const QChar ch : text)
    {
        const char16_t u = ch.unicode();
        if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z'))
        {
            if (++run >= 3)
            {
                letters = true;
                break;
            }
        }
        else
        {
            run = 0;
        }
    }
    if (!letters)
    {
        return;
    }

    const QString key = stripAccel(text);
    if (s.missingSet.contains(key))
    {
        return;
    }
    s.missingSet.insert(key);
    s.missing.push_back(key);

    if (!s.flushPending)
    {
        s.flushPending = true;
        QTimer::singleShot(3000, QCoreApplication::instance(), [] {
            flushMissing();
        });
    }
}

inline bool active()
{
    auto *settings = getSettings();
    return settings != nullptr &&
           settings->uiLanguage.getValue() == QStringLiteral("uk");
}

}  // namespace detail

/// Translates `text`. `found` (optional) tells if a translation was applied.
inline QString translate(const QString &text, bool *found = nullptr)
{
    if (found != nullptr)
    {
        *found = false;
    }
    if (text.trimmed().isEmpty())
    {
        return text;
    }

    auto &s = detail::loaded();
    auto hit = [&](const QString &uk) {
        if (found != nullptr)
        {
            *found = true;
        }
        return uk;
    };

    // 1. exact match
    if (auto it = s.exact.constFind(text); it != s.exact.constEnd())
    {
        return hit(*it);
    }

    // 2. ignoring accelerators, case and whitespace differences
    if (auto it = s.normalized.constFind(detail::normalize(text));
        it != s.normalized.constEnd())
    {
        return hit(*it);
    }

    // 3. "Some label:" (drop-down labels get a ':' appended in code)
    if (text.endsWith(QLatin1Char(':')) && text.size() > 2)
    {
        const QString base = text.chopped(1);
        if (auto it = s.exact.constFind(base); it != s.exact.constEnd())
        {
            return hit(*it + QLatin1Char(':'));
        }
        if (auto it = s.normalized.constFind(detail::normalize(base));
            it != s.normalized.constEnd())
        {
            return hit(*it + QLatin1Char(':'));
        }
    }

    // 4. texts with arguments, like "Send message as %1..."
    const QString plain = detail::stripAccel(text);
    for (const auto &p : s.patterns)
    {
        const auto m = p.re.match(plain);
        if (m.hasMatch())
        {
            QString out = p.replacement;
            for (int i = 0; i < p.placeholders.size(); ++i)
            {
                out.replace(QStringLiteral("%") +
                                QString::number(p.placeholders[i]),
                            m.captured(i + 1));
            }
            return hit(out);
        }
    }

    // 5. long texts built from several parts (descriptions with links, ...)
    if (text.size() >= 12)
    {
        QString out = text;
        bool changed = false;
        for (const auto &[en, uk] : s.fragments)
        {
            if (out.contains(en))
            {
                out.replace(en, uk);
                changed = true;
            }
        }
        if (changed)
        {
            return hit(out);
        }
    }

    return text;
}

/// Translates `text` if the Ukrainian UI language is selected.
inline QString tr(const QString &text)
{
    return detail::active() ? translate(text) : text;
}

namespace detail {

template <typename Setter>
inline void apply(const QString &src, bool log, Setter &&set)
{
    if (src.isEmpty())
    {
        return;
    }
    bool found = false;
    const QString out = translate(src, &found);
    if (found)
    {
        if (out != src)
        {
            set(out);
        }
    }
    else if (log)
    {
        recordMissing(src);
    }
}

inline void translateCombo(QComboBox *combo)
{
    // Combo boxes created by GeneralPageView and SettingWidget keep their
    // real value in the item index/data (and, for the former, in the ORIGINAL
    // text role, see GeneralPageView.hpp), so translating the visible text is
    // safe there. Other combo boxes may use the text as the value: skip them.
    const bool pageCombo = dynamic_cast<ComboBox *>(combo) != nullptr;

    const QSignalBlocker blocker(combo);
    for (int i = 0; i < combo->count(); ++i)
    {
        const bool hasData = combo->itemData(i, Qt::UserRole).isValid();
        if (!hasData && !pageCombo)
        {
            continue;
        }

        const QVariant original = combo->itemData(i, ORIGINAL_TEXT_ROLE);
        const QString src =
            original.isValid() ? original.toString() : combo->itemText(i);

        bool found = false;
        const QString out = translate(src, &found);
        if (!found)
        {
            continue;
        }
        if (!original.isValid())
        {
            combo->setItemData(i, src, ORIGINAL_TEXT_ROLE);
        }
        if (combo->itemText(i) != out)
        {
            combo->setItemText(i, out);
        }
    }
}

inline void translateHeader(QHeaderView *header)
{
    auto *model = header->model();
    if (model == nullptr)
    {
        return;
    }

    const auto orientation = header->orientation();
    const int count = orientation == Qt::Horizontal ? model->columnCount()
                                                    : model->rowCount();
    bool changed = false;
    for (int i = 0; i < count; ++i)
    {
        const QString src =
            model->headerData(i, orientation, Qt::DisplayRole).toString();
        apply(src, false, [&](const QString &out) {
            model->setHeaderData(i, orientation, out, Qt::DisplayRole);
            changed = true;
        });
    }

    // column widths were computed for the English texts: grow where needed
    if (changed && orientation == Qt::Horizontal)
    {
        for (int i = 0; i < count; ++i)
        {
            if (header->isSectionHidden(i))
            {
                continue;
            }
            const int need = header->sectionSizeHint(i);
            if (header->sectionSize(i) < need)
            {
                header->resizeSection(i, need);
            }
        }
    }
}

inline void translateTabBar(QTabBar *tabs)
{
    for (int i = 0; i < tabs->count(); ++i)
    {
        apply(tabs->tabText(i), true, [&](const QString &out) {
            tabs->setTabText(i, out);
        });
    }
}

inline void translateMenu(QMenu *menu)
{
    for (QAction *action : menu->actions())
    {
        if (action == nullptr || action->isSeparator() ||
            dynamic_cast<QWidgetAction *>(action) != nullptr)
        {
            continue;
        }
        apply(action->text(), true, [&](const QString &out) {
            action->setText(out);
        });
    }
}

inline void translateWidget(QWidget *w)
{
    if (auto *label = dynamic_cast<QLabel *>(w))
    {
        apply(label->text(), true, [&](const QString &out) {
            label->setText(out);
        });
    }
    else if (auto *button = dynamic_cast<QAbstractButton *>(w))
    {
        apply(button->text(), true, [&](const QString &out) {
            button->setText(out);
        });
    }
    else if (auto *group = dynamic_cast<QGroupBox *>(w))
    {
        apply(group->title(), true, [&](const QString &out) {
            group->setTitle(out);
        });
    }
    else if (auto *edit = dynamic_cast<QLineEdit *>(w))
    {
        apply(edit->placeholderText(), false, [&](const QString &out) {
            edit->setPlaceholderText(out);
        });
    }
    else if (auto *combo = dynamic_cast<QComboBox *>(w))
    {
        translateCombo(combo);
    }
    else if (auto *header = dynamic_cast<QHeaderView *>(w))
    {
        translateHeader(header);
    }
    else if (auto *tabs = dynamic_cast<QTabBar *>(w))
    {
        translateTabBar(tabs);
    }
    else if (auto *menu = dynamic_cast<QMenu *>(w))
    {
        translateMenu(menu);
    }

    apply(w->toolTip(), false, [&](const QString &out) {
        w->setToolTip(out);
    });

    if (w->isWindow())
    {
        apply(w->windowTitle(), false, [&](const QString &out) {
            w->setWindowTitle(out);
        });
    }
}

class Filter : public QObject
{
public:
    explicit Filter(QObject *parent)
        : QObject(parent)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const auto type = event->type();
        if ((type == QEvent::Show || type == QEvent::Polish) &&
            watched->isWidgetType() && active())
        {
            auto *widget = static_cast<QWidget *>(watched);
            if (type == QEvent::Show ||
                dynamic_cast<QMenu *>(widget) != nullptr)
            {
                translateWidget(widget);
            }
        }
        return false;
    }
};

}  // namespace detail

/// Installs the translation filter on the application (only once).
inline void install()
{
    static bool done = false;
    auto *app = QCoreApplication::instance();
    if (done || app == nullptr)
    {
        return;
    }
    done = true;
    app->installEventFilter(new detail::Filter(app));
}

}  // namespace chatterino::uitr
