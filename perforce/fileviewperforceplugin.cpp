/***************************************************************************
 *   Copyright (C) 2012 by Martin Andersen <martin9000andersen gmail.com>  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "fileviewperforceplugin.h"

#include <kaction.h>
#include <kdemacros.h>
#include <kdialog.h>
#include <kfileitem.h>
#include <kicon.h>
#include <klocale.h>
#include <krun.h>
#include <kshell.h>
#include <kvbox.h>
#include <KUrl>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <kdebug.h>
#include <iostream>
#include <QDirIterator>

#include <KPluginFactory>
#include <KPluginLoader>
K_PLUGIN_FACTORY ( FileViewPerforcePluginFactory, registerPlugin<FileViewPerforcePlugin>(); )
K_EXPORT_PLUGIN ( FileViewPerforcePluginFactory ( "fileviewperforceplugin" ) )

FileViewPerforcePlugin::FileViewPerforcePlugin ( QObject* parent, const QList<QVariant>& args ) :
    KVersionControlPlugin2 ( parent ),
    m_pendingOperation ( false ),
    m_versionInfoHash(),
    m_versionInfoHashDir(),
    m_updateAction ( 0 ),
    m_addAction ( 0 ),
    m_removeAction ( 0 ),
    m_command(),
    m_arguments(),
    m_errorMsg(),
    m_operationCompletedMsg(),
    m_contextDir(),
    m_contextItems(),
    m_process()
{
    Q_UNUSED ( args );

    m_updateAction = new KAction ( this );
    m_updateAction->setIcon ( KIcon ( "view-refresh" ) );
    m_updateAction->setText ( i18nc ( "@item:inmenu", "Perforce Update" ) );
    connect ( m_updateAction, SIGNAL ( triggered() ),
              this, SLOT ( updateFiles() ) );

    m_addAction = new KAction ( this );
    m_addAction->setIcon ( KIcon ( "list-add" ) );
    m_addAction->setText ( i18nc ( "@item:inmenu", "Perforce Add" ) );
    connect ( m_addAction, SIGNAL ( triggered() ),
              this, SLOT ( addFiles() ) );

    m_removeAction = new KAction ( this );
    m_removeAction->setIcon ( KIcon ( "list-remove" ) );
    m_removeAction->setText ( i18nc ( "@item:inmenu", "Perforce Delete" ) );
    connect ( m_removeAction, SIGNAL ( triggered() ),
              this, SLOT ( removeFiles() ) );

    m_openForEditAction = new KAction ( this );
    m_openForEditAction->setIcon ( KIcon ( "document-edit" ) );
    m_openForEditAction->setText ( i18nc ( "@item:inmenu", "Perforce Edit" ) );
    connect ( m_openForEditAction, SIGNAL ( triggered() ),
              this, SLOT ( openFilesForEdit() ) );

    m_revertAction = new KAction ( this );
    m_revertAction->setIcon ( KIcon ( "edit-undo" ) );
    m_revertAction->setText ( i18nc ( "@item:inmenu", "Perforce Revert" ) );
    connect ( m_revertAction, SIGNAL ( triggered() ),
              this, SLOT ( revertFiles() ) );

    m_revertUnchangedAction = new KAction ( this );
    m_revertUnchangedAction->setIcon ( KIcon ( "edit-undo" ) );
    m_revertUnchangedAction->setText ( i18nc ( "@item:inmenu", "Perforce Revert Unchanged Files" ) );
    connect ( m_revertUnchangedAction, SIGNAL ( triggered() ),
              this, SLOT ( revertUnchangedFiles() ) );

    connect ( &m_process, SIGNAL ( finished ( int, QProcess::ExitStatus ) ),
              this, SLOT ( slotOperationCompleted ( int, QProcess::ExitStatus ) ) );
    connect ( &m_process, SIGNAL ( error ( QProcess::ProcessError ) ),
              this, SLOT ( slotOperationError() ) );

    QProcessEnvironment currentEviron ( QProcessEnvironment::systemEnvironment() );
    // We will default search for p4config.txt - However if something else is used, search for that
    QString tmp ( currentEviron.value ( "P4CONFIG" ) );
    if ( !tmp.isEmpty() ) {
        m_perforceConfigName = tmp;
    } else {
        m_perforceConfigName = "p4config.txt";
    }
}

FileViewPerforcePlugin::~FileViewPerforcePlugin()
{
}

QString FileViewPerforcePlugin::fileName() const
{
    return m_perforceConfigName;
}

bool FileViewPerforcePlugin::beginRetrieval ( const QString& directory )
{
    Q_ASSERT ( directory.endsWith ( QLatin1Char ( '/' ) ) );

    m_versionInfoHash.clear();
    m_versionInfoHashDir.clear();

    QProcess process;    
    process.start ( "p4 -d\""+directory+"\" fstat -T\"clientFile,headRev,haveRev,action\" ..." );
//     QStringList arguments;
//     arguments << QLatin1String("-d") << directory
//               << QLatin1String("fstat")
//               << QLatin1String("-T\'clientFile,action\'")
//               << QLatin1String("...");
//     process.start( QLatin1String("p4"), arguments );    
    while ( process.waitForReadyRead() ) {
        char buffer[1024];
        while ( true )  {
            
            // strings contains uptil 4 lines in the format:
            //    "... clientFile " follower by a file path
            //    "... headRev " followed by a revision number
            //    "... haveRev "followed by a revision number
            //    "... action " followed by an action
            // The first line in mandatory, the remaning lines can be missing,
            // the order however is constant
            
            QStringList strings;
            bool failed = false;
            while ( true ) {
                if ( process.readLine ( buffer, sizeof ( buffer ) )<=0 )
                {
                    failed = true;
                    break;
                }
                strings.append( buffer );
                if ( strings.last().length() < 3 ) {
                    strings.removeLast();
                    break;
                }
            }
            if ( strings.isEmpty() || failed )
            {
                break;
            }
            
            QString headRev;
            QString haveRev;
            QString action;
            
            if ( strings.last().startsWith("... action") ) {
                static const int pos = sizeof ( "... action" );
                int length = strings.last().length() - pos -1;
                action = strings.takeLast().mid ( pos, length );
            }
            if ( strings.last().startsWith("... haveRev") ) {
                static const int pos = sizeof ( "... haveRev" );
                int length = strings.last().length() - pos -1;
                haveRev = strings.takeLast().mid ( pos, length );
            }
            if ( strings.last().startsWith("... headRev") ) {
                static const int pos = sizeof ( "... headRev" );
                int length = strings.last().length() - pos -1;
                headRev = strings.takeLast().mid ( pos, length );
            }
            
            bool needsUpdate = (haveRev != headRev);

            
            static const int clientFileStartPos = sizeof ( "... clientFile" );
            const int lengthFileName = strings.first().length() - clientFileStartPos -1;
            QString filePath = strings.first().mid ( clientFileStartPos, lengthFileName );
            
            if ( action.isEmpty() ) {
                if ( !needsUpdate ) {
                    updataFileVersion ( filePath, NormalVersion );
                } else {
                    updataFileVersion ( filePath, UpdateRequiredVersion );
                }
            } else if ( needsUpdate ) {
                updataFileVersion ( filePath, ConflictingVersion );
            } else  if ( action.isEmpty() ) {
                updataFileVersion ( filePath, NormalVersion );
            } else if ( action=="edit" ) {
                updataFileVersion ( filePath, LocallyModifiedVersion );
            } else if ( action=="add" || action=="move/add" || action=="import" || action=="branch" ) {
                updataFileVersion ( filePath, AddedVersion );
            } else if ( action=="delete" || action=="move/delete" || action=="purge" ) {
                updataFileVersion ( filePath, RemovedVersion );
            } else if ( action=="integrate" || action=="archive" ) {
                updataFileVersion ( filePath, NormalVersion );
            } else {
                kWarning() << "Unknown perforce file version: "<<action;
                updataFileVersion ( filePath, NormalVersion );
            }  
            // FIXME: check version of a action = {import, branch, integrate, archive}
        }
    }

    if ( ( process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit ) ) {
        emit errorMessage ( QLatin1String("p4 error: ") + process.errorString() );
        return false;
    }
    return true;
}

void FileViewPerforcePlugin::updataFileVersion ( const QString& filePath, ItemVersion version )
{
    m_versionInfoHash.insert ( filePath, version );

    // Update version of parent directories
    ItemVersion stateOfDir = version;
    if ( stateOfDir == AddedVersion || stateOfDir == RemovedVersion ) {
        stateOfDir = LocallyModifiedVersion;
    } else if ( stateOfDir == MissingVersion ) {
        stateOfDir=UpdateRequiredVersion;
    }

    QDir dir ( filePath ); // After first call to cdUp() dir points to the directory of the file
    while ( dir.cdUp() ) {
        if ( !m_versionInfoHashDir.contains ( dir.path() ) ) {
            m_versionInfoHashDir.insert ( dir.path(), stateOfDir );
            continue;
        }

        if ( stateOfDir==NormalVersion ) { // lowest priority
            return;
        }

        const ItemVersion currentRegistratedState = m_versionInfoHashDir.value ( dir.path() );

        if ( currentRegistratedState == stateOfDir || currentRegistratedState==ConflictingVersion ) {
            return;
        }

        if ( stateOfDir==ConflictingVersion ) {
            m_versionInfoHashDir.insert ( dir.path(), ConflictingVersion );
        } else if ( currentRegistratedState==UpdateRequiredVersion ) {
            return;
        } else if ( stateOfDir==UpdateRequiredVersion ) {
            m_versionInfoHashDir.insert ( dir.path(), UpdateRequiredVersion );
        } else if ( currentRegistratedState==LocallyModifiedVersion ) {
            return;
        } else { // stateOfDir==LocallyModifiedVersion
            m_versionInfoHashDir.insert ( dir.path(), LocallyModifiedVersion );
        }
    }
}

void FileViewPerforcePlugin::endRetrieval()
{
}

KVersionControlPlugin2::ItemVersion FileViewPerforcePlugin::itemVersion ( const KFileItem& item ) const
{
    const QString itemUrl = item.localPath();
    if ( m_versionInfoHash.contains ( itemUrl ) ) {
        return m_versionInfoHash.value ( itemUrl );
    }

    if ( m_versionInfoHashDir.contains ( itemUrl ) ) {
        return m_versionInfoHashDir.value ( itemUrl );
    }

    return UnversionedVersion;
}

QList<QAction*> FileViewPerforcePlugin::actions ( const KFileItemList& items ) const
{
    if ( items.count() == 1 && items.first().isDir() ) {
        const QString directory = items.first().url().path();
        return directoryActions ( directory );
    }

    foreach ( const KFileItem& item, items ) {
        m_contextItems.append ( item );
    }
    m_contextDir.clear();

    const bool noPendingOperation = !m_pendingOperation;
    if ( noPendingOperation ) {
        // iterate all items and check the version to know which
        // actions can be enabled
        const int itemsCount = items.count();
        int versionedCount = 0;
        int editingCount = 0;
        foreach ( const KFileItem& item, items ) {
            const ItemVersion version = itemVersion ( item );
            if ( version != UnversionedVersion ) {
                ++versionedCount;
            }

            switch ( version ) {
            case LocallyModifiedVersion:
            case ConflictingVersion:
            case AddedVersion:
            case RemovedVersion:
                ++editingCount;
                break;
            default:
                break;
            }
        }
        m_revertAction->setEnabled ( editingCount > 0 );
        m_revertUnchangedAction->setEnabled ( editingCount > 0 );
        m_addAction->setEnabled ( versionedCount == 0 );
        m_removeAction->setEnabled ( versionedCount == itemsCount && editingCount == 0 );
        m_openForEditAction->setEnabled ( editingCount < itemsCount );
    } else {
        m_revertAction->setEnabled ( false );
        m_revertUnchangedAction->setEnabled ( false );
        m_addAction->setEnabled ( false );
        m_removeAction->setEnabled ( false );
        m_openForEditAction->setEnabled ( false );
    }
    m_updateAction->setEnabled ( noPendingOperation );

    QList<QAction*> actions;
    actions.append ( m_openForEditAction );
    actions.append ( m_updateAction );
    actions.append ( m_addAction );
    actions.append ( m_removeAction );
    actions.append ( m_revertAction );
    actions.append ( m_revertUnchangedAction );
    return actions;
}


void FileViewPerforcePlugin::updateFiles()
{
    execPerforceCommand ( QLatin1String ( "sync" ), QStringList(),
                          i18nc ( "@info:status", "Syncing Perforce repository..." ),
                          i18nc ( "@info:status", "Syncing of Perforce repository failed." ),
                          i18nc ( "@info:status", "Syncing Perforce repository compleet." ) );
}

void FileViewPerforcePlugin::addFiles()
{
    execPerforceCommand ( QLatin1String ( "add" ), QStringList(),
                          i18nc ( "@info:status", "Adding files to Perforce repository..." ),
                          i18nc ( "@info:status", "Adding of files to Perforce repository failed." ),
                          i18nc ( "@info:status", "Added files to Perforce repository." ) );
}

void FileViewPerforcePlugin::removeFiles()
{
    execPerforceCommand ( QLatin1String ( "delete" ), QStringList(),
                          i18nc ( "@info:status", "Deleting files from Perforce repository..." ),
                          i18nc ( "@info:status", "Deleting of files from Perforce repository failed." ),
                          i18nc ( "@info:status", "Deleted files from Perforce repository." ) );
}

void FileViewPerforcePlugin::openFilesForEdit()
{
    execPerforceCommand ( QLatin1String ( "edit" ), QStringList(),
                          i18nc ( "@info:status", "Opening files for edit..." ),
                          i18nc ( "@info:status", "Opening files for edit failed." ),
                          i18nc ( "@info:status", "Opened files for edit." ) );
}

void FileViewPerforcePlugin::revertFiles()
{
    execPerforceCommand ( QLatin1String ( "revert" ), QStringList(),
                          i18nc ( "@info:status", "Reverting files from Perforce repository..." ),
                          i18nc ( "@info:status", "Reverting of files from Perforce repository failed." ),
                          i18nc ( "@info:status", "Reverted files from Perforce repository." ) );
}

void FileViewPerforcePlugin::revertUnchangedFiles()
{
    QStringList arguments;
    arguments << "-a";
    execPerforceCommand ( QLatin1String ( "revert" ), arguments,
                          i18nc ( "@info:status", "Reverting unchanged files from Perforce repository..." ),
                          i18nc ( "@info:status", "Reverting unchanged of files from Perforce repository failed." ),
                          i18nc ( "@info:status", "Reverted unchanged files from Perforce repository." ) );
}

void FileViewPerforcePlugin::slotOperationCompleted ( int exitCode, QProcess::ExitStatus exitStatus )
{
    m_pendingOperation = false;

    if ( ( exitStatus != QProcess::NormalExit ) || ( exitCode != 0 ) ) {
        emit errorMessage ( m_errorMsg );
    } else if ( m_contextItems.isEmpty() ) {
        emit operationCompletedMessage ( m_operationCompletedMsg );
        emit itemVersionsChanged();
    } else {
        startPerforceCommandProcess();
    }
}

void FileViewPerforcePlugin::slotOperationError()
{
    // don't do any operation on other items anymore
    m_contextItems.clear();
    m_pendingOperation = false;

    emit errorMessage ( m_errorMsg );
}

void FileViewPerforcePlugin::execPerforceCommand ( const QString& perforceCommand,
        const QStringList& arguments,
        const QString& infoMsg,
        const QString& errorMsg,
        const QString& operationCompletedMsg )
{
    emit infoMessage ( infoMsg );

    m_command = perforceCommand;
    m_arguments = arguments;
    m_errorMsg = errorMsg;
    m_operationCompletedMsg = operationCompletedMsg;

    startPerforceCommandProcess();
}

void FileViewPerforcePlugin::startPerforceCommandProcess()
{
    Q_ASSERT ( m_process.state() == QProcess::NotRunning );
    m_pendingOperation = true;

    const QString program ( QLatin1String ( "p4" ) );
    QStringList arguments;
    if ( !m_contextDir.isEmpty() ) {
        arguments << QLatin1String ( "-d" ) << m_contextDir
                  << m_command << m_arguments
                  << m_contextDir.append ( "..." ); // append '...' to make the operation recurisive
        m_contextDir.clear();
    } else {
        const KFileItem item = m_contextItems.takeLast();
        arguments << QLatin1String ( "-d" ) << item.localPath()
                  << m_command << m_arguments;
        if ( item.isDir() ) {
            arguments << item.localPath().append ( "..." ); // add '...' to make the operation recurisive
        } else {
            arguments << item.localPath();
        }
        // the remaining items of m_contextItems will be executed
        // after the process has finished (see slotOperationCompleted())
    }

    m_process.start ( program, arguments );
}

QList<QAction*> FileViewPerforcePlugin::directoryActions ( const QString& directory ) const
{
    m_contextDir = directory;
    m_contextItems.clear();

    // No items if on file in the directory is not under version control
    if ( !m_versionInfoHashDir.contains ( directory ) ) {
        QList<QAction*> actions;
        return actions;
    }

    // Only enable the Perforce actions if no Perforce commands are
    // executed currently (see slotOperationCompleted() and
    // startPerforceCommandProcess()).
    const bool enabled = !m_pendingOperation;

    m_updateAction->setEnabled ( enabled );
    m_openForEditAction->setEnabled ( enabled );
    m_revertAction->setEnabled ( enabled );
    m_revertUnchangedAction->setEnabled ( enabled );

    QList<QAction*> actions;
    actions.append ( m_openForEditAction );
    actions.append ( m_updateAction );
    actions.append ( m_revertAction );
    actions.append ( m_revertUnchangedAction );
    return actions;
}

#include "fileviewperforceplugin.moc"
