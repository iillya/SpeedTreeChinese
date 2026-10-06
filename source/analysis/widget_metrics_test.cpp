#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../hook.cpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QListWidget>
#include <QtCore/QThread>
#include <cstdio>

static bool enabled = true;
static QString lookup(const QString& text) {
    return enabled && text == QLatin1String("UV")
        ? QStringLiteral("纹理坐标与投影设置") : QString();
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    const QString source = QStringLiteral("UV");
    const QString target = lookup(source);
    g_translations["UV"] = target.toUtf8().toStdString();
    const QFontMetrics fm(app.font());
    const int originalWidth = fm.horizontalAdvance(source);
    const QString originalElide = fm.elidedText(source, Qt::ElideRight, 80);
    WidgetDisplayMetrics::install(lookup);
    installProxyStyle();
    if (!WidgetDisplayMetrics::patchedSlots || !WidgetDisplayMetrics::sizeHooked) return 2;
    if (fm.horizontalAdvance(source) != originalWidth ||
        fm.elidedText(source, Qt::ElideRight, 80) != originalElide) return 3;
    int failures = 0;
    const auto check = [&](const char* type, const QSize& actual, const QSize& expected) {
        const bool same = actual == expected;
        std::printf("%s %s actual=%dx%d expected=%dx%d\n", same ? "PASS" : "FAIL",
            type, actual.width(), actual.height(), expected.width(), expected.height());
        if (!same) ++failures;
    };
    QLabel label(source), labelReference(target);
    QPushButton button(source), buttonReference(target);
    QCheckBox checkBox(source), checkReference(target);
    QRadioButton radio(source), radioReference(target);
    QGroupBox group(source), groupReference(target);
    QTabBar tabs, tabsReference;
    tabs.addTab(source); tabsReference.addTab(target);
    QComboBox combo, comboReference;
    combo.addItem(source); comboReference.addItem(target);
    QMenuBar menu, menuReference;
    menu.addAction(source); menuReference.addAction(target);
    QListWidget list, listReference;
    list.addItem(source); listReference.addItem(target);
    applyWidget(&list);
    check("label", label.sizeHint(), labelReference.sizeHint());
    check("button", button.sizeHint(), buttonReference.sizeHint());
    check("checkbox", checkBox.sizeHint(), checkReference.sizeHint());
    check("radio", radio.sizeHint(), radioReference.sizeHint());
    check("group", group.minimumSizeHint(), groupReference.minimumSizeHint());
    check("tab", tabs.sizeHint(), tabsReference.sizeHint());
    tabs.setShape(QTabBar::RoundedWest); tabsReference.setShape(QTabBar::RoundedWest);
    check("vertical-tab", tabs.sizeHint(), tabsReference.sizeHint());
    check("combo", combo.sizeHint(), comboReference.sizeHint());
    check("menu", menu.sizeHint(), menuReference.sizeHint());
    check("list-row", QSize(list.sizeHintForColumn(0),list.sizeHintForRow(0)),
        QSize(listReference.sizeHintForColumn(0),listReference.sizeHintForRow(0)));
    if (label.text() != source || button.text() != source || tabs.tabText(0) != source ||
        combo.currentText() != source || list.item(0)->text() != source ||
        menu.actions().at(0)->text() != source) ++failures;
    enabled = false;
    toggleTranslation();
    QLabel english(source);
    check("existing-label-English", label.sizeHint(), english.sizeHint());
    if (english.sizeHint().width() >= labelReference.sizeHint().width()) ++failures;
    std::printf("Qt %s: %u widget import slots, source data retained, failures=%d\n",
        qVersion(), WidgetDisplayMetrics::patchedSlots, failures);
    return failures ? 1 : 0;
}
