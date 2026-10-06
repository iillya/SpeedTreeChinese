#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QListWidget>
#include <QtCore/QTemporaryDir>

// Test the real translation helpers and QPainter wrappers without installing
// process-wide IAT hooks into the test application.
#include "../hook.cpp"

#include <cstdio>

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (false)

static QString g_lastPaintText;
static QString g_lastStylePaintText;
static QString g_lastStyleMeasureText;
static bool unrelatedCacheRetainsSource = true;

class RecordingStyle : public QCommonStyle {
public:
    void drawControl(ControlElement, const QStyleOption*, QPainter* painter,
                     const QWidget*) const override {
        QImage unrelated(180, 50, QImage::Format_ARGB32);
        QPainter exportPainter(&unrelated);
        hookedDrawPoint(&exportPainter, QPoint(4, 24), QStringLiteral("Material"));
        proxy()->drawItemText(&exportPainter, unrelated.rect(), Qt::AlignLeft,
                             QPalette(), true, QStringLiteral("Material"));
        unrelatedCacheRetainsSource = unrelatedCacheRetainsSource &&
            g_lastPaintText == QStringLiteral("Material") && g_lastStylePaintText == QStringLiteral("Material");
        hookedDrawPoint(painter, QPoint(4, 24), QStringLiteral("Material"));
        proxy()->drawItemText(painter, QRect(0, 0, 180, 50), Qt::AlignLeft,
                             QPalette(), true, QStringLiteral("Material"));
    }
    void drawPrimitive(PrimitiveElement, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget) const override {
        drawControl(CE_PushButton, option, painter, widget);
    }
    void drawComplexControl(ComplexControl, const QStyleOptionComplex* option,
                            QPainter* painter, const QWidget* widget) const override {
        drawControl(CE_PushButton, option, painter, widget);
    }
    void drawItemText(QPainter*, const QRect&, int, const QPalette&, bool,
                      const QString& text, QPalette::ColorRole) const override {
        g_lastStylePaintText = text;
    }

    QRect itemTextRect(const QFontMetrics&, const QRect& rect, int, bool,
                       const QString& text) const override {
        g_lastStyleMeasureText = text;
        return rect;
    }
};

static ChineseProxyStyle g_testStyle(new RecordingStyle);

static void __fastcall recordDrawText(QPainter* painter, const QPoint& point,
                                      const QString& text) {
    g_lastPaintText = text;
    painter->drawText(point, text);
}

class ScreenTextWidget : public QWidget {
public:
    using QWidget::QWidget;
    bool paintTargetAccepted = false;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        paintTargetAccepted = visibleWidgetPaintTarget(&painter) == this;
        hookedDrawPoint(&painter, QPoint(4, 24), QStringLiteral("Material"));
        g_testStyle.drawItemText(&painter, rect(), Qt::AlignLeft, palette(),
                                 true, QStringLiteral("Material"));
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    g_translations.emplace("Material", u8"材质");
    g_drawPoint = &recordDrawText;

    const QString source = QStringLiteral("Material");
    const QString translation = QStringLiteral("材质");
    const QString beforeTr = QCoreApplication::translate("host", "Material");
    QFontMetrics metrics(app.font());
    const int sourceWidth = metrics.horizontalAdvance(source);
    const QString sourceElided = metrics.elidedText(source, Qt::ElideRight, 35);

    QLabel label(source);
    QLineEdit input;
    input.setText(source);
    QStandardItemModel model;
    model.appendRow(new QStandardItem(source));
    const QModelIndex index = model.index(0, 0);
    const QVariant sourceValue = index.data(Qt::DisplayRole);
    TranslateDelegate delegate;
    CHECK(delegate.displayText(sourceValue, QLocale()) == translation);
    CHECK(index.data(Qt::DisplayRole) == sourceValue);

    ScreenTextWidget widget;
    widget.resize(180, 50);
    widget.show();
    QApplication::processEvents();
    g_lastPaintText.clear();
    widget.repaint();
    CHECK(widget.paintTargetAccepted);
    CHECK(g_lastPaintText == translation);
    CHECK(g_lastStylePaintText == translation);
    g_testStyle.itemTextRect(metrics, QRect(0, 0, 180, 50), Qt::AlignLeft,
                             true, source);
    CHECK(g_lastStyleMeasureText == translation);

    QImage exportImage(180, 50, QImage::Format_ARGB32);
    exportImage.fill(Qt::transparent);
    {
        QPainter painter(&exportImage);
        CHECK(visibleWidgetPaintTarget(&painter) == nullptr);
        hookedDrawPoint(&painter, QPoint(4, 24), source);
        g_testStyle.drawItemText(&painter, QRect(0, 0, 180, 50), Qt::AlignLeft,
                                 label.palette(), true, source);
    }
    CHECK(g_lastPaintText == source);
    CHECK(g_lastStylePaintText == source);
    QWidget cacheOwner;
    cacheOwner.setAttribute(Qt::WA_DontShowOnScreen);
    cacheOwner.show();
    {
        QPainter painter(&exportImage);
        QStyleOption option;
        QStyleOptionComplex complex;
        g_testStyle.drawControl(QStyle::CE_PushButton, &option, &painter, &cacheOwner);
        CHECK(g_lastPaintText == translation && g_lastStylePaintText == translation);
        g_testStyle.drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, &cacheOwner);
        CHECK(g_lastPaintText == translation && g_lastStylePaintText == translation);
        g_testStyle.drawComplexControl(QStyle::CC_ComboBox, &complex, &painter, &cacheOwner);
        CHECK(g_lastPaintText == translation && g_lastStylePaintText == translation);
        g_testStyle.drawControl(QStyle::CE_PushButton, &option, &painter, nullptr);
        CHECK(g_lastPaintText == source && g_lastStylePaintText == source);
    }
    CHECK(unrelatedCacheRetainsSource);
    QPixmap exportPixmap(180, 50);
    exportPixmap.fill(Qt::transparent);
    {
        QPainter painter(&exportPixmap);
        CHECK(visibleWidgetPaintTarget(&painter) == nullptr);
        hookedDrawPoint(&painter, QPoint(4, 24), source);
    }
    CHECK(g_lastPaintText == source);

    toggleTranslation();
    CHECK(!g_enabled);
    widget.repaint();
    CHECK(g_lastPaintText == source);
    CHECK(g_lastStylePaintText == source);
    toggleTranslation();
    CHECK(g_enabled);
    widget.repaint();
    CHECK(g_lastPaintText == translation);

    CHECK(label.text() == source);
    CHECK(input.text() == source);
    CHECK(index.data(Qt::DisplayRole) == sourceValue);
    CHECK(QCoreApplication::translate("host", "Material") == beforeTr);
    CHECK(QFontMetrics(app.font()).horizontalAdvance(source) == sourceWidth);
    CHECK(QFontMetrics(app.font()).elidedText(source, Qt::ElideRight, 35) == sourceElided);
    {
        QListWidget view;
        applyWidget(&view);
        auto* first = view.itemDelegate();
        CHECK(dynamic_cast<TranslateDelegate*>(first));
        applyWidget(&view);
        CHECK(view.itemDelegate() == first);
        // The host can replace its stock delegate after a panel is rebuilt.
        view.setItemDelegate(new QStyledItemDelegate(&view));
        applyWidget(&view);
        CHECK(dynamic_cast<TranslateDelegate*>(view.itemDelegate()));
        for (int i = 0; i != 100; ++i) {
            QListWidget temporary;
            applyWidget(&temporary);
            CHECK(dynamic_cast<TranslateDelegate*>(temporary.itemDelegate()));
        }
    }
    {
        g_translations.emplace("UV", u8"纹理坐标与投影设置");
        ChineseProxyStyle style(QStyleFactory::create(QStringLiteral("Fusion")));
        QTabBar english, reference;
        english.setStyle(&style);
        reference.setStyle(&style);
        english.addTab(QStringLiteral("UV"));
        reference.addTab(QStringLiteral("纹理坐标与投影设置"));
        for (auto shape : {QTabBar::RoundedNorth, QTabBar::RoundedWest}) {
            english.setShape(shape);
            reference.setShape(shape);
            CHECK(english.sizeHint() == reference.sizeHint());
        }
        english.setShape(QTabBar::RoundedNorth);
        const int chineseWidth = english.sizeHint().width();
        toggleTranslation();
        CHECK(english.sizeHint().width() < chineseWidth);
        CHECK(english.tabText(0) == QStringLiteral("UV"));
        toggleTranslation();
        CHECK(english.sizeHint().width() == chineseWidth);
        CHECK(metrics.horizontalAdvance(source) == sourceWidth);
    }
    {
        QTemporaryDir dictionaries;
        CHECK(dictionaries.isValid());
        QDir root(dictionaries.path());
        CHECK(root.mkpath(QStringLiteral("translations")));
        QFile file(root.filePath(QStringLiteral("translations/test_zh.json")));
        CHECK(file.open(QIODevice::WriteOnly));
        CHECK(file.write(u8R"({"translations":{"File Name":"文件名称","File_Name":"文件标识","Material":"材质"}})") > 0);
        file.close();
        g_plugInDir = dictionaries.path();
        LoadDictionaries();
        CHECK(translated(QStringLiteral("File Name")) == QStringLiteral("文件名称"));
        CHECK(translated(QStringLiteral("File_Name")) == QStringLiteral("文件标识"));
        CHECK(translated(QStringLiteral("FILE NAME")).isEmpty());
        CHECK(translated(QStringLiteral("MATERIAL")) == translation);
        CHECK(g_normCache.size() < 4);
        for (int i = 0; i < 10000; ++i) normalizeForMatch(QString::number(i));
        CHECK(g_normCache.size() <= kMaxNormCache);
    }
    std::puts("PASS UI tab sizing, delegate lifecycle, bounded lookup, language restoration and source-data boundary");
    return 0;
}
