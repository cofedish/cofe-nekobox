#pragma once

#include <QTreeWidget>

#include "ui/Icon.hpp"

class SidebarNavigation : public QTreeWidget {
    Q_OBJECT

public:
    enum ItemDataRole {
        LabelRole = Qt::UserRole + 1,
        GlyphRole,
        PageIndexRole,
        ActionRole,
        DynamicIndexRole
    };

    explicit SidebarNavigation(QWidget *parent = nullptr);

    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const;
    void refreshThemeIcons();

protected:
    bool event(QEvent *event) override;

private:
    void applyCollapsedState();

    bool collapsed_ = false;
    QTreeWidgetItem *hovered_item_ = nullptr;
};
