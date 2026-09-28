#include <QApplication>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTextBrowser>
#include <QSoundEffect>
#include "sql/connection.h"
#include "widget/mainwindow.h"
#include "widget/trainingwidget.h"
#include "widget/tickerboard.h"
#include "widget/helpbrowser.h"

static void require(bool ok, const char *message) {
    if (!ok) qFatal("%s", message);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc > 1) {
        QFile expected(QString::fromLocal8Bit(argv[1]));
        QFile embedded(":/tipp10v2.template");
        require(expected.open(QIODevice::ReadOnly) && embedded.open(QIODevice::ReadOnly),
                "Cannot open expected or embedded database");
        require(QCryptographicHash::hash(expected.readAll(), QCryptographicHash::Sha256)
                == QCryptographicHash::hash(embedded.readAll(), QCryptographicHash::Sha256),
                "Embedded database does not match the selected input");
    }
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    app.setOrganizationName("tipp10-smoke");
    app.setApplicationName("tipp10-smoke");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings settings;
    settings.setValue("database/pathpro", temporary.filePath("test.db"));
    settings.setValue("general/language_gui", "de");
    settings.setValue("general/language_layout", APP_STD_LANGUAGE_LAYOUT);
    settings.setValue("general/language_lesson", "de_de_qwertz");
    // Turn unexpected modal database errors into a failed test, not a hang.
    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, [] { qFatal("Smoke test timed out"); });
    watchdog.start(20000);
    require(createConnection(), "Database initialization failed");
    QSqlQuery query;
    require(query.exec("SELECT COUNT(*) FROM lesson_list"), "Lesson query failed");
    require(query.next() && query.value(0).toInt() > 0, "No lessons installed");
    MainWindow window;
    window.show();
    window.toggleStartToTraining(1, 0, "Smoke test");
    auto training = window.findChild<TrainingWidget *>();
    require(training, "Training widget missing");
    auto ticker = training->findChild<TickerBoard *>();
    require(ticker, "Ticker missing");
    QTest::keyClick(ticker, Qt::Key_Space);
    QTest::keyClicks(ticker, "asdf jkl");
    QTest::qWait(500);
    const auto sounds = training->findChildren<QSoundEffect *>();
    require(sounds.size() == 2, "Sound objects missing");
    for (auto sound : sounds)
        require(sound->status() != QSoundEffect::Error, "Embedded sound failed to load");
    require(QMetaObject::invokeMethod(training, "exitTraining"), "Cannot finish lesson");
    require(query.exec("SELECT COUNT(*) FROM user_lesson_list"), "Results query failed");
    require(query.next() && query.value(0).toInt() == 1, "Lesson result not saved");
    HelpBrowser help("", &window);
    auto browser = help.findChild<QTextBrowser *>();
    require(browser && !browser->toPlainText().isEmpty(), "Embedded help missing");
    query.finish();
    require(createConnection(), "Database reopen failed");
    require(query.exec("SELECT COUNT(*) FROM user_lesson_list"), "Reopened results query failed");
    require(query.next() && query.value(0).toInt() == 1, "Saved result lost after reopening");
    settings.sync();
    require(settings.status() == QSettings::NoError, "Settings were not saved");
    qInfo("PASS: database initialization, lesson startup/input/results, sounds, help, settings");
    return 0;
}
