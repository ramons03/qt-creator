/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.3, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#ifndef GITCLIENT_H
#define GITCLIENT_H

#include "gitsettings.h"

#include <coreplugin/iversioncontrol.h>
#include <coreplugin/editormanager/ieditor.h>
#include <projectexplorer/environment.h>

#include <QtCore/QString>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE
class QErrorMessage;
QT_END_NAMESPACE

namespace Core {
    class ICore;
}

namespace VCSBase {
    class VCSBaseEditor;
}

namespace Git {
namespace Internal {

class GitPlugin;
class GitOutputWindow;
class GitCommand;
struct CommitData;
struct GitSubmitEditorPanelData;

class GitClient : public QObject
{
    Q_OBJECT

public:
    explicit GitClient(GitPlugin *plugin, Core::ICore *core);
    ~GitClient();

    bool managesDirectory(const QString &) const { return false; }
    QString findTopLevelForDirectory(const QString &) const { return QString(); }

    static QString findRepositoryForFile(const QString &fileName);
    static QString findRepositoryForDirectory(const QString &dir);

    void diff(const QString &workingDirectory, const QString &fileName);
    void diff(const QString &workingDirectory, const QStringList &fileNames);

    void status(const QString &workingDirectory);
    void log(const QString &workingDirectory, const QString &fileName);
    void blame(const QString &workingDirectory, const QString &fileName);
    void showCommit(const QString &workingDirectory, const QString &commit);
    void checkout(const QString &workingDirectory, const QString &file);
    void checkoutBranch(const QString &workingDirectory, const QString &branch);
    void hardReset(const QString &workingDirectory, const QString &commit);
    void addFile(const QString &workingDirectory, const QString &fileName);
    bool synchronousAdd(const QString &workingDirectory, const QStringList &files);
    bool synchronousReset(const QString &workingDirectory, const QStringList &files);
    bool synchronousReset(const QString &workingDirectory, const QStringList &files, QString *errorMessage);
    bool synchronousCheckout(const QString &workingDirectory, const QStringList &files, QString *errorMessage);
    bool synchronousStash(const QString &workingDirectory, QString *errorMessage);
    bool synchronousBranchCmd(const QString &workingDirectory, QStringList branchArgs,
                              QString *output, QString *errorMessage);
    bool synchronousShow(const QString &workingDirectory, const QString &id,
                              QString *output, QString *errorMessage);

    void pull(const QString &workingDirectory);
    void push(const QString &workingDirectory);

    void stash(const QString &workingDirectory);
    void stashPop(const QString &workingDirectory);
    void revert(const QStringList &files);
    void branchList(const QString &workingDirectory);
    void stashList(const QString &workingDirectory);

    QString readConfig(const QString &workingDirectory, const QStringList &configVar);

    QString readConfigValue(const QString &workingDirectory, const QString &configVar);

    enum StashResult { StashUnchanged, StashCanceled, StashFailed,
                       Stashed, NotStashed /* User did not want it */ };
    StashResult ensureStash(const QString &workingDirectory, QString *errorMessage);
    StashResult ensureStash(const QString &workingDirectory);

    bool getCommitData(const QString &workingDirectory,
                       QString *commitTemplate,
                       CommitData *d,
                       QString *errorMessage);

    bool addAndCommit(const QString &workingDirectory,
                      const GitSubmitEditorPanelData &data,
                      const QString &messageFile,
                      const QStringList &checkedFiles,
                      const QStringList &origCommitFiles);

    enum StatusResult { StatusChanged, StatusUnchanged, StatusFailed };
    StatusResult gitStatus(const QString &workingDirectory,
                           bool untracked = false,
                           QString *output = 0,
                           QString *errorMessage = 0);

    GitSettings  settings() const;
    void setSettings(const GitSettings &s);

    static QString msgNoChangedFiles();

public slots:
    void show(const QString &source, const QString &id);

private slots:
    void appendAndPopup(const QString &text);
    void appendDataAndPopup(const QByteArray &data);

private:
    VCSBase::VCSBaseEditor *createVCSEditor(const QString &kind,
                                                 QString title,
                                                 const QString &source,
                                                 bool setSourceCodec,
                                                 const char *registerDynamicProperty,
                                                 const QString &dynamicPropertyValue) const;


    void executeGit(const QString &workingDirectory,
                                           const QStringList &arguments,
                                           VCSBase::VCSBaseEditor* editor = 0,
                                           bool outputToWindow = false);

    bool synchronousGit(const QString &workingDirectory,
                        const QStringList &arguments,
                        QByteArray* outputText = 0,
                        QByteArray* errorText = 0,
                        bool logCommandToWindow = true);

    enum RevertResult { RevertOk, RevertUnchanged, RevertCanceled, RevertFailed };
    RevertResult revertI(QStringList files, bool *isDirectory, QString *errorMessage);

    const QString m_msgWait;
    GitPlugin     *m_plugin;
    Core::ICore   *m_core;
    GitSettings   m_settings;
};

class GitCommand : public QObject
{
    Q_OBJECT
public:
    GitCommand();
    ~GitCommand();
    void execute(const QStringList &arguments,
                 const QString &workingDirectory,
                 const ProjectExplorer::Environment &environment);
    void run(const QStringList &arguments,
                 const QString &workingDirectory,
                 const ProjectExplorer::Environment &environment);

Q_SIGNALS:
    void outputData(const QByteArray&);
    void outputText(const QString&);
    void errorText(const QString&);
};

} // namespace Internal
} // namespace Git

#endif // GITCLIENT_H
