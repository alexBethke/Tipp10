/*
Copyright (c) 2006-2009, Tom Thielicke IT Solutions

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.
*/

/****************************************************************
**
** File name: connection.h
**
****************************************************************/

#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTextStream>
#include <QMessageBox>
#include <QSqlQuery>
#include <QList>

#include "def/defines.h"
#include "def/errordefines.h"
#include "widget/errormessage.h"
#include "sql/startsql.h"

// The writable database lives beside the app bundle (or executable).
static QString applicationDatabasePath(const QString &appDirectory = QCoreApplication::applicationDirPath()) {
    QDir directory(appDirectory);
#if APP_MAC
    if (directory.dirName() == "MacOS") {
        directory.cdUp();
        if (directory.dirName() == "Contents") {
            directory.cdUp();
            if (directory.dirName().endsWith(".app"))
                directory.cdUp();
        }
    }
#endif
    return directory.filePath(APP_USER_DB);
}

// Database connection to SQLite
static bool createConnection(const QString &appDirectory = QCoreApplication::applicationDirPath()) {
	// Path do the database
	QString dbPath;
	// Filename of the template database
	QString dbNameTemplate = APP_DB;
	// Filename of the user database
	QString dbNameUser = APP_USER_DB;
	// Connection to SQLite
    QSqlDatabase db;
	QList<QString> lessonId;
	QList<QString> lessonLesson;
	QList<QString> lessonTime;
	QList<QString> lessonToken;
	QList<QString> lessonStrokes;
	QList<QString> lessonErrors;
	QList<QString> lessonStamp;
	QList<QString> charUnicode;
	QList<QString> charTarget;
	QList<QString> charMistake;
	QList<QString> charOccur;
	int lessonCounter = 0;
	int charCounter = 0;
	int i;

    // Read user db path from settings, if not exist as argument
	#if APP_PORTABLE
	QSettings settings(QCoreApplication::applicationDirPath() +
    	"/portable/settings.ini", QSettings::IniFormat);
	#else
	QSettings settings;
	#endif
	settings.beginGroup("database");
	QString previousPath = settings.value("pathpro", "").toString();
	settings.endGroup();
	dbPath = applicationDatabasePath(appDirectory);

	// Search for an old database if first programmstart or if user want to
	bool searchOldDb = false;
	if (searchOldDb) {
		searchOldDb = false;
		if (QFile::exists(QCoreApplication::applicationDirPath() + "/tipp10.db")) {
			if (QMessageBox::question(0, APP_NAME, QObject::tr(""
				"Es wurde eine alte TIPP10-Datenbank der Version 1 im\n"
				"Programmverzeichnis gefunden.\n\n"
				"Wollen Sie die persoenlichen Daten aus der alten Datenbank\n"
				"in die neue Datenbank dieser Version uebernehmen?\n"),
				QObject::tr("&Ja"), QObject::tr("&Nein"), 0, 1) == 0) {

				lessonId.clear();
				lessonLesson.clear();
				lessonTime.clear();
				lessonToken.clear();
				lessonStrokes.clear();
				lessonErrors.clear();
				lessonStamp.clear();
				charUnicode.clear();
				charTarget.clear();
				charMistake.clear();
				charOccur.clear();

				//Transfer old data to new database now
				// Set database
				db.setDatabaseName(QCoreApplication::applicationDirPath() + "/tipp10.db");
				// Open the database
				if (!db.open()) {
					// Error message
					ErrorMessage *errorMessage = new ErrorMessage();
					errorMessage->showMessage(ERR_SQL_CONNECTION, TYPE_WARNING,
						CANCEL_NO);
				} else {
					QSqlQuery query;
					// Lessons done by the user
					if (query.exec("SELECT user_lesson_id, user_lesson_lesson, "
						"user_lesson_timelen, user_lesson_tokenlen, "
						"user_lesson_strokesnum, user_lesson_errornum, "
						"user_lesson_timestamp "
						"FROM user_lesson_list;")) {
						// Read all datasets to list items
						while (query.next()) {
							lessonId.append(query.value(0).toString());
							lessonLesson.append(query.value(1).toString());
							lessonTime.append(query.value(2).toString());
							lessonToken.append(query.value(3).toString());
							lessonStrokes.append(query.value(4).toString());
							lessonErrors.append(query.value(5).toString());
							lessonStamp.append(query.value(6).toString());
							lessonCounter++;
						}
						// Char list of the user
						if (query.exec("SELECT user_char_unicode, user_char_target_errornum, "
							"user_char_mistake_errornum, user_char_occur_num "
							"FROM user_chars;")) {
							// Read all datasets to list items
							while (query.next()) {
								charUnicode.append(query.value(0).toString());
								charTarget.append(query.value(1).toString());
								charMistake.append(query.value(2).toString());
								charOccur.append(query.value(3).toString());
								charCounter++;
							}
							searchOldDb = true;
						}
					}
				}
			}
		}
		settings.beginGroup("general");
		settings.setValue("check_db_update", false);
		settings.endGroup();
	}

    // Preserve existing data when switching from the previous storage location.
    // An existing database beside the app always takes precedence.
    if (!QFile::exists(dbPath)) {
        QStringList candidates;
        candidates << previousPath;
        candidates << QCoreApplication::applicationDirPath() + "/portable/" + dbNameUser;
        candidates << QDir::homePath() + "/tipp10/" + dbNameUser;
#if APP_MAC
        candidates << QDir::homePath() + "/Library/Application Support/tipp10/" + dbNameUser;
#endif
#if APP_WIN
        QSettings homeAppPath("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", QSettings::NativeFormat);
        candidates << homeAppPath.value("AppData").toString() + "/tipp10/" + dbNameUser;
#endif
        QString source = QString(":/") + dbNameTemplate;
        for (const QString &candidate : candidates) {
            if (!candidate.isEmpty() && QFile::exists(candidate)) {
                source = candidate;
                break;
            }
        }
        if (!QFile::copy(source, dbPath)) {
            ErrorMessage *errorMessage = new ErrorMessage();
            errorMessage->showMessage(ERR_SQL_DB_APP_COPY, TYPE_CRITICAL,
                CANCEL_NO, QObject::tr("Betroffener Kopierpfad:\n") + dbPath);
            return false;
        }
        if (!QFile::setPermissions(dbPath, QFile::permissions(dbPath) | QFile::WriteUser)) {
            return false;
        }
    }

    if (QSqlDatabase::contains()) {
        db = QSqlDatabase::database();
        db.close();
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE");
    }

	// Set database
    db.setDatabaseName(dbPath);
    // Open the database
    if (!db.open()) {
		// Error message
		ErrorMessage *errorMessage = new ErrorMessage();
		errorMessage->showMessage(ERR_SQL_CONNECTION, TYPE_CRITICAL,
			CANCEL_PROGRAM, QObject::tr("Betroffener Pfad:\n") + dbPath);
        return false;
    }


    // If necessary, write old data to new database
    if (searchOldDb) {
		QSqlQuery queryNew;
    	if (lessonCounter != 0) {
			for (i = 0; i < lessonCounter; i++) {
				if (queryNew.exec("INSERT INTO user_lesson_list VALUES(NULL," +
					lessonLesson.first() + "," + lessonTime.takeFirst() + "," +
					lessonToken.takeFirst() + "," + lessonStrokes.takeFirst() + "," +
					lessonErrors.takeFirst() + ", '" +
					lessonStamp.takeFirst() + "', "
					"0, '" + QObject::tr("Uebungslektion") + " " + lessonLesson.first() + "');")) {
				}
				lessonLesson.removeFirst();
			}
		}
    	if (charCounter != 0) {
			for (i = 0; i < charCounter; i++) {
				if (queryNew.exec("INSERT INTO user_chars VALUES(" +
					charUnicode.takeFirst() + "," + charTarget.takeFirst() + "," +
					charMistake.takeFirst() + "," + charOccur.takeFirst() + ");")) {
				}
			}
		}
		if (lessonCounter != 0 || charCounter != 0) {
			QMessageBox::information(0, APP_NAME,
				QObject::tr("Ihre Daten wurden erfolgreich in die\n"
				"neue Datenbank uebertragen!\n"));
		}
		searchOldDb = false;
	}

    settings.beginGroup("database");
	settings.setValue("pathpro", dbPath);
	settings.endGroup();


	QSqlQuery queryUpdate;

	if (!queryUpdate.exec("SELECT * FROM db_version ORDER BY version DESC;")) {
		// Error message
		ErrorMessage *errorMessage = new ErrorMessage();
		errorMessage->showMessage(ERR_DB_VERSION_READABLE,
			TYPE_CRITICAL, CANCEL_UPDATE);
	} else {
		if (!queryUpdate.first()) {
			// Error message
			ErrorMessage *errorMessage = new ErrorMessage();
			errorMessage->showMessage(ERR_DB_VERSION_READABLE,
				TYPE_CRITICAL, CANCEL_UPDATE);
		} else {
			// Server DB version is 0
			// -> software is too old to update
            if (queryUpdate.value(0).toInt() < (int) COMPILED_UPDATE_VERSION) {

				// Update the database
				QStringList updateFileName = QString(UPDATE_URL_SQL).split("/");

				QFile sqlFile(":/update/" + updateFileName[updateFileName.count() - 1]);
				if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
					// Error message
					ErrorMessage *errorMessage = new ErrorMessage();
					errorMessage->showMessage(ERR_TEMP_FILE_CREATION, TYPE_CRITICAL,
						CANCEL_UPDATE);
				}

				// Go to the beginning of the version file
				sqlFile.seek(0);

				QTextStream in(&sqlFile);

				QString line;

				// Execute all sql command of the downloaded file
				while (!in.atEnd()) {
					line = in.readLine();
					line = line.trimmed();
					// Exclude comments and empty lines
					if (line != "" && !line.startsWith("//", Qt::CaseSensitive)) {
						// Without error handling, because DROP-Statements are allowed to
						// be invalid (there exist also a IF EXISTS statement in the SQLite
						// library which suppresses an error, but it didn't work when I try it)
						if (!queryUpdate.exec(line) && !line.startsWith("drop", Qt::CaseInsensitive)) {
							// Error message + failed sql string
							ErrorMessage *errorMessage = new ErrorMessage();
							errorMessage->showMessage(ERR_UPDATE_SQL_EXECUTION, TYPE_CRITICAL,
								CANCEL_UPDATE, line);
							break;
						}
					}
                }
                StartSql *lessonSql = new StartSql();
				if (!lessonSql->analyzeLessons("lesson")) {
					ErrorMessage *errorMessage = new ErrorMessage();
					errorMessage->showMessage(ERR_ANALYZE_TABLE_FILL, TYPE_CRITICAL,
						CANCEL_UPDATE, line);
                }
                if (!lessonSql->analyzeLessons("open")) {
                    ErrorMessage *errorMessage = new ErrorMessage();
                    errorMessage->showMessage(ERR_ANALYZE_TABLE_FILL, TYPE_CRITICAL,
                        CANCEL_UPDATE, line);
                }
                if (!lessonSql->analyzeLessons("own")) {
                    ErrorMessage *errorMessage = new ErrorMessage();
                    errorMessage->showMessage(ERR_ANALYZE_TABLE_FILL, TYPE_CRITICAL,
                        CANCEL_UPDATE, line);
                }
			}
		}
	}

    return true;


	// If to connect directly to an online database
    // Example: database connection to MySql
	/*QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("http://www.domain.com");
    db.setDatabaseName("dbname");
    db.setUserName("user");
    db.setPassword("pass");*/
}

#endif // CONNECTION_H
