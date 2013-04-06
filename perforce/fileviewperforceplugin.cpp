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
#include <kfileitem.h>
#include <kicon.h>
#include <klocale.h>
#include <KUrl>
#include <krun.h>
#include <QProcess>
#include <QString>
#include <kdebug.h>
#include <QDirIterator>
#include <QDir>
#include <QStringBuilder>
#include <kshell.h>

#include <KPluginFactory>
#include <KPluginLoader>
K_PLUGIN_FACTORY ( FileViewPerforcePluginFactory, registerPlugin<FileViewPerforcePlugin>(); )
K_EXPORT_PLUGIN ( FileViewPerforcePluginFactory ( "fileviewperforceplugin" ) )

const QString DIFF_FILE_NAME = "/tmp/DIFF_FILE_NAME.diff";

FileViewPerforcePlugin::FileViewPerforcePlugin ( QObject* parent, const QList<QVariant>& args ) :
    KVersionControlPlugin2 ( parent ),
    m_pendingOperation ( false )
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

    m_diffActionHaveRev = new KAction ( this );
    m_diffActionHaveRev->setIcon ( KIcon ( "view-split-left-right" ) );
    m_diffActionHaveRev->setText ( i18nc ( "@item:inmenu", "Perforce Diff Against Have" ) );
    connect ( m_diffActionHaveRev, SIGNAL ( triggered() ),
              this, SLOT ( diffAgainstHaveRev() ) );

    m_resolveAction = new KAction ( this );
    // m_resolveAction->setIcon ( KIcon ( "view-split-left-right" ) ); //TODO: find icon
    m_resolveAction->setText ( i18nc ( "@item:inmenu", "Perforce Resolve Conflict" ) );
    connect ( m_resolveAction, SIGNAL ( triggered() ),
              this, SLOT ( resolveConflict() ) );

    m_timelapsviewAction = new KAction ( this );
    m_timelapsviewAction->setIcon ( KIcon ( "view-history" ) );
    m_timelapsviewAction->setText ( i18nc ( "@item:inmenu", "Perforce Timelapsview" ) );
    connect ( m_timelapsviewAction, SIGNAL ( triggered() ),
              this, SLOT ( timelapsview() ) );

    m_showInP4VAction = new KAction ( this );
    m_showInP4VAction->setIcon ( KIcon ( "p4v" ) );
    m_showInP4VAction->setText ( i18nc ( "@item:inmenu", "Perforce Show File in P4V" ) );
    connect ( m_showInP4VAction, SIGNAL ( triggered() ),
              this, SLOT ( showInP4V() ) );

    m_submitAction = new KAction ( this );
    m_submitAction->setIcon ( KIcon ( "svn-commit" ) );
    m_submitAction->setText ( i18nc ( "@item:inmenu", "Perforce Submit" ) );
    connect ( m_submitAction, SIGNAL ( triggered() ),
              this, SLOT ( submit() ) );

    m_diffActionHeadRev = new KAction ( this );
    m_diffActionHeadRev->setIcon ( KIcon ( "view-split-left-right" ) );
    m_diffActionHeadRev->setText ( i18nc ( "@item:inmenu", "Perforce Diff Against Head" ) );
    connect ( m_diffActionHeadRev, SIGNAL ( triggered() ),
              this, SLOT ( diffAgainstHeadRev() ) );

    connect ( &m_process, SIGNAL ( finished ( int, QProcess::ExitStatus ) ),
              this, SLOT ( slotOperationCompleted ( int, QProcess::ExitStatus ) ) );
    connect ( &m_process, SIGNAL ( error ( QProcess::ProcessError ) ),
              this, SLOT ( slotOperationError() ) );

    connect ( &m_diffProcess, SIGNAL ( finished ( int, QProcess::ExitStatus ) ),
              this, SLOT ( slotDiffOperationCompleted ( int, QProcess::ExitStatus ) ) );
    connect ( &m_diffProcess, SIGNAL ( error ( QProcess::ProcessError ) ),
              this, SLOT ( slotOperationError() ) );

    QProcessEnvironment processEnvironment ( QProcessEnvironment::systemEnvironment() );
    // We will default search for p4config.txt - However if something else is used, search for that
    QString tmp ( processEnvironment.value ( "P4CONFIG" ) );
    if ( !tmp.isEmpty() ) {
        m_perforceConfigName = tmp;
    } else {
        m_perforceConfigName = "p4config.txt";
    }

    // The diff commands in this file depends on the default perforce diff tool. This can be changed
    // with if P4DIFF, there fore this environment var is removed.
    // Note however that it can also be changed in the config file in the directory of the file under
    // perforce control and there are no easy way to overwrite this setting
    processEnvironment.remove( "P4DIFF" );
    m_diffProcess.setProcessEnvironment( processEnvironment );

    m_diffProcess.setStandardOutputFile( DIFF_FILE_NAME );
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
    m_p4WorkingDir = directory;

    QProcess process;
    // The 'p4' command needs to be executed from within the directory under perforce control,
    // process.setWorkingDirectory(m_p4WorkingDir) does not work,
    // using QDir::setCurrent(m_p4WorkingDir) works
    QDir::setCurrent(m_p4WorkingDir);
    process.start ( "p4"
                    " fstat"
                    " -T clientFile,movedRev,headRev,haveRev,action,unresolved"
                    " -F haveRev|(^haveRev&^(headAction=delete|headAction=move/delete|headAction=purge))"
                    " ..." );

    // The output of this command consists of blocks of up till 5 lines separated by a blank line.
    // The format of each block is:
    //    "... clientFile " followed by the local file path
    //    "... movedRev " followed by a revision number of the latest revision on the server
    //                    of a file that have moved in the local branch
    //    "... headRev " followed by the revision number of the local version of the file
    //    "... haveRev " followed by a revision number of the latest revision on the server
    //    "... action " followed by an action
    //    "... unresolved"
    // The first line in mandatory, the remaning lines can be missing,
    // the order however is constant

    if ( !process.waitForStarted() ) {
        emit errorMessage ( QLatin1String ( "Could not start 'p4 fstat' command." ) );
        return false;
    }

    // Not sure if this is needed
    if ( !process.waitForFinished() ) {
        emit errorMessage ( QLatin1String ( "Error while executing 'p4 fstat' command." ) );
        return false;
    }

    QStringList strings;
    while ( process.state() !=QProcess::NotRunning || !process.atEnd() ) {
        if ( !process.canReadLine() ) {
            process.waitForReadyRead();
            continue;
        }

        char buffer[1024];
        if ( process.readLine ( buffer, sizeof ( buffer ) ) <= 0 ) {
            emit errorMessage ( QLatin1String ( "Error reading output from 'p4 fstat' command." ) );
            break;
        }

        strings.append ( QString::fromUtf8( buffer ) );
        if ( strings.last() != QLatin1String ( "\n" ) ) {
            continue;
        }
        strings.removeLast();

        if ( strings.isEmpty() ) {
            emit errorMessage ( QLatin1String ( "Error reading output from 'p4 fstat' command: only newline read." ) );
            return false;
        }

        static const int clientFileStartPos = sizeof ( "... clientFile" );
        const int lengthFileName = strings.first().length() - clientFileStartPos -1;
        QString filePath = strings.first().mid ( clientFileStartPos, lengthFileName );

        QString serverRev;
        QString haveRev;
        QString action;

        if ( strings.last().startsWith ( "... unresolved" ) ) {
            updateFileVersion ( filePath, ConflictingVersion );
            strings.clear();
            continue;
        }

        if ( strings.last().startsWith ( "... action" ) ) {
            static const int pos = sizeof ( "... action" );
            int length = strings.last().length() - pos -1;
            action = strings.takeLast().mid ( pos, length );
        }

        if ( strings.last().startsWith ( "... haveRev" ) ) {
            static const int pos = sizeof ( "... haveRev" );
            int length = strings.last().length() - pos -1;
            haveRev = strings.takeLast().mid ( pos, length );
        }

        if ( strings.last().startsWith ( "... headRev" ) ) {
            static const int pos = sizeof ( "... headRev" );
            int length = strings.last().length() - pos -1;
            serverRev = strings.takeLast().mid ( pos, length );
        } else if ( strings.last().startsWith ( "... movedRev" ) ) {
            static const int pos = sizeof ( "... movedRev" );
            int length = strings.last().length() - pos -1;
            serverRev = strings.takeLast().mid ( pos, length );
        }

        bool needsUpdate = !haveRev.isEmpty() && !serverRev.isEmpty() && ( haveRev != serverRev );

        if ( action.isEmpty() ) {
            if ( !needsUpdate ) {
                updateFileVersion ( filePath, NormalVersion );
            } else {
                updateFileVersion ( filePath, UpdateRequiredVersion );
            }
        } else if ( needsUpdate ) {
            updateFileVersion ( filePath, ConflictingVersion );
        } else  if ( action.isEmpty() ) {
            updateFileVersion ( filePath, NormalVersion );
        } else if ( action=="edit" || action=="integrate" ) {
            updateFileVersion ( filePath, LocallyModifiedVersion );
        } else if ( action=="add" || action=="move/add" || action=="import" || action=="branch" ) {
            updateFileVersion ( filePath, AddedVersion );
        } else if ( action=="delete" || action=="move/delete" || action=="purge" ) {
            updateFileVersion ( filePath, RemovedVersion );
        } else if ( action=="archive" ) {
            updateFileVersion ( filePath, NormalVersion );
        } else {
            kWarning() << "Unknown perforce file version: " << action;
            updateFileVersion ( filePath, NormalVersion );
        }
        strings.clear();
    }

    if ( ( process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit ) ) {
        emit errorMessage ( QLatin1String ( "p4 error: " ) + process.errorString() );
        return false;
    }
    return true;
}

void FileViewPerforcePlugin::updateFileVersion ( const QString& filePath, ItemVersion version )
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

    QHash<QString, ItemVersion>::const_iterator it = m_versionInfoHash.find ( itemUrl );
    if ( it != m_versionInfoHash.end() ) {
        return *it;
    }

    it = m_versionInfoHashDir.find ( itemUrl );
    if ( it != m_versionInfoHashDir.end() ) {
        return *it;
    }

    return UnversionedVersion;
}

QList<QAction*> FileViewPerforcePlugin::actions ( const KFileItemList& items ) const
{
    foreach ( const KFileItem& item, items ) {
        m_contextItems.append ( item );
    }

    const bool noPendingOperation = !m_pendingOperation;
    if ( noPendingOperation ) {
        // iterate all items and check the version to know which
        // actions can be enabled
        const int itemsCount = items.count();
        int versionedCount = 0;
        int editingCount = 0;
        int diffableAgainstHeadRev = 0;
        int diffableAgainstHaveRev = 0;
        int conflictCount = 0;
        int dirCount = 0;
        foreach ( const KFileItem& item, items ) {
            const ItemVersion version = itemVersion ( item );
            if ( version != UnversionedVersion ) {
                ++versionedCount;
            }
            if ( item.isDir() ) {
                ++dirCount;
            }

            switch ( version ) {
            case LocallyModifiedVersion:
                ++editingCount;
                ++diffableAgainstHaveRev;
                break;
            case AddedVersion:
            case RemovedVersion:
                ++editingCount;
                break;
            case ConflictingVersion:
                ++editingCount;
                ++diffableAgainstHaveRev;
                ++diffableAgainstHeadRev;
                ++conflictCount;
                break;
            case UpdateRequiredVersion:
                ++diffableAgainstHeadRev;
                break;
            default:
                break;
            }
        }
        m_revertAction->setEnabled ( editingCount > 0 );
        m_revertUnchangedAction->setEnabled ( editingCount > 0 );
        m_diffActionHaveRev->setEnabled ( diffableAgainstHaveRev > 0 );
        m_diffActionHeadRev->setEnabled ( diffableAgainstHeadRev > 0 );
        m_addAction->setEnabled ( versionedCount < itemsCount || dirCount > 0 );
        m_removeAction->setEnabled ( versionedCount == itemsCount && editingCount == 0 );
        m_openForEditAction->setEnabled ( editingCount < itemsCount && versionedCount > 0 );
        m_resolveAction->setEnabled ( conflictCount > 0 );
        m_timelapsviewAction->setEnabled ( versionedCount == 1 && itemsCount==1 && dirCount==0 );
        m_showInP4VAction->setEnabled ( versionedCount == 1 && itemsCount==1 );
        m_submitAction->setEnabled ( editingCount == 1 && itemsCount==1 && dirCount==0 );
    } else {
        m_revertAction->setEnabled ( false );
        m_revertUnchangedAction->setEnabled ( false );
        m_diffActionHaveRev->setEnabled ( false );
        m_diffActionHeadRev->setEnabled ( false );
        m_addAction->setEnabled ( false );
        m_removeAction->setEnabled ( false );
        m_openForEditAction->setEnabled ( false );
        m_resolveAction->setEnabled ( false );
        m_timelapsviewAction->setEnabled ( false );
        m_showInP4VAction->setEnabled ( false );
        m_submitAction->setEnabled ( false );
    }
    m_updateAction->setEnabled ( noPendingOperation );

    QList<QAction*> actions;
    actions.append ( m_openForEditAction );
    actions.append ( m_updateAction );
    actions.append ( m_addAction );
    actions.append ( m_removeAction );
    actions.append ( m_revertAction );
    actions.append ( m_revertUnchangedAction );
    actions.append ( m_diffActionHaveRev );
    actions.append ( m_diffActionHeadRev );
    actions.append ( m_resolveAction );
    actions.append ( m_timelapsviewAction );
    actions.append ( m_showInP4VAction );
    actions.append ( m_submitAction );

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
    QStringList arguments;
    arguments << "-a";
    execPerforceCommand ( QLatin1String ( "reconcile" ), arguments,
                          i18nc ( "@info:status", "Adding files to Perforce repository..." ),
                          i18nc ( "@info:status", "Adding files to Perforce repository failed." ),
                          i18nc ( "@info:status", "Added files to Perforce repository." ) );
}

void FileViewPerforcePlugin::removeFiles()
{
    execPerforceCommand ( QLatin1String ( "delete" ), QStringList(),
                          i18nc ( "@info:status", "Deleting files from Perforce repository..." ),
                          i18nc ( "@info:status", "Deleting files from Perforce repository failed." ),
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
                          i18nc ( "@info:status", "Reverting files from Perforce repository failed." ),
                          i18nc ( "@info:status", "Reverted files from Perforce repository." ) );
}

void FileViewPerforcePlugin::revertUnchangedFiles()
{
    QStringList arguments;
    arguments << "-a";
    execPerforceCommand ( QLatin1String ( "revert" ), arguments,
                          i18nc ( "@info:status", "Reverting unchanged files from Perforce repository..." ),
                          i18nc ( "@info:status", "Reverting unchanged files from Perforce repository failed." ),
                          i18nc ( "@info:status", "Reverted unchanged files from Perforce repository." ) );
}

void FileViewPerforcePlugin::diffAgainstRev( const QString& rev )
{
    QStringList arguments;
    arguments << "diff" << "-du";
    foreach ( const KFileItem& item, m_contextItems ) {
        QString str = item.localPath();
        if( item.isDir() )
        {
            str += "/...";
        }
        str += QLatin1String("#") % rev;
        arguments << str;
    }
    m_contextItems.clear();

    QFile::remove( DIFF_FILE_NAME );

    emit infoMessage ( i18nc ( "@info:status", "Performing perforce diff..." ) );

    m_errorMsg = i18nc ( "@info:status", "Perforce diff failed." ) ;
    m_operationCompletedMsg = i18nc ( "@info:status", "Perforce diff compleet." ) ;

    m_pendingOperation = true;

    // The 'p4' command needs to be executed from within the directory under perforce control,
    // m_diffProcess.setWorkingDirectory(m_p4WorkingDir) does not work,
    // using QDir::setCurrent(m_p4WorkingDir) works
    QDir::setCurrent(m_p4WorkingDir);
    m_diffProcess.start( "p4", arguments );
}

void FileViewPerforcePlugin::diffAgainstHaveRev()
{
    diffAgainstRev("have");
}

void FileViewPerforcePlugin::diffAgainstHeadRev()
{
    diffAgainstRev("head");
}

void FileViewPerforcePlugin::resolveConflict()
{
    QString files;
    foreach ( const KFileItem& item, m_contextItems ) {
        files += QLatin1String(" ") % KShell::quoteArg(item.localPath());
        if( item.isDir() )
        {
            files += "/...";
        }
    }
    m_contextItems.clear();

    emit infoMessage ( i18nc ( "@info:status", "Launcing external conflict resolver" ) );
    m_operationCompletedMsg = i18nc ( "@info:status", "Launched Perforce Resolve." );
    m_errorMsg = i18nc ( "@info:status", "Launcing Perforce Resolve failed." );

    bool res = KRun::runCommand( QLatin1String("p4vc resolve ") % files, 0, m_p4WorkingDir );

    if ( res )
    {
        emit operationCompletedMessage ( m_operationCompletedMsg );
    }
    else
    {
        emit errorMessage ( m_errorMsg );
    }
}

void FileViewPerforcePlugin::timelapsview()
{
    QString path = m_contextItems.first().localPath(); // only one specified
    m_contextItems.clear();

    emit infoMessage ( i18nc ( "@info:status", "Launcing Perforce Timelapsview..." ) ); //TODO: space in filename...
    m_operationCompletedMsg = i18nc ( "@info:status", "Launched Perforce Timelapsview." );
    m_errorMsg = i18nc ( "@info:status", "Launcing Perforce Timelapsview failed." );

    bool res = KRun::runCommand( QLatin1String("p4vc timelapseview ") % KShell::quoteArg(path), 0, m_p4WorkingDir );

    if ( res )
    {
        emit operationCompletedMessage ( m_operationCompletedMsg );
    }
    else
    {
        emit errorMessage ( m_errorMsg );
    }
}

void FileViewPerforcePlugin::showInP4V()
{
    QString path = m_contextItems.first().localPath(); // only one specified
    m_contextItems.clear();

    emit infoMessage ( i18nc ( "@info:status", "Launcing P4V..." ) );
    m_operationCompletedMsg = i18nc ( "@info:status", "Launched P4V." );
    m_errorMsg = i18nc ( "@info:status", "Launcing P4V failed." );

    // The command to run is "p4v -s path", but we need to give the p4-port, p4-client name and p4 username
    // they are returned by "p4 set" in a format like this:
    //     P4CLIENT=my_workspace (config)
    //     P4PORT=localhost:1666 (config)
    //     P4USER=user
    // sed are used to filter the output and export the variabels P4CLIENT, P4PORT, and P4USER
    bool res = KRun::runCommand( QLatin1String("`p4 set | sed -n 's/\\(^P4[^=]*=[^ ]*\\).*/export \\1/p'`;") %
                                 QLatin1String("p4v -p ${P4PORT} -c ${P4CLIENT} -u ${P4USER} -s ") % KShell::quoteArg(path),
                                 "p4v", QString(), 0, QByteArray(), m_p4WorkingDir );

    if ( res )
    {
        emit operationCompletedMessage ( m_operationCompletedMsg );
    }
    else
    {
        emit errorMessage ( m_errorMsg );
    }
}

void FileViewPerforcePlugin::submit()
{
    QString path = m_contextItems.first().localPath(); // only one specified
    m_contextItems.clear();

    emit infoMessage ( i18nc ( "@info:status", "Launcing P4V submit..." ) );
    m_operationCompletedMsg = i18nc ( "@info:status", "Launched P4V submit." );
    m_errorMsg = i18nc ( "@info:status", "Launcing P4V submit failed." );

    // The command to run is 'p4v -cmd "submit path"', but we need to give the p4-port, p4-client name and p4 username
    // they are returned by "p4 set" in a format like this:
    //     P4CLIENT=my_workspace (config)
    //     P4PORT=localhost:1666 (config)
    //     P4USER=user
    // sed are used to filter the output and export the variabels P4CLIENT, P4PORT, and P4USER
    bool res = KRun::runCommand( QLatin1String("`p4 set | sed -n 's/\\(^P4[^=]*=[^ ]*\\).*/export \\1/p'`;") %
                                 QLatin1String("p4v -p ${P4PORT} -c ${P4CLIENT} -u ${P4USER} -cmd \"submit ") % path % "\"",
                                 "p4v", QString(), 0, QByteArray(), m_p4WorkingDir );

    if ( res )
    {
        emit operationCompletedMessage ( m_operationCompletedMsg );
    }
    else
    {
        emit errorMessage ( m_errorMsg );
    }
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

void FileViewPerforcePlugin::slotDiffOperationCompleted ( int exitCode, QProcess::ExitStatus exitStatus )
{
    m_pendingOperation = false;

    if ( ( exitStatus != QProcess::NormalExit ) || ( exitCode != 0 ) ) {
        emit errorMessage ( m_errorMsg );
    } else {
        emit operationCompletedMessage ( m_operationCompletedMsg );
        if( QFile::exists( DIFF_FILE_NAME ) && QFile( DIFF_FILE_NAME ).size() > 0 )
        {
            emit infoMessage ( "Launcing external diff viewer" );
            KRun::runCommand( QLatin1String("kompare ") % DIFF_FILE_NAME % QLatin1String("; rm ") % DIFF_FILE_NAME, 0);
        }
        else
        {
            emit operationCompletedMessage ( "No diff to show" );
        }
    }
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
    arguments << m_command << m_arguments;

    const KFileItem item = m_contextItems.takeLast();
    if ( item.isDir() ) {
        arguments << item.localPath().append ( "/..." ); // append '...' to make the operation recursive
    } else {
        arguments << item.localPath();
    }
    // the remaining items of m_contextItems will be executed
    // after the process has finished (see slotOperationCompleted())

    // The 'p4' command needs to be executed from within the directory under perforce control,
    // m_process.setWorkingDirectory(m_p4WorkingDir) does not work,
    // using QDir::setCurrent(m_p4WorkingDir) works
    QDir::setCurrent(m_p4WorkingDir);
    m_process.start ( program, arguments );
}

#include "fileviewperforceplugin.moc"
