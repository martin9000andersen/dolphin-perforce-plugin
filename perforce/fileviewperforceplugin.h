/***************************************************************************
 *   Copyright (C) 2012 Martin Andersen  <martin9000andersen gmail.com>    *
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

#ifndef FILEVIEWPERFORCEPLUGIN_H
#define FILEVIEWPERFORCEPLUGIN_H

#include <kfileitem.h>
#include <kversioncontrolplugin2.h>
#include <QHash>
#include <QProcess>

/**
 * @brief Perforce implementation for the KVersionControlPlugin interface.
 */
class FileViewPerforcePlugin : public KVersionControlPlugin2
{
    Q_OBJECT

public:
    FileViewPerforcePlugin(QObject* parent, const QList<QVariant>& args);
    virtual ~FileViewPerforcePlugin();
    virtual QString fileName() const;
    virtual bool beginRetrieval(const QString& directory);
    virtual void endRetrieval();
    virtual ItemVersion itemVersion(const KFileItem& item) const;
    virtual QList<QAction*> actions(const KFileItemList& items) const;

private slots:
    void updateFiles();
    void addFiles();
    void openFilesForEdit();
    void removeFiles();
    void revertFiles();
    void revertUnchangedFiles();
    void diffAgainstHaveRev();
    void diffAgainstHeadRev();

    void slotOperationCompleted(int exitCode, QProcess::ExitStatus exitStatus);
    void slotOperationError();
    void slotDiffOperationCompleted(int exitCode, QProcess::ExitStatus exitStatus);

private:
    /**
     * Executes the command "perforce {perforceCommand}" for the files that have been
     * set by getting the context menu actions (see contextMenuActions()).
     * @param infoMsg     Message that should be shown before the command is executed.
     * @param errorMsg    Message that should be shown if the execution of the command
     *                    has been failed.
     * @param operationCompletedMsg
     *                    Message that should be shown if the execution of the command
     *                    has been completed successfully.
     */
    void execPerforceCommand(const QString& perforceCommand,
                        const QStringList& arguments,
                        const QString& infoMsg,
                        const QString& errorMsg,
                        const QString& operationCompletedMsg);

    void startPerforceCommandProcess();

    void updateFileVersion( const QString& filePath, KVersionControlPlugin2::ItemVersion version );

    void diffAgainstRev(const QString& rev);

    QList<QAction*> directoryActions(const QString& directory) const;

    bool m_pendingOperation;
    QHash<QString, ItemVersion> m_versionInfoHash;
    QHash<QString, ItemVersion> m_versionInfoHashDir;

    QAction* m_updateAction;
    QAction* m_addAction;
    QAction* m_removeAction;
    QAction* m_openForEditAction;
    QAction* m_revertAction;
    QAction* m_revertUnchangedAction;
    QAction* m_diffActionHaveRev;
    QAction* m_diffActionHeadRev;

    QString m_command;
    QStringList m_arguments;
    QString m_errorMsg;
    QString m_operationCompletedMsg;

    mutable QString m_contextDir;
    mutable KFileItemList m_contextItems;

    QProcess m_process;
    QProcess m_diffProcess;
    QString m_perforceConfigName;
    QString m_p4WorkingDir;
};
#endif // FILEVIEWPERFORCEPLUGIN_H

