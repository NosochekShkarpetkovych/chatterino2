// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/Settings.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace chatterino {

/// Settings -> Language: choose the interface language (English / Ukrainian).
/// Header-only (no Q_OBJECT) so that no new .cpp has to be added to the build.
class LanguagePage : public SettingsPage
{
public:
    LanguagePage()
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

        view->addTitle("Language");
        view->addDescription(
            "Interface language. Changes apply fully after restarting "
            "Chatterino.");

        const std::vector<std::pair<QString, QVariant>> languages{
            {QStringLiteral("English"), QVariant(QStringLiteral("en"))},
            {QString::fromUtf8("\320\243\320\272\321\200\320\260\321\227\320\275\321\201\321\214\320\272\320\260"),
             QVariant(QStringLiteral("uk"))},
        };
        SettingWidget::dropdown("Interface language", s.uiLanguage, languages)
            ->addTo(*view);

        view->addStretch();
    }
};

}  // namespace chatterino
