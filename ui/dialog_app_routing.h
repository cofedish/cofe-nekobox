#pragma once

#include "main/ProcessRoutingRules.hpp"

#include <QDialog>
#include <QList>

class QButtonGroup;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPoint;
class QPushButton;
class QRadioButton;
class QTableWidget;

class DialogAppRouting : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        AllThroughProxy,
        OnlySelected,
        ExceptSelected
    };

    explicit DialogAppRouting(QWidget *parent = nullptr);
    ~DialogAppRouting() override = default;

public slots:
    void accept() override;

private:
    QButtonGroup *modeGroup_ = nullptr;
    QRadioButton *modeAllRadio_ = nullptr;
    QRadioButton *modeAllowRadio_ = nullptr;
    QRadioButton *modeBlockRadio_ = nullptr;
    QLabel *modeHintLabel_ = nullptr;
    QGroupBox *rulesGroup_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QTableWidget *rulesTable_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *removeButton_ = nullptr;
    QPushButton *importButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QPushButton *checkButton_ = nullptr;
    QList<AppRoutingRules::Entry> rules_;

    Mode currentMode() const;
    void loadFromDataStore();
    void syncModeUi();
    void refreshRulesTable();
    void updateFilter();
    void addRule();
    void removeSelectedRules();
    void showRulesContextMenu(const QPoint &pos);
    void importRules();
    void exportRules();
    void checkRuleRouting();
};
