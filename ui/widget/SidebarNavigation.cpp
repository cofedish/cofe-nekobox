#include "SidebarNavigation.h"

#include <QEvent>
#include <QIcon>

SidebarNavigation::SidebarNavigation(QWidget *parent)
    : QTreeWidget(parent) {
    setObjectName("drawer_tree");
    setColumnCount(1);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setUniformRowHeights(false);
    setIndentation(18);
    setMouseTracking(true);
    setAnimated(false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setTextElideMode(Qt::ElideRight);
    setIconSize(QSize(20, 20));
    setExpandsOnDoubleClick(false);
    viewport()->setAttribute(Qt::WA_Hover, true);
    viewport()->setMouseTracking(true);

    connect(this, &QTreeWidget::itemEntered, this, [this](QTreeWidgetItem *item, int) {
        hovered_item_ = item;
        refreshThemeIcons();
    });
}

void SidebarNavigation::setCollapsed(bool collapsed) {
    if (collapsed_ == collapsed) return;
    collapsed_ = collapsed;
    applyCollapsedState();
}

bool SidebarNavigation::isCollapsed() const {
    return collapsed_;
}

void SidebarNavigation::refreshThemeIcons() {
    const auto refreshItem = [this](QTreeWidgetItem *item) {
        if (item == nullptr) return;
        const auto glyphValue = item->data(0, GlyphRole).toInt();
        Icon::SidebarIconState state = Icon::SidebarIconState::Normal;
        if (item == currentItem()) {
            state = Icon::SidebarIconState::Active;
        } else if (item == hovered_item_) {
            state = Icon::SidebarIconState::Hovered;
        }
        item->setIcon(0, QIcon(Icon::GetSidebarIconPixmap(static_cast<Icon::SidebarGlyph>(glyphValue), palette(), state, iconSize().width())));
    };

    for (int i = 0; i < topLevelItemCount(); ++i) {
        auto *root = topLevelItem(i);
        refreshItem(root);
        for (int j = 0; j < root->childCount(); ++j) {
            refreshItem(root->child(j));
        }
    }
}

bool SidebarNavigation::event(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        refreshThemeIcons();
    } else if (event->type() == QEvent::Leave) {
        hovered_item_ = nullptr;
        refreshThemeIcons();
    }
    return QTreeWidget::event(event);
}

void SidebarNavigation::applyCollapsedState() {
    setIndentation(collapsed_ ? 0 : 18);
    setIconSize(collapsed_ ? QSize(20, 20) : QSize(18, 18));

    for (int i = 0; i < topLevelItemCount(); ++i) {
        auto *root = topLevelItem(i);
        if (root == nullptr) continue;
        const auto label = root->data(0, LabelRole).toString();
        root->setText(0, collapsed_ ? QString() : label);
        root->setToolTip(0, label);
        root->setExpanded(!collapsed_);

        for (int j = 0; j < root->childCount(); ++j) {
            auto *child = root->child(j);
            if (child == nullptr) continue;
            const auto childLabel = child->data(0, LabelRole).toString();
            child->setHidden(collapsed_);
            child->setText(0, childLabel);
            child->setToolTip(0, collapsed_ ? childLabel : QString());
        }
    }
}
