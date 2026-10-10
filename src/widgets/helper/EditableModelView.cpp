// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/EditableModelView.hpp"

#include "singletons/Settings.hpp"
#include "widgets/helper/RegExpItemDelegate.hpp"
#include "widgets/helper/TableStyles.hpp"

#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <typeinfo>

namespace chatterino {

EditableModelView::EditableModelView(QAbstractTableModel *model, bool movable)
    : tableView_(new QTableView(this))
    , model_(model)
{
    this->model_->setParent(this);
    this->tableView_->setModel(this->model_);
    // disabling word-wrap somehow prevent '/'-prefixed commands from being elided
    this->tableView_->setWordWrap(false);
    this->tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    this->tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->tableView_->setDragDropMode(QTableView::DragDropMode::InternalMove);
    this->tableView_->setDragDropOverwriteMode(false);
    this->tableView_->setDefaultDropAction(Qt::DropAction::MoveAction);
    this->tableView_->verticalHeader()->setVisible(false);
    this->tableView_->horizontalHeader()->setSectionsClickable(false);

    TableRowDragStyle::applyTo(this->tableView_);

    // create layout
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);

    // create button layout
    auto *buttons = new QHBoxLayout();
    this->buttons_ = buttons;
    vbox->addLayout(buttons);

    // add
    QPushButton *add = new QPushButton("Add");
    buttons->addWidget(add);
    QObject::connect(add, &QPushButton::clicked, [this] {
        this->addButtonPressed.invoke();
    });

    // remove
    QPushButton *remove = new QPushButton("Remove");
    buttons->addWidget(remove);
    QObject::connect(remove, &QPushButton::clicked, [this] {
        auto selected = this->getTableView()->selectionModel()->selectedRows(0);

        // Remove rows backwards so indices don't shift.
        std::vector<int> rows;
        for (auto &&index : selected)
        {
            rows.push_back(index.row());
        }

        std::sort(rows.begin(), rows.end(), std::greater{});

        for (auto &&row : rows)
        {
            this->model_->removeRow(row);
        }
    });

    if (movable)
    {
        // move up
        QPushButton *moveUp = new QPushButton("Move up");
        buttons->addWidget(moveUp);
        QObject::connect(moveUp, &QPushButton::clicked, this, [this] {
            this->moveRow(-1);
        });

        // move down
        QPushButton *moveDown = new QPushButton("Move down");
        buttons->addWidget(moveDown);
        QObject::connect(moveDown, &QPushButton::clicked, this, [this] {
            this->moveRow(1);
        });
    }

    buttons->addStretch();

    QObject::connect(this->model_, &QAbstractTableModel::rowsMoved, this,
                     [this](const QModelIndex &parent, int start, int end,
                            const QModelIndex &destination, int row) {
                         this->tableView_->selectRow(row);
                     });

    // select freshly added row
    QObject::connect(this->model_, &QAbstractTableModel::rowsInserted, this,
                     [this](const QModelIndex &parent, int first, int last) {
                         this->tableView_->selectRow(last);
                     });

    // add tableview
    vbox->addWidget(this->tableView_);

    // finish button layout
    buttons->addStretch(1);

    // Pages set up their columns right after creating the view, so the header
    // is adjusted a little later.
    this->saveTimer_ = new QTimer(this);
    this->saveTimer_->setSingleShot(true);
    this->saveTimer_->setInterval(600);
    QObject::connect(this->saveTimer_, &QTimer::timeout, this, [this] {
        this->saveHeaderState();
    });
    QTimer::singleShot(100, this, [this] {
        this->setupHeader();
    });
}

QString EditableModelView::headerStateKey() const
{
    QString type = QString::fromLatin1(typeid(*this->model_).name());
    for (auto &c : type)
    {
        if (!c.isLetterOrNumber())
        {
            c = '_';
        }
    }
    return QStringLiteral("/ui/tableState/%1_%2")
        .arg(type)
        .arg(this->model_->columnCount());
}

void EditableModelView::fitHeaderTexts()
{
    auto *header = this->tableView_->horizontalHeader();
    for (int i = 0; i < header->count(); i++)
    {
        if (header->isSectionHidden(i) ||
            header->sectionResizeMode(i) == QHeaderView::Stretch)
        {
            continue;
        }

        // The header text must always fit into its column
        const int wanted = header->sectionSizeHint(i) + 12;
        if (header->sectionSize(i) < wanted)
        {
            header->resizeSection(i, wanted);
        }
    }
}

void EditableModelView::setupHeader()
{
    auto *header = this->tableView_->horizontalHeader();

    header->setSectionsMovable(true);
    for (int i = 0; i < header->count(); i++)
    {
        if (header->sectionResizeMode(i) == QHeaderView::Fixed)
        {
            header->setSectionResizeMode(i, QHeaderView::Interactive);
        }
    }

    this->fitHeaderTexts();

    pajlada::Settings::Setting<QString> state(
        this->headerStateKey().toStdString(), QString());
    const QString saved = state.getValue();
    if (!saved.isEmpty())
    {
        header->restoreState(QByteArray::fromBase64(saved.toLatin1()));
        this->fitHeaderTexts();
    }

    QObject::connect(header, &QHeaderView::sectionResized, this, [this] {
        this->saveTimer_->start();
    });
    QObject::connect(header, &QHeaderView::sectionMoved, this, [this] {
        this->saveTimer_->start();
    });
}

void EditableModelView::saveHeaderState()
{
    const auto data =
        this->tableView_->horizontalHeader()->saveState().toBase64();

    pajlada::Settings::Setting<QString> state(
        this->headerStateKey().toStdString(), QString());
    state.setValue(QString::fromLatin1(data));
    getSettings()->requestSave();
}
void EditableModelView::setValidationRegexp(QRegularExpression regexp)
{
    this->tableView_->setItemDelegate(new RegExpItemDelegate(this, regexp));
}

void EditableModelView::setTitles(std::initializer_list<QString> titles)
{
    int i = 0;
    for (const QString &title : titles)
    {
        if (this->model_->columnCount() == i)
        {
            break;
        }

        this->model_->setHeaderData(i++, Qt::Horizontal, title,
                                    Qt::DisplayRole);
    }
}

QTableView *EditableModelView::getTableView()
{
    return this->tableView_;
}

QAbstractTableModel *EditableModelView::getModel()
{
    return this->model_;
}

void EditableModelView::addCustomButton(QWidget *widget)
{
    this->buttons_->addWidget(widget);
}

void EditableModelView::addRegexHelpLink()
{
    auto *regexHelpLabel =
        new QLabel("<a href='"
                   "https://chatterino.com/help/regex'>"
                   "<span style='color:#99f'>regex info</span></a>");
    regexHelpLabel->setOpenExternalLinks(true);
    this->addCustomButton(regexHelpLabel);
}

bool EditableModelView::filterSearchResults(const QString &query,
                                            std::span<const int> columnSelect)
{
    bool searchFoundSomething = false;
    auto rowAmount = this->model_->rowCount();

    // make sure to show the page even if the table is empty,
    // but only if we aren't search something
    if (rowAmount == 0 && query.isEmpty())
    {
        return true;
    }

    for (int i = 0; i < rowAmount; i++)
    {
        bool foundMatch = false;

        for (int j : columnSelect)
        {
            QModelIndex idx = this->model_->index(i, j);
            QVariant a = this->model_->data(idx);
            if (a.toString().contains(query, Qt::CaseInsensitive))
            {
                foundMatch = true;
                searchFoundSomething = true;
                break;
            }
        }

        this->tableView_->setRowHidden(i, !foundMatch);
    }

    return searchFoundSomething;
}

void EditableModelView::filterSearchResultsHotkey(
    const QKeySequence &keySequenceQuery)
{
    auto rowAmount = this->model_->rowCount();

    for (int i = 0; i < rowAmount; i++)
    {
        QModelIndex idx = this->model_->index(i, 1);
        QVariant a = this->model_->data(idx);
        auto seq = qvariant_cast<QKeySequence>(a);

        // todo: Make this fuzzy match, right now only exact matches happen
        // so ctrl+f won't match ctrl+shift+f shortcuts
        if (keySequenceQuery.matches(seq) != QKeySequence::NoMatch)
        {
            this->tableView_->showRow(i);
        }
        else
        {
            this->tableView_->hideRow(i);
        }
    }
}

void EditableModelView::moveRow(int dir)
{
    auto selected = this->getTableView()->selectionModel()->selectedRows(0);

    int row;
    if (selected.size() == 0 ||
        (row = selected.at(0).row()) + dir >=
            this->model_->rowCount(QModelIndex()) ||
        row + dir < 0)
    {
        return;
    }

    this->model_->moveRows(this->model_->index(row, 0), row, selected.size(),
                           this->model_->index(row + dir, 0), row + dir);
    this->tableView_->selectRow(row + dir);
}

}  // namespace chatterino
