// SPDX-License-Identifier: MIT

#pragma once

#include "Application.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>

namespace chatterino {

namespace ytpage_detail {

struct TwitchColor {
    const char *name;
    const char *hex;
};

/// The default name colors of Twitch chat.
inline constexpr std::array<TwitchColor, 15> TWITCH_COLORS{{
    {"Red", "#ff0000"},
    {"Blue", "#0000ff"},
    {"Green", "#008000"},
    {"Firebrick", "#b22222"},
    {"Coral", "#ff7f50"},
    {"YellowGreen", "#9acd32"},
    {"OrangeRed", "#ff4500"},
    {"SeaGreen", "#2e8b57"},
    {"GoldenRod", "#daa520"},
    {"Chocolate", "#d2691e"},
    {"CadetBlue", "#5f9ea0"},
    {"DodgerBlue", "#1e90ff"},
    {"HotPink", "#ff69b4"},
    {"BlueViolet", "#8a2be2"},
    {"SpringGreen", "#00ff7f"},
}};

}  // namespace ytpage_detail

/// List of "YouTube user -> fixed name color" rules, stored as JSON in the
/// `ytUserColors` setting:  [{"name": "...", "color": "#rrggbb", "enabled": true}]
class YtUserColorsEditor : public QWidget
{
public:
    explicit YtUserColorsEditor(QStringSetting &setting)
        : setting_(setting)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        this->rows_ = new QVBoxLayout;
        layout->addLayout(this->rows_);

        auto *add = new QPushButton("Add user");
        layout->addWidget(add, 0, Qt::AlignLeft);
        QObject::connect(add, &QPushButton::clicked, this, [this] {
            this->addRow({}, "#ff0000", true);
            this->save();
        });

        const auto doc =
            QJsonDocument::fromJson(this->setting_.getValue().toUtf8());
        for (const auto &value : doc.array())
        {
            const auto obj = value.toObject();
            this->addRow(obj["name"].toString(),
                         obj["color"].toString("#ff0000"),
                         obj["enabled"].toBool(true));
        }
    }

private:
    void addRow(const QString &name, const QString &color, bool enabled)
    {
        auto *row = new QWidget;
        row->setProperty("color", color);

        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);

        auto *on = new QCheckBox;
        on->setChecked(enabled);
        on->setToolTip(
            "Use this color for the user. When off, the color from YouTube "
            "is used.");

        auto *edit = new QLineEdit(name);
        edit->setPlaceholderText("YouTube name or channel ID");
        edit->setMinimumWidth(240);

        auto *button = new ColorButton(QColor(color));

        auto *remove = new QPushButton("Remove");

        h->addWidget(on);
        h->addWidget(edit, 1);
        h->addWidget(button);
        h->addWidget(remove);
        this->rows_->addWidget(row);

        QObject::connect(on, &QCheckBox::toggled, this, [this] {
            this->save();
        });
        QObject::connect(edit, &QLineEdit::textChanged, this, [this] {
            this->save();
        });
        QObject::connect(button, &ColorButton::clicked, this,
                         [this, button, row] {
                             this->pickColor(button, row);
                         });
        QObject::connect(remove, &QPushButton::clicked, this, [this, row] {
            this->rows_->removeWidget(row);
            row->deleteLater();
            this->save();
        });
    }

    void setColor(ColorButton *button, QWidget *row, const QColor &color)
    {
        button->setColor(color);
        row->setProperty("color", color.name());
        this->save();
    }

    void pickColor(ColorButton *button, QWidget *row)
    {
        auto *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        for (const auto &twitch : ytpage_detail::TWITCH_COLORS)
        {
            QPixmap swatch(14, 14);
            swatch.fill(QColor(twitch.hex));
            auto *action = menu->addAction(QIcon(swatch), twitch.name);
            const QColor color(twitch.hex);
            QObject::connect(action, &QAction::triggered, this,
                             [this, button, row, color] {
                                 this->setColor(button, row, color);
                             });
        }

        menu->addSeparator();
        auto *custom = menu->addAction("Custom...");
        QObject::connect(custom, &QAction::triggered, this,
                         [this, button, row] {
                             auto *dialog = new ColorPickerDialog(
                                 QColor(row->property("color").toString()),
                                 this);
                             QObject::connect(
                                 dialog, &ColorPickerDialog::colorConfirmed,
                                 this, [this, button, row](auto selected) {
                                     if (selected.isValid())
                                     {
                                         this->setColor(button, row, selected);
                                     }
                                 });
                             dialog->show();
                         });

        menu->popup(button->mapToGlobal(QPoint(0, button->height())));
    }

    void save()
    {
        QJsonArray array;
        for (int i = 0; i < this->rows_->count(); ++i)
        {
            auto *row = this->rows_->itemAt(i)->widget();
            if (row == nullptr)
            {
                continue;
            }
            const auto name =
                row->findChild<QLineEdit *>()->text().trimmed();
            if (name.isEmpty())
            {
                continue;
            }

            QJsonObject obj;
            obj["name"] = name;
            obj["color"] = row->property("color").toString();
            obj["enabled"] = row->findChild<QCheckBox *>()->isChecked();
            array.push_back(obj);
        }
        this->setting_.setValue(QString::fromUtf8(
            QJsonDocument(array).toJson(QJsonDocument::Compact)));
    }

    QStringSetting &setting_;
    QVBoxLayout *rows_ = nullptr;
};

/// Settings -> YouTube: options of the yt-chat plugin (read by the plugin
/// through c2.yt_settings()). Header-only (no Q_OBJECT) so that no new .cpp
/// has to be added to the build.
class YouTubePage : public SettingsPage
{
public:
    YouTubePage()
    {
        auto *view = GeneralPageView::withoutNavigation(this);

        auto *y = new QVBoxLayout;
        auto *x = new QHBoxLayout;
        x->addWidget(view);
        auto *z = new QFrame;
        z->setLayout(x);
        y->addWidget(z);
        this->setLayout(y);

        auto &s = *getSettings();

        view->addTitle("YouTube chat");
        view->addDescription(
            "Settings of the yt-chat plugin. They apply to new messages "
            "within a couple of seconds.");

        // --- badges: two check boxes, exactly one of them is always on
        view->addSubtitle("Badges");
        view->addDescription("Show badges of YouTube users as:");

        const bool twitch = s.ytBadgeStyle.getValue() == "twitch";
        auto *youtubeBox = new QCheckBox("YouTube badges");
        auto *twitchBox = new QCheckBox("Twitch badges");
        youtubeBox->setChecked(!twitch);
        twitchBox->setChecked(twitch);

        QObject::connect(youtubeBox, &QCheckBox::clicked, youtubeBox,
                         [&s, youtubeBox, twitchBox](bool on) {
                             if (!on)
                             {
                                 youtubeBox->setChecked(true);
                                 return;
                             }
                             twitchBox->setChecked(false);
                             s.ytBadgeStyle.setValue("youtube");
                         });
        QObject::connect(twitchBox, &QCheckBox::clicked, twitchBox,
                         [&s, youtubeBox, twitchBox](bool on) {
                             if (!on)
                             {
                                 twitchBox->setChecked(true);
                                 return;
                             }
                             youtubeBox->setChecked(false);
                             s.ytBadgeStyle.setValue("twitch");
                         });
        view->addWidget(youtubeBox);
        view->addWidget(twitchBox);
        view->addDescription(
            "With Twitch badges the owner gets the Twitch Broadcaster "
            "badge, moderators get the green sword and verified users get "
            "the Verified badge. Channel member badges always come from "
            "YouTube.");

        // same settings as on the General page (shown here too)
        SettingWidget::checkbox("Show channel badges (owner, moderator)",
                                s.showBadgesChannelAuthority)
            ->addTo(*view);
        SettingWidget::checkbox("Show channel member badges",
                                s.showBadgesSubscription)
            ->addTo(*view);
        SettingWidget::checkbox("Twitch icon", s.showTwitchPlatformIcon)
            ->setTooltip("Small Twitch logo before every Twitch message")
            ->addTo(*view);
        SettingWidget::checkbox("YouTube icon", s.showYoutubePlatformIcon)
            ->setTooltip(
                "Small YouTube logo before every message from the yt-chat "
                "plugin")
            ->addTo(*view);
        s.showTwitchPlatformIcon.connect(
            []() {
                getApp()->getWindows()->forceLayoutChannelViews();
            },
            false);
        s.showYoutubePlatformIcon.connect(
            []() {
                getApp()->getWindows()->forceLayoutChannelViews();
            },
            false);

        // --- events
        view->addSubtitle("Events");
        SettingWidget::checkbox("Show events (new members, Super Chats, gifts)",
                                s.ytShowEvents)
            ->addTo(*view);
        SettingWidget::checkbox("Show leaderboard messages",
                                s.ytShowLeaderboard)
            ->addTo(*view);
        SettingWidget::checkbox("Decorations around events", s.ytEventDecor)
            ->addTo(*view);
        SettingWidget::checkbox("Decorations around leaderboard messages",
                                s.ytLeaderboardDecor)
            ->addTo(*view);
        SettingWidget::checkbox("Show events in the /mentions tab",
                                s.ytEventsInMentions)
            ->addTo(*view);
        SettingWidget::colorButton("Event highlight color", s.ytEventColor)
            ->addTo(*view);
        SettingWidget::colorButton("Leaderboard highlight color",
                                   s.ytLeaderboardColor)
            ->addTo(*view);

        // --- messages
        view->addSubtitle("Messages");
        SettingWidget::intInput("Messages shown on connect",
                                s.ytInitialMessages,
                                {
                                    .min = 0,
                                    .max = 500,
                                    .singleStep = 1,
                                })
            ->setTooltip(
                "How many of the latest messages YouTube sends are shown "
                "when connecting to a stream. 0 shows none.")
            ->addTo(*view);

        // --- connection status messages
        view->addSubtitle("Connection messages");
        SettingWidget::checkbox("Show connection messages",
                                s.ytShowStatusMessages)
            ->addTo(*view);
        view->addDescription(
            "Short outages (for example a 503 error from YouTube) are retried "
            "silently. A message appears only if the problem lasts longer than "
            "the time below, and a second one when the connection is restored.");
        SettingWidget::intInput("Report problems that last longer than (seconds)",
                                s.ytStatusGraceSeconds,
                                {
                                    .min = 0,
                                    .max = 120,
                                    .singleStep = 1,
                                })
            ->addTo(*view);

        SettingWidget::checkbox("Show message reply button", s.showReplyButton)
            ->setTooltip("Show a reply button next to every chat message")
            ->addTo(*view);

        // --- user colors
        view->addSubtitle("User name colors");
        view->addDescription(
            "Set a fixed name color for a YouTube user, for example the same "
            "one they have on Twitch. Type the YouTube name (as shown in "
            "chat) or the channel ID. Turn the check box off to use the "
            "color from YouTube again.");
        view->addWidget(new YtUserColorsEditor(s.ytUserColors));

        // --- links to the other settings that were added in this fork
        view->addSubtitle("Other settings of this fork");
        view->addDescription(
            "These settings are not specific to YouTube. Click to open them:");

        auto *links = new QWidget;
        auto *linksLayout = new QVBoxLayout(links);
        linksLayout->setContentsMargins(0, 0, 0, 0);
        const auto addLink = [&](const QString &text, const QString &tab,
                                 const QString &query) {
            auto *button = new QPushButton(text);
            QObject::connect(button, &QPushButton::clicked, button,
                             [tab, query] {
                                 SettingsDialog::jumpTo(tab, query);
                             });
            linksLayout->addWidget(button, 0, Qt::AlignLeft);
        };
        addLink("General: Window title", "General", "Window title");
        addLink("General: Hide the update button", "General",
                "update button");
        addLink("General: Show send message button", "General",
                "send message button");
        addLink("General: Moderate menu", "General", "moderate menu");
        addLink("General: Message menu", "General", "message menu");
        addLink("Language", "Language", "");
        view->addWidget(links);

        view->addStretch();
    }
};

}  // namespace chatterino
