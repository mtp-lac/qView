#include <QtTest>

#include "actionmanager.h"
#include "mainwindow.h"
#include "qvapplication.h"
#include "qvgraphicsview.h"
#include "settingsmanager.h"
#include "shortcutmanager.h"

#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>

class ActionManagerTests : public QObject
{
    Q_OBJECT

public:
    ActionManagerTests();
    ~ActionManagerTests();

private slots:
    void testClonedActionsUntracked();

    void testZoomPercentageActionExists();

    void testZoomPercentageShortcutIsZ();

    void testFormatZoomPercentage();

    void testSetZoomPercentage();

    void testWindowTitleShowsZoomPercentage();
};

ActionManagerTests::ActionManagerTests() { }

ActionManagerTests::~ActionManagerTests() { }

void ActionManagerTests::testClonedActionsUntracked()
{
    // Get initial counts of certain actions
    int fullscreenCount = qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length();
    int openCount = qvApp->getActionManager().getAllInstancesOfAction("open").length();
    qDebug() << fullscreenCount;

    // Have window clone actions
    MainWindow window;
    window.show();
    // Make sure they were cloned
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length()
            != fullscreenCount);
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("open").length() != openCount);
    // Untrack them
    window.close();

    // Make sure the count has not changed from the initial
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length(),
             fullscreenCount);
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("open").length(), openCount);
}

void ActionManagerTests::testZoomPercentageActionExists()
{
    // The action must be registered so that it can be cloned into menus and windows
    QVERIFY(qvApp->getActionManager().getActionLibrary().contains("setzoompercentage"));

    MainWindow window;
    window.show();

    const auto clonedActions =
            qvApp->getActionManager().getAllClonesOfAction("setzoompercentage", &window);
    QVERIFY(!clonedActions.isEmpty());

    // No image is loaded, so setting a zoom level must not be available
    for (const auto &action : clonedActions) {
        QVERIFY(!action->isEnabled());
    }

    window.close();
}

void ActionManagerTests::testZoomPercentageShortcutIsZ()
{
    bool foundShortcut = false;
    const auto &shortcutsList = qvApp->getShortcutManager().getShortcutsList();
    for (const auto &shortcut : shortcutsList) {
        if (shortcut.name != "setzoompercentage")
            continue;

        foundShortcut = true;
        QVERIFY(shortcut.defaultShortcuts.contains(QKeySequence(Qt::Key_Z).toString()));
    }

    QVERIFY(foundShortcut);
}

void ActionManagerTests::testFormatZoomPercentage()
{
    QCOMPARE(MainWindow::formatZoomPercentage(100.0), QString("100%"));
    QCOMPARE(MainWindow::formatZoomPercentage(1.0), QString("1%"));
    QCOMPARE(MainWindow::formatZoomPercentage(25.0), QString("25%"));
    QCOMPARE(MainWindow::formatZoomPercentage(250.0), QString("250%"));
    QCOMPARE(MainWindow::formatZoomPercentage(12.5), QString("12.5%"));
    QCOMPARE(MainWindow::formatZoomPercentage(33.333), QString("33.3%"));
}

void ActionManagerTests::testSetZoomPercentage()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString imagePath = QDir(temporaryDir.path()).filePath("zoomtest.png");
    QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::blue);
    if (!image.save(imagePath, "png"))
        QSKIP("No image plugin available to write a png test image");

    // The view is smaller than the image, so fit-to-window is below 100%
    QVGraphicsView view;
    view.resize(480, 360);
    view.show();

    QSignalSpy fileChangedSpy(&view, &QVGraphicsView::fileChanged);
    view.loadFile(imagePath);
    QVERIFY(fileChangedSpy.wait(10000));

    const qreal fitPercentage = view.getZoomPercentage();
    QVERIFY(fitPercentage > 0.0);
    QVERIFY(fitPercentage < 100.0);

    // The reachable range has to contain actual size for this test to be meaningful
    QVERIFY(view.getMinZoomPercentage() < 100.0);
    QVERIFY(view.getMaxZoomPercentage() > 100.0);

    // Zooming to a specific percentage lands on that percentage
    view.setZoomPercentage(100.0);
    QVERIFY(qAbs(view.getZoomPercentage() - 100.0) < 0.01);

    view.setZoomPercentage(12.5);
    QVERIFY(qAbs(view.getZoomPercentage() - 12.5) < 0.01);

    view.setZoomPercentage(275.0);
    QVERIFY(qAbs(view.getZoomPercentage() - 275.0) < 0.01);

    // Requested values are clamped to the reachable range
    view.setZoomPercentage(1000000.0);
    const qreal clampedMaxPercentage = view.getZoomPercentage();
    QVERIFY(clampedMaxPercentage <= view.getMaxZoomPercentage());
    QVERIFY(clampedMaxPercentage > view.getMaxZoomPercentage() * 0.999);

    view.setZoomPercentage(0.0000001);
    const qreal clampedMinPercentage = view.getZoomPercentage();
    QVERIFY(clampedMinPercentage >= view.getMinZoomPercentage());
    QVERIFY(clampedMinPercentage < view.getMinZoomPercentage() * 1.001);
}

void ActionManagerTests::testWindowTitleShowsZoomPercentage()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString imagePath = QDir(temporaryDir.path()).filePath("zoomtest.png");
    QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    if (!image.save(imagePath, "png"))
        QSKIP("No image plugin available to write a png test image");

    MainWindow window;

    // Nothing is loaded yet, so no zoom level is shown
    QCOMPARE(window.windowTitle(), QString("qView"));

    window.openFile(imagePath);
    QTRY_VERIFY(window.getIsPixmapLoaded());

    // Going to the original size always means 100%, whatever the window size is
    window.originalSize();
    QVERIFY(window.windowTitle().contains("100%"));

    // Zooming in updates the title immediately, through the regular zoom command
    const int scaleFactor =
            qvApp->getSettingsManager().getInt(SettingsManager::Setting::ScaleFactor);
    window.zoomIn();
    QVERIFY(window.windowTitle().contains(MainWindow::formatZoomPercentage(100.0 + scaleFactor)));
}

int main(int argc, char *argv[])
{
    QVApplication app(argc, argv);
    ActionManagerTests actionManagerTests;
    return QTest::qExec(&actionManagerTests, argc, argv);
}

#include "tst_actionmanagertests.moc"
