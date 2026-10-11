// SPDX-License-Identifier: MIT

#pragma once

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QTableView>
#include <QTimer>

#include <algorithm>
#include <vector>

namespace chatterino {

/// Makes every column of `table` resizable by dragging its border.
///
/// Settings tables used `Fixed` columns plus one `Stretch` column, so nothing
/// could be dragged (and the stretched column reacted in a confusing way).
/// Here all columns are `Interactive`. After the page set its own initial
/// widths, the `mainColumns` get the remaining width (at least 120 px each).
inline void makeColumnsResizable(QTableView *table,
                                 std::vector<int> mainColumns)
{
    auto *header = table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);

    QTimer::singleShot(150, table, [table, mainColumns] {
        if (table->model() == nullptr || mainColumns.empty())
        {
            return;
        }

        const int count = table->model()->columnCount();
        int others = 0;
        for (int i = 0; i < count; ++i)
        {
            if (table->isColumnHidden(i) ||
                std::find(mainColumns.begin(), mainColumns.end(), i) !=
                    mainColumns.end())
            {
                continue;
            }
            others += table->columnWidth(i);
        }

        const int available = table->viewport()->width();
        const int each = std::max(
            120, (available - others) / static_cast<int>(mainColumns.size()));
        for (const int column : mainColumns)
        {
            table->setColumnWidth(column, each);
        }
    });
}

}  // namespace chatterino
