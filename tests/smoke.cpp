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
    // Simulate a macOS bundle in an isolated, writable app folder.
    const QString executable = temporary.filePath("tipp10.app/Contents/MacOS/tipp10");
    require(QDir().mkpath(QFileInfo(executable).absolutePath()), "App folder creation failed");
    const QString appDirectory = QFileInfo(executable).absolutePath();
    require(applicationDatabasePath(appDirectory) == temporary.filePath("tipp10v2.db"),
            "Database is not beside the app bundle");
    const QString previousPath = temporary.filePath("previous.db");
    require(QFile::copy(":/tipp10v2.template", previousPath), "Previous database setup failed");
    require(QFile::setPermissions(previousPath, QFile::permissions(previousPath) | QFile::WriteUser),
            "Previous database permissions failed");
    {
        QSqlDatabase previous = QSqlDatabase::addDatabase("QSQLITE", "migration-source");
        previous.setDatabaseName(previousPath);
        require(previous.open(), "Previous database open failed");
        QSqlQuery marker(previous);
        require(marker.exec("CREATE TABLE migration_marker (value TEXT)"), "Migration marker failed");
        previous.close();
    }
    QSqlDatabase::removeDatabase("migration-source");
    settings.setValue("database/pathpro", previousPath);
    settings.setValue("general/language_gui", "de");
    settings.setValue("general/language_layout", APP_STD_LANGUAGE_LAYOUT);
    settings.setValue("general/language_lesson", "de_de_qwertz");
    // Turn unexpected modal database errors into a failed test, not a hang.
    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, [] { qFatal("Smoke test timed out"); });
    watchdog.start(20000);
#if APP_MAC
    const QString translocated = temporary.filePath("T/AppTranslocation/test/d/tipp10.app/Contents/MacOS");
    const QString originalSetting = settings.value("database/pathpro").toString();
    bool installationMessageShown = false;
    QTimer::singleShot(0, [&] {
        auto message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(message && message->text().contains("Finder"), "Installation guidance missing");
        installationMessageShown = true;
        message->accept();
    });
    require(!createConnection(translocated), "Translocated app must stop before database initialization");
    require(installationMessageShown, "Translocation message missing");
    require(!QFile::exists(applicationDatabasePath(translocated)), "Translocation created a database");
    require(settings.value("database/pathpro").toString() == originalSetting,
            "Translocation changed the saved database path");
#endif
    require(createConnection(appDirectory), "Database initialization failed");
    require(QSqlDatabase::database().databaseName() == applicationDatabasePath(appDirectory),
            "Active database path is incorrect");
    require(QFile::exists(previousPath), "Migration removed the original database");
    QSqlQuery query;
    require(query.exec("SELECT COUNT(*) FROM migration_marker"), "Previous database was not migrated");
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
    settings.setValue("database/pathpro", previousPath);
    require(createConnection(appDirectory), "Database reopen failed");
    require(query.exec("SELECT COUNT(*) FROM user_lesson_list"), "Reopened results query failed");
    require(query.next() && query.value(0).toInt() == 1, "Saved result lost after reopening");
    settings.sync();
    require(settings.status() == QSettings::NoError, "Settings were not saved");
    qInfo("PASS: database initialization, lesson startup/input/results, sounds, help, settings");
    return 0;
}
