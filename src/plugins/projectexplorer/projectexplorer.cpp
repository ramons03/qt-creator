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

#include "applicationrunconfiguration.h"
#include "allprojectsfilter.h"
#include "allprojectsfind.h"
#include "currentprojectfind.h"
#include "buildmanager.h"
#include "buildsettingspropertiespage.h"
#include "editorsettingspropertiespage.h"
#include "currentprojectfilter.h"
#include "customexecutablerunconfiguration.h"
#include "foldernavigationwidget.h"
#include "iprojectmanager.h"
#include "metatypedeclarations.h"
#include "nodesvisitor.h"
#include "outputwindow.h"
#include "persistentsettings.h"
#include "pluginfilefactory.h"
#include "processstep.h"
#include "projectexplorer.h"
#include "projectexplorerconstants.h"
#include "projectfilewizardextension.h"
#include "projecttreewidget.h"
#include "projectwindow.h"
#include "removefiledialog.h"
#include "runsettingspropertiespage.h"
#include "scriptwrappers.h"
#include "session.h"
#include "sessiondialog.h"

#include <coreplugin/basemode.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/mainwindow.h>
#include <coreplugin/mimedatabase.h>
#include <coreplugin/filemanager.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/actionmanager/actionmanagerinterface.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/editormanager/ieditorfactory.h>
#include <coreplugin/findplaceholder.h>
#include <coreplugin/basefilewizard.h>
#include <coreplugin/mainwindow.h>
#include <coreplugin/welcomemode.h>
#include <coreplugin/vcsmanager.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>
#include <utils/listutils.h>
#include <utils/qtcassert.h>

#include <QtCore/qplugin.h>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QSettings>

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QFileDialog>
#include <QtGui/QFileSystemModel>
#include <QtGui/QHeaderView>
#include <QtGui/QInputDialog>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QToolBar>

Q_DECLARE_METATYPE(QSharedPointer<ProjectExplorer::RunConfiguration>);
Q_DECLARE_METATYPE(Core::IEditorFactory *);

namespace {
bool debug = false;
}

using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

CoreListenerCheckingForRunningBuild::CoreListenerCheckingForRunningBuild(BuildManager *manager)
    : Core::ICoreListener(0), m_manager(manager)
{
}

bool CoreListenerCheckingForRunningBuild::coreAboutToClose()
{
    if (m_manager->isBuilding()) {
        QMessageBox box;
        QPushButton *closeAnyway = box.addButton(tr("Cancel Build && Close"), QMessageBox::AcceptRole);
        QPushButton *cancelClose = box.addButton(tr("Don't Close"), QMessageBox::RejectRole);
        box.setDefaultButton(cancelClose);
        box.setWindowTitle(tr("Close QtCreator?"));
        box.setText(tr("A project is currently being built."));
        box.setInformativeText(tr("Do you want to cancel the build process and close QtCreator anyway?"));
        box.exec();
        return (box.clickedButton() == closeAnyway);
    }
    return true;
}

ProjectExplorerPlugin *ProjectExplorerPlugin::m_instance = 0;

ProjectExplorerPlugin::ProjectExplorerPlugin()
    : m_buildConfigurationActionGroup(0),
      m_runConfigurationActionGroup(0),
      m_currentProject(0),
      m_currentNode(0),
      m_delayedRunConfiguration(0),
      m_debuggingRunControl(0)
{
    m_instance = this;
}

ProjectExplorerPlugin::~ProjectExplorerPlugin()
{
    removeObject(this);
}

ProjectExplorerPlugin *ProjectExplorerPlugin::instance()
{
    return m_instance;
}

bool ProjectExplorerPlugin::initialize(const QStringList & /*arguments*/, QString *)
{
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    m_core = pm->getObject<Core::ICore>();
    Core::ActionManagerInterface *am = m_core->actionManager();

    addObject(this);

    connect(m_core->fileManager(), SIGNAL(currentFileChanged(const QString&)),
            this, SLOT(setCurrentFile(const QString&)));

    m_session = new SessionManager(m_core, this);

    connect(m_session, SIGNAL(projectAdded(ProjectExplorer::Project *)),
            this, SIGNAL(fileListChanged()));
    connect(m_session, SIGNAL(aboutToRemoveProject(ProjectExplorer::Project *)),
            this, SLOT(invalidateProject(ProjectExplorer::Project *)));
    connect(m_session, SIGNAL(projectRemoved(ProjectExplorer::Project *)),
            this, SIGNAL(fileListChanged()));
    connect(m_session, SIGNAL(startupProjectChanged(ProjectExplorer::Project *)),
            this, SLOT(startupProjectChanged()));

    m_proWindow = new ProjectWindow(m_core);

    QList<int> globalcontext;
    globalcontext.append(Core::Constants::C_GLOBAL_ID);

    QList<int> pecontext;
    pecontext << m_core->uniqueIDManager()->uniqueIdentifier(Constants::C_PROJECTEXPLORER);

    Core::BaseMode *mode = new Core::BaseMode(tr("Projects"),
                                              Constants::MODE_SESSION,
                                              QIcon(QLatin1String(":/fancyactionbar/images/mode_Project.png")),
                                              Constants::P_MODE_SESSION,
                                              m_proWindow);
    mode->setContext(QList<int>() << pecontext);
    addAutoReleasedObject(mode);
    m_proWindow->layout()->addWidget(new Core::FindToolBarPlaceHolder(mode));

    m_buildManager = new BuildManager(this);
    connect(m_buildManager, SIGNAL(buildStateChanged(ProjectExplorer::Project *)),
            this, SLOT(buildStateChanged(ProjectExplorer::Project *)));
    connect(m_buildManager, SIGNAL(buildQueueFinished(bool)),
            this, SLOT(buildQueueFinished(bool)));
    connect(m_buildManager, SIGNAL(tasksChanged()),
            this, SLOT(updateTaskActions()));

    addAutoReleasedObject(new CoreListenerCheckingForRunningBuild(m_buildManager));

    m_outputPane = new OutputPane();
    addAutoReleasedObject(m_outputPane);
    connect(m_session, SIGNAL(projectRemoved(ProjectExplorer::Project *)),
            m_outputPane, SLOT(projectRemoved()));

    AllProjectsFilter *allProjectsFilter = new AllProjectsFilter(this, m_core);
    addAutoReleasedObject(allProjectsFilter);

    CurrentProjectFilter *currentProjectFilter = new CurrentProjectFilter(this, m_core);
    addAutoReleasedObject(currentProjectFilter);

    addAutoReleasedObject(new BuildSettingsPanelFactory);
    addAutoReleasedObject(new RunSettingsPanelFactory);
    addAutoReleasedObject(new EditorSettingsPanelFactory);

    ProcessStepFactory *processStepFactory = new ProcessStepFactory;
    addAutoReleasedObject(processStepFactory);

    AllProjectsFind *allProjectsFind = new AllProjectsFind(this, m_core,
        m_core->pluginManager()->getObject<Find::SearchResultWindow>());
    addAutoReleasedObject(allProjectsFind);

    CurrentProjectFind *currentProjectFind = new CurrentProjectFind(this, m_core,
        m_core->pluginManager()->getObject<Find::SearchResultWindow>());
    addAutoReleasedObject(currentProjectFind);

    addAutoReleasedObject(new ApplicationRunConfigurationRunner);
    addAutoReleasedObject(new CustomExecutableRunConfigurationFactory);

    addAutoReleasedObject(new ProjectFileWizardExtension(m_core));

    // context menus
    Core::IActionContainer *msessionContextMenu =
        am->createMenu(Constants::M_SESSIONCONTEXT);
    Core::IActionContainer *mproject =
        am->createMenu(Constants::M_PROJECTCONTEXT);
    Core::IActionContainer *msubProject =
        am->createMenu(Constants::M_SUBPROJECTCONTEXT);
    Core::IActionContainer *mfolder =
        am->createMenu(Constants::M_FOLDERCONTEXT);
    Core::IActionContainer *mfilec =
        am->createMenu(Constants::M_FILECONTEXT);

    m_sessionContextMenu = msessionContextMenu->menu();
    m_projectMenu = mproject->menu();
    m_subProjectMenu = msubProject->menu();
    m_folderMenu = mfolder->menu();
    m_fileMenu = mfilec->menu();

    Core::IActionContainer *mfile =
        am->actionContainer(Core::Constants::M_FILE);
    Core::IActionContainer *menubar =
        am->actionContainer(Core::Constants::MENU_BAR);

    // mode manager (for fancy actions)
    Core::ModeManager *modeManager = m_core->modeManager();

    // build menu
    Core::IActionContainer *mbuild =
        am->createMenu(Constants::M_BUILDPROJECT);
    mbuild->menu()->setTitle("&Build");
    menubar->addMenu(mbuild, Core::Constants::G_VIEW);

    // debug menu
    Core::IActionContainer *mdebug =
        am->createMenu(Constants::M_DEBUG);
    mdebug->menu()->setTitle("&Debug");
    menubar->addMenu(mdebug, Core::Constants::G_VIEW);


    //
    // Groups
    //

    mbuild->appendGroup(Constants::G_BUILD_SESSION);
    mbuild->appendGroup(Constants::G_BUILD_PROJECT);
    mbuild->appendGroup(Constants::G_BUILD_OTHER);
    mbuild->appendGroup(Constants::G_BUILD_RUN);
    mbuild->appendGroup(Constants::G_BUILD_TASK);
    mbuild->appendGroup(Constants::G_BUILD_CANCEL);

    msessionContextMenu->appendGroup(Constants::G_SESSION_BUILD);
    msessionContextMenu->appendGroup(Constants::G_SESSION_FILES);
    msessionContextMenu->appendGroup(Constants::G_SESSION_OTHER);
    msessionContextMenu->appendGroup(Constants::G_SESSION_CONFIG);

    mproject->appendGroup(Constants::G_PROJECT_OPEN);
    mproject->appendGroup(Constants::G_PROJECT_NEW);
    mproject->appendGroup(Constants::G_PROJECT_BUILD);
    mproject->appendGroup(Constants::G_PROJECT_RUN);
    mproject->appendGroup(Constants::G_PROJECT_FILES);
    mproject->appendGroup(Constants::G_PROJECT_OTHER);
    mproject->appendGroup(Constants::G_PROJECT_CONFIG);

    msubProject->appendGroup(Constants::G_PROJECT_OPEN);
    msubProject->appendGroup(Constants::G_PROJECT_FILES);
    msubProject->appendGroup(Constants::G_PROJECT_OTHER);
    msubProject->appendGroup(Constants::G_PROJECT_CONFIG);

    mfolder->appendGroup(Constants::G_FOLDER_FILES);
    mfolder->appendGroup(Constants::G_FOLDER_OTHER);
    mfolder->appendGroup(Constants::G_FOLDER_CONFIG);

    mfilec->appendGroup(Constants::G_FILE_OPEN);
    mfilec->appendGroup(Constants::G_FILE_OTHER);
    mfilec->appendGroup(Constants::G_FILE_CONFIG);

    // "open with" submenu
    Core::IActionContainer * const openWith =
            am->createMenu(ProjectExplorer::Constants::M_OPENFILEWITHCONTEXT);
    m_openWithMenu = openWith->menu();
    m_openWithMenu->setTitle(tr("Open With"));

    connect(mfilec->menu(), SIGNAL(aboutToShow()), this, SLOT(populateOpenWithMenu()));
    connect(m_openWithMenu, SIGNAL(triggered(QAction *)),
            this, SLOT(openWithMenuTriggered(QAction *)));

    //
    // Separators
    //

    Core::ICommand *cmd;
    QAction *sep;

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Build.Sep"), globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Files.Sep"), globalcontext);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);
    mproject->addAction(cmd, Constants::G_PROJECT_FILES);
    msubProject->addAction(cmd, Constants::G_PROJECT_FILES);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.New.Sep"), globalcontext);
    mproject->addAction(cmd, Constants::G_PROJECT_NEW);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Config.Sep"), globalcontext);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_CONFIG);
    mproject->addAction(cmd, Constants::G_PROJECT_CONFIG);
    msubProject->addAction(cmd, Constants::G_PROJECT_CONFIG);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Close.Sep"), globalcontext);
    mfile->addAction(cmd, Core::Constants::G_FILE_CLOSE);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Projects.Sep"), globalcontext);
    mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Other.Sep"), globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_OTHER);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_OTHER);
    mproject->addAction(cmd, Constants::G_PROJECT_OTHER);
    msubProject->addAction(cmd, Constants::G_PROJECT_OTHER);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Run.Sep"), globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_RUN);
    mproject->addAction(cmd, Constants::G_PROJECT_RUN);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.Task.Sep"), globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_TASK);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("ProjectExplorer.CancelBuild.Sep"), globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_CANCEL);

    //
    // Actions
    //

    // new session action
    m_sessionManagerAction = new QAction(tr("Session Manager..."), this);
    cmd = am->registerAction(m_sessionManagerAction, Constants::NEWSESSION, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence());

    // new action
    m_newAction = new QAction(tr("New Project..."), this);
    cmd = am->registerAction(m_newAction, Constants::NEWPROJECT, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+N")));
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);

#if 0
    // open action
    m_loadAction = new QAction(tr("Load Project..."), this);
    cmd = am->registerAction(m_loadAction, Constants::LOAD, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+O")));
    mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);
#endif

    // Default open action
    m_openFileAction = new QAction(tr("Open File"), this);
    cmd = am->registerAction(m_openFileAction, ProjectExplorer::Constants::OPENFILE,
                       globalcontext);
    mfilec->addAction(cmd, Constants::G_FILE_OPEN);

    // Open With menu
    mfilec->addMenu(openWith, ProjectExplorer::Constants::G_FILE_OPEN);

    // recent projects menu
    Core::IActionContainer *mrecent =
        am->createMenu(Constants::M_RECENTPROJECTS);
    mrecent->menu()->setTitle("Recent Projects");
    mfile->addMenu(mrecent, Core::Constants::G_FILE_OPEN);
    connect(mfile->menu(), SIGNAL(aboutToShow()),
        this, SLOT(updateRecentProjectMenu()));

    // unload action
    m_unloadAction = new QAction(tr("Unload Project"), this);
    cmd = am->registerAction(m_unloadAction, Constants::UNLOAD, globalcontext);
    cmd->setAttribute(Core::ICommand::CA_UpdateText);
    cmd->setDefaultText(m_unloadAction->text());
    mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);
    mproject->addAction(cmd, Constants::G_PROJECT_FILES);

    // unload session action
    m_clearSession = new QAction(tr("Unload All Projects"), this);
    cmd = am->registerAction(m_clearSession, Constants::CLEARSESSION, globalcontext);
    mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);

    // session menu
    Core::IActionContainer *msession = am->createMenu(Constants::M_SESSION);
    msession->menu()->setTitle("&Session");
    mfile->addMenu(msession, Core::Constants::G_FILE_PROJECT);
    m_sessionMenu = msession->menu();
    connect(mfile->menu(), SIGNAL(aboutToShow()),
            this, SLOT(updateSessionMenu()));

    // build menu
    Core::IActionContainer *mbc =
        am->createMenu(Constants::BUILDCONFIGURATIONMENU);
    m_buildConfigurationMenu = mbc->menu();
    m_buildConfigurationMenu->setTitle(tr("Set Build Configuration"));
    //TODO this means it is build twice, rrr
    connect(mproject->menu(), SIGNAL(aboutToShow()), this, SLOT(populateBuildConfigurationMenu()));
    connect(mbuild->menu(), SIGNAL(aboutToShow()), this, SLOT(populateBuildConfigurationMenu()));
    connect(m_buildConfigurationMenu, SIGNAL(aboutToShow()), this, SLOT(populateBuildConfigurationMenu()));
    connect(m_buildConfigurationMenu, SIGNAL(triggered(QAction *)), this, SLOT(buildConfigurationMenuTriggered(QAction *)));

    // build session action
    QIcon buildIcon(Constants::ICON_BUILD);
    buildIcon.addFile(Constants::ICON_BUILD_SMALL);
    m_buildSessionAction = new QAction(buildIcon, tr("Build All"), this);
    cmd = am->registerAction(m_buildSessionAction, Constants::BUILDSESSION, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+B")));
    mbuild->addAction(cmd, Constants::G_BUILD_SESSION);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);
    // Add to mode bar
    modeManager->addAction(cmd, Constants::P_ACTION_BUILDSESSION, m_buildConfigurationMenu);


    // rebuild session action
    QIcon rebuildIcon(Constants::ICON_REBUILD);
    rebuildIcon.addFile(Constants::ICON_REBUILD_SMALL);
    m_rebuildSessionAction = new QAction(rebuildIcon, tr("Rebuild All"), this);
    cmd = am->registerAction(m_rebuildSessionAction, Constants::REBUILDSESSION, globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_SESSION);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);

    // clean session
    QIcon cleanIcon(Constants::ICON_CLEAN);
    cleanIcon.addFile(Constants::ICON_CLEAN_SMALL);
    m_cleanSessionAction = new QAction(cleanIcon, tr("Clean All"), this);
    cmd = am->registerAction(m_cleanSessionAction, Constants::CLEANSESSION, globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_SESSION);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);


    // build action
    m_buildAction = new QAction(tr("Build Project"), this);
    cmd = am->registerAction(m_buildAction, Constants::BUILD, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+B")));
    mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);
    mproject->addAction(cmd, Constants::G_PROJECT_BUILD);

    // rebuild action
    m_rebuildAction = new QAction(tr("Rebuild Project"), this);
    cmd = am->registerAction(m_rebuildAction, Constants::REBUILD, globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);
    mproject->addAction(cmd, Constants::G_PROJECT_BUILD);

    // clean action
    m_cleanAction = new QAction(tr("Clean Project"), this);
    cmd = am->registerAction(m_cleanAction, Constants::CLEAN, globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);
    mproject->addAction(cmd, Constants::G_PROJECT_BUILD);

    // Add Set Build Configuration to menu
    mbuild->addMenu(mbc, Constants::G_BUILD_PROJECT);
    mproject->addMenu(mbc, Constants::G_PROJECT_CONFIG);


    // run action
    QIcon runIcon(Constants::ICON_RUN);
    runIcon.addFile(Constants::ICON_RUN_SMALL);
    m_runAction = new QAction(runIcon, tr("Run"), this);
    cmd = am->registerAction(m_runAction, Constants::RUN, globalcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+R")));
    mbuild->addAction(cmd, Constants::G_BUILD_RUN);
    mproject->addAction(cmd, Constants::G_PROJECT_RUN);

    Core::IActionContainer *mrc = am->createMenu(Constants::RUNCONFIGURATIONMENU);
    m_runConfigurationMenu = mrc->menu();
    m_runConfigurationMenu->setTitle(tr("Set Run Configuration"));
    mbuild->addMenu(mrc, Constants::G_BUILD_RUN);
    mproject->addMenu(mrc, Constants::G_PROJECT_CONFIG);
    // TODO this recreates the menu twice if shown in the Build or context menu
    connect(m_runConfigurationMenu, SIGNAL(aboutToShow()), this, SLOT(populateRunConfigurationMenu()));
    connect(mproject->menu(), SIGNAL(aboutToShow()), this, SLOT(populateRunConfigurationMenu()));
    connect(mbuild->menu(), SIGNAL(aboutToShow()), this, SLOT(populateRunConfigurationMenu()));
    connect(m_runConfigurationMenu, SIGNAL(triggered(QAction *)), this, SLOT(runConfigurationMenuTriggered(QAction *)));

    modeManager->addAction(cmd, Constants::P_ACTION_RUN, m_runConfigurationMenu);

    // jump to next task
    m_taskAction = new QAction(tr("Go to Task Window"), this);
    m_taskAction->setIcon(QIcon(Core::Constants::ICON_NEXT));
    cmd = am->registerAction(m_taskAction, Constants::GOTOTASKWINDOW, globalcontext);
    // FIXME: Eike, look here! cmd->setDefaultKeySequence(QKeySequence(tr("F9")));
    mbuild->addAction(cmd, Constants::G_BUILD_TASK);

    // cancel build action
    m_cancelBuildAction = new QAction(tr("Cancel Build"), this);
    cmd = am->registerAction(m_cancelBuildAction, Constants::CANCELBUILD, globalcontext);
    mbuild->addAction(cmd, Constants::G_BUILD_CANCEL);

    // debug action
    QIcon debuggerIcon(":/projectexplorer/images/debugger_start_small.png");
    debuggerIcon.addFile(":/gdbdebugger/images/debugger_start.png");
    m_debugAction = new QAction(debuggerIcon, tr("Start Debugging"), this);
    cmd = am->registerAction(m_debugAction, Constants::DEBUG, globalcontext);
    cmd->setAttribute(Core::ICommand::CA_UpdateText);
    cmd->setAttribute(Core::ICommand::CA_UpdateIcon);
    cmd->setDefaultText(tr("Start Debugging"));
    cmd->setDefaultKeySequence(QKeySequence(tr("F5")));
    mdebug->addAction(cmd, Core::Constants::G_DEFAULT_ONE);
    modeManager->addAction(cmd, Constants::P_ACTION_DEBUG, m_runConfigurationMenu);

    // dependencies action
    m_dependenciesAction = new QAction(tr("Edit Dependencies..."), this);
    cmd = am->registerAction(m_dependenciesAction, Constants::DEPENDENCIES, pecontext);
    msessionContextMenu->addAction(cmd, Constants::G_SESSION_CONFIG);

    // add new file action
    m_addNewFileAction = new QAction(tr("Add New..."), this);
    cmd = am->registerAction(m_addNewFileAction, ProjectExplorer::Constants::ADDNEWFILE,
                       globalcontext);
    mproject->addAction(cmd, Constants::G_PROJECT_FILES);
    msubProject->addAction(cmd, Constants::G_PROJECT_FILES);

    // add existing file action
    m_addExistingFilesAction = new QAction(tr("Add Existing Files..."), this);
    cmd = am->registerAction(m_addExistingFilesAction, ProjectExplorer::Constants::ADDEXISTINGFILES,
                       globalcontext);
    mproject->addAction(cmd, Constants::G_PROJECT_FILES);
    msubProject->addAction(cmd, Constants::G_PROJECT_FILES);

    // remove file action
    m_removeFileAction = new QAction(tr("Remove File..."), this);
    cmd = am->registerAction(m_removeFileAction, ProjectExplorer::Constants::REMOVEFILE,
                       globalcontext);
    mfilec->addAction(cmd, Constants::G_FILE_OTHER);

    // renamefile action (TODO: Not supported yet)
    m_renameFileAction = new QAction(tr("Rename"), this);
    cmd = am->registerAction(m_renameFileAction, ProjectExplorer::Constants::RENAMEFILE,
                       globalcontext);
    mfilec->addAction(cmd, Constants::G_FILE_OTHER);
    m_renameFileAction->setEnabled(false);
    m_renameFileAction->setVisible(false);

    connect(m_core, SIGNAL(saveSettingsRequested()),
        this, SLOT(savePersistentSettings()));

    addAutoReleasedObject(new ProjectTreeWidgetFactory(m_core));
    addAutoReleasedObject(new FolderNavigationWidgetFactory(m_core));

    if (QSettings *s = m_core->settings())
        m_recentProjects = s->value("ProjectExplorer/RecentProjects/Files", QStringList()).toStringList();
    for (QStringList::iterator it = m_recentProjects.begin(); it != m_recentProjects.end(); ) {
        if (QFileInfo(*it).isFile()) {
            ++it;
        } else {
            it = m_recentProjects.erase(it);
        }
    }

    connect(m_sessionManagerAction, SIGNAL(triggered()), this, SLOT(showSessionManager()));
    connect(m_newAction, SIGNAL(triggered()), this, SLOT(newProject()));
#if 0
    connect(m_loadAction, SIGNAL(triggered()), this, SLOT(loadAction()));
#endif
    connect(m_buildAction, SIGNAL(triggered()), this, SLOT(buildProject()));
    connect(m_buildSessionAction, SIGNAL(triggered()), this, SLOT(buildSession()));
    connect(m_rebuildAction, SIGNAL(triggered()), this, SLOT(rebuildProject()));
    connect(m_rebuildSessionAction, SIGNAL(triggered()), this, SLOT(rebuildSession()));
    connect(m_cleanAction, SIGNAL(triggered()), this, SLOT(cleanProject()));
    connect(m_cleanSessionAction, SIGNAL(triggered()), this, SLOT(cleanSession()));
    connect(m_runAction, SIGNAL(triggered()), this, SLOT(runProject()));
    connect(m_cancelBuildAction, SIGNAL(triggered()), this, SLOT(cancelBuild()));
    connect(m_debugAction, SIGNAL(triggered()), this, SLOT(debugProject()));
    connect(m_dependenciesAction, SIGNAL(triggered()), this, SLOT(editDependencies()));
    connect(m_unloadAction, SIGNAL(triggered()), this, SLOT(unloadProject()));
    connect(m_clearSession, SIGNAL(triggered()), this, SLOT(clearSession()));
    connect(m_taskAction, SIGNAL(triggered()), this, SLOT(goToTaskWindow()));
    connect(m_addNewFileAction, SIGNAL(triggered()), this, SLOT(addNewFile()));
    connect(m_addExistingFilesAction, SIGNAL(triggered()), this, SLOT(addExistingFiles()));
    connect(m_openFileAction, SIGNAL(triggered()), this, SLOT(openFile()));
    connect(m_removeFileAction, SIGNAL(triggered()), this, SLOT(removeFile()));
    connect(m_renameFileAction, SIGNAL(triggered()), this, SLOT(renameFile()));

    updateActions();

    connect(m_core, SIGNAL(coreOpened()), this, SLOT(restoreSession()));

    return true;
}

// Find a factory by file mime type in a sequence of factories
template <class Factory, class Iterator>
    Factory *findFactory(const QString &mimeType, Iterator i1, Iterator i2)
{
    for ( ; i1 != i2; ++i2) {
        Factory *f = *i1;
        if (f->mimeTypes().contains(mimeType))
            return f;
    }
    return 0;
}

ProjectFileFactory * ProjectExplorerPlugin::findProjectFileFactory(const QString &filename) const
{
    // Find factory
    if (const Core::MimeType mt = m_core->mimeDatabase()->findByFile(QFileInfo(filename)))
        if (ProjectFileFactory *pf = findFactory<ProjectFileFactory>(mt.type(), m_fileFactories.constBegin(), m_fileFactories.constEnd()))
            return pf;
    qWarning("Unable to find project file factory for '%s'", filename.toUtf8().constData());
    return 0;
}

void ProjectExplorerPlugin::loadAction()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::loadAction";


    QString dir = m_lastOpenDirectory;

    // for your special convenience, we preselect a pro file if it is
    // the current file
    if (Core::IEditor *editor = Core::EditorManager::instance()->currentEditor()) {
        if (const Core::IFile *file = editor->file()) {
            const QString fn = file->fileName();
            const bool isProject = m_profileMimeTypes.contains(file->mimeType());
            dir = isProject ? fn : QFileInfo(fn).absolutePath();
        }
    }

    QString filename = QFileDialog::getOpenFileName(0, tr("Load Project"),
                                                    dir,
                                                    m_projectFilterString);
    if (filename.isEmpty())
        return;
    if (ProjectFileFactory *pf = findProjectFileFactory(filename))
        pf->open(filename);
    updateActions();
}

void ProjectExplorerPlugin::unloadProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::unloadProject";

    Core::IFile *fi = m_currentProject->file();

    if (!fi || fi->fileName().isEmpty()) //nothing to save?
        return;

    QList<Core::IFile*> filesToSave;
    filesToSave << fi;
    filesToSave << m_currentProject->dependencies();

    // check the number of modified files
    int readonlycount = 0;
    foreach (const Core::IFile *file, filesToSave) {
        if (file->isReadOnly())
            ++readonlycount;
    }

    bool success = false;
    if (readonlycount > 0)
        success = m_core->fileManager()->saveModifiedFiles(filesToSave).isEmpty();
    else
        success = m_core->fileManager()->saveModifiedFilesSilently(filesToSave).isEmpty();

    if (!success)
        return;

    addToRecentProjects(fi->fileName());
    m_session->removeProject(m_currentProject);
    updateActions();
}

void ProjectExplorerPlugin::clearSession()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::clearSession";

    if (!m_session->clear())
        return; // Action has been cancelled
    updateActions();
}

void ProjectExplorerPlugin::extensionsInitialized()
{
    m_fileFactories = ProjectFileFactory::createFactories(m_core, &m_projectFilterString);
    foreach (ProjectFileFactory *pf, m_fileFactories) {
        m_profileMimeTypes += pf->mimeTypes();
        addAutoReleasedObject(pf);
    }
}

void ProjectExplorerPlugin::shutdown()
{
    m_session->clear();
//    m_proWindow->saveConfigChanges();
}

void ProjectExplorerPlugin::newProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::newProject";

    QString defaultLocation;
    if (currentProject()) {
        const QFileInfo file(currentProject()->file()->fileName());
        QDir dir = file.dir();
        dir.cdUp();
        defaultLocation = dir.absolutePath();
    }

    m_core->showNewItemDialog(tr("New Project", "Title of dialog"),
                              Core::BaseFileWizard::findWizardsOfKind(Core::IWizard::ProjectWizard),
                              defaultLocation);
    updateActions();
}

void ProjectExplorerPlugin::showSessionManager()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::showSessionManager";

    if (m_session->isDefaultVirgin()) {
        // do not save new virgin default sessions
    } else {
        m_session->save();
    }
    SessionDialog sessionDialog(m_session, m_session->activeSession(), false);
    sessionDialog.exec();

    updateActions();
}

void ProjectExplorerPlugin::setStartupProject(Project *project)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::setStartupProject";

    if (!project)
        project = m_currentProject;
    if (!project)
        return;
    m_session->setStartupProject(project);
    // NPE: Visually mark startup project
    updateActions();
}

void ProjectExplorerPlugin::savePersistentSettings()
{
    if (debug)
        qDebug()<<"ProjectExplorerPlugin::savePersistentSettings()";

    foreach (Project *pro, m_session->projects())
        pro->saveSettings();

    if (m_session->isDefaultVirgin()) {
        // do not save new virgin default sessions
    } else {
        m_session->save();
    }

    QSettings *s = m_core->settings();
    if (s) {
        s->setValue("ProjectExplorer/StartupSession", m_session->file()->fileName());
        s->setValue("ProjectExplorer/RecentProjects/Files", m_recentProjects);
    }
}

bool ProjectExplorerPlugin::openProject(const QString &fileName)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::openProject";

    if (openProjects(QStringList() << fileName)) {
        addToRecentProjects(fileName);
        return true;
    }
    return false;
}

bool ProjectExplorerPlugin::openProjects(const QStringList &fileNames)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin - opening projects " << fileNames;

    QList<IProjectManager*> projectManagers =
        m_core->pluginManager()->getObjects<IProjectManager>();

    //QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   // bool blocked = blockSignals(true);
    QList<Project*> openedPro;
    foreach (QString fileName, fileNames)
        if (const Core::MimeType mt = m_core->mimeDatabase()->findByFile(QFileInfo(fileName))) {
            foreach (IProjectManager *manager, projectManagers)
                if (manager->mimeType() == mt.type()) {
                    if (Project *pro = manager->openProject(fileName))
                        openedPro += pro;
                    m_session->reportProjectLoadingProgress();
                    break;
                }
        }
    //blockSignals(blocked);

    if (openedPro.isEmpty()) {
        if (debug)
            qDebug() << "ProjectExplorerPlugin - Could not open any projects!";
        QApplication::restoreOverrideCursor();
        return false;
    }

    foreach (Project *pro, openedPro) {
        if (debug)
            qDebug()<<"restoring settings for "<<pro->file()->fileName();
        pro->restoreSettings();
        connect(pro, SIGNAL(fileListChanged()), this, SIGNAL(fileListChanged()));
    }
    m_session->addProjects(openedPro);

    // Make sure we always have a current project / node
    if (!m_currentProject)
        setCurrentNode(openedPro.first()->rootProjectNode());

    updateActions();

    m_core->modeManager()->activateMode(Core::Constants::MODE_EDIT);
    QApplication::restoreOverrideCursor();

    return true;
}

Project *ProjectExplorerPlugin::currentProject() const
{
    if (debug) {
        if (m_currentProject)
            qDebug() << "ProjectExplorerPlugin::currentProject returns " << m_currentProject->name();
        else
            qDebug() << "ProjectExplorerPlugin::currentProject returns 0";
    }
    return m_currentProject;
}

Node *ProjectExplorerPlugin::currentNode() const
{
    return m_currentNode;
}

void ProjectExplorerPlugin::setCurrentFile(Project *project, const QString &filePath)
{
    setCurrent(project, filePath, 0);
}

void ProjectExplorerPlugin::setCurrentFile(const QString &filePath)
{
    Project *project = m_session->projectForFile(filePath);
    setCurrent(project, filePath, 0);
}

void ProjectExplorerPlugin::setCurrentNode(Node *node)
{
    setCurrent(m_session->projectForNode(node), QString(), node);
}

SessionManager *ProjectExplorerPlugin::session() const
{
    return m_session;
}

Project *ProjectExplorerPlugin::startupProject() const
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::startupProject";

    Project *pro = m_session->startupProject();

    if (!pro)
        pro = m_currentProject;

    return pro;
}

// update welcome page
void ProjectExplorerPlugin::updateWelcomePage(Core::Internal::WelcomeMode *welcomeMode)
{
    Core::Internal::WelcomeMode::WelcomePageData welcomePageData;
    welcomePageData.sessionList =  m_session->sessions();
    welcomePageData.activeSession = m_session->activeSession();
    welcomePageData.previousSession = m_session->lastSession();
    welcomePageData.projectList = m_recentProjects;
    welcomeMode->updateWelcomePage(welcomePageData);
}

void ProjectExplorerPlugin::currentModeChanged(Core::IMode *mode)
{
    if (Core::Internal::WelcomeMode *welcomeMode = qobject_cast<Core::Internal::WelcomeMode*>(mode))
        updateWelcomePage(welcomeMode);
}

/*!
    \fn void ProjectExplorerPlugin::restoreSession()

    This method is connected to the ICore::coreOpened signal.  If
    there was no session explicitly loaded, it creates an empty new
    default session and puts the list of recent projects and sessions
    onto the welcome page.
*/

void ProjectExplorerPlugin::restoreSession()
{

    if (debug)
        qDebug() << "ProjectExplorerPlugin::restoreSession";

    QStringList sessions = m_session->sessions();

    // We have command line arguments, try to find a session in them
    QStringList arguments = ExtensionSystem::PluginManager::instance()->arguments();

    // Default to no session loading
    QString sessionToLoad = QString::null;
    if (!arguments.isEmpty()) {
        foreach (const QString &arg, arguments) {
            if (sessions.contains(arg)) {
                // Session argument
                sessionToLoad = arg;
                arguments.removeOne(arg);
                if (debug)
                    qDebug()<< "Found session argument, loading session"<<sessionToLoad;
                break;
            }
        }
    }

    // Restore latest session or what was passed on the command line
    if (sessionToLoad == QString::null) {
        m_session->createAndLoadNewDefaultSession();
    } else {
        m_session->loadSession(sessionToLoad);
    }

    // update welcome page
    Core::ModeManager *modeManager = m_core->modeManager();
    connect(modeManager, SIGNAL(currentModeChanged(Core::IMode*)), this, SLOT(currentModeChanged(Core::IMode*)));
    if (Core::Internal::WelcomeMode *welcomeMode = qobject_cast<Core::Internal::WelcomeMode*>(modeManager->mode(Core::Constants::MODE_WELCOME))) {
        updateWelcomePage(welcomeMode);
        connect(welcomeMode, SIGNAL(requestSession(QString)), this, SLOT(loadSession(QString)));
        connect(welcomeMode, SIGNAL(requestProject(QString)), this, SLOT(loadProject(QString)));
    }

    m_core->openFiles(arguments);
    updateActions();

}

void ProjectExplorerPlugin::loadSession(const QString &session)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::loadSession" << session;
    m_session->loadSession(session);
}


void ProjectExplorerPlugin::showContextMenu(const QPoint &globalPos, Node *node)
{
    QMenu *contextMenu = 0;

    updateContextMenuActions();

    if (!node)
        node = m_session->sessionNode();

    if (node->nodeType() != SessionNodeType) {
        Project *project = m_session->projectForNode(node);
        setCurrentNode(node);

        emit aboutToShowContextMenu(project, node);
        switch (node->nodeType()) {
        case ProjectNodeType:
            if (node->parentFolderNode() == m_session->sessionNode())
                contextMenu = m_projectMenu;
            else
                contextMenu = m_subProjectMenu;
            break;
        case FolderNodeType:
            contextMenu = m_folderMenu;
            break;
        case FileNodeType:
            contextMenu = m_fileMenu;
            break;
        default:
            qWarning("ProjectExplorerPlugin::showContextMenu - Missing handler for node type");
        }
    } else { // session item
        emit aboutToShowContextMenu(0, node);

        contextMenu = m_sessionContextMenu;
    }

    if (contextMenu && contextMenu->actions().count() > 0) {
        contextMenu->popup(globalPos);
    }
}

BuildManager *ProjectExplorerPlugin::buildManager() const
{
    return m_buildManager;
}

void ProjectExplorerPlugin::buildStateChanged(Project * pro)
{
    if (debug) {
        qDebug() << "buildStateChanged";
        qDebug() << pro->file()->fileName() << "isBuilding()" << m_buildManager->isBuilding(pro);
    }
    Q_UNUSED(pro);
    updateActions();
}

void ProjectExplorerPlugin::buildQueueFinished(bool success)
{
    if (debug)
        qDebug() << "buildQueueFinished()" << success;

    updateActions();

    if (success && m_delayedRunConfiguration) {
        IRunConfigurationRunner *runner = findRunner(m_delayedRunConfiguration, m_runMode);
        if (runner) {
            emit aboutToExecuteProject(m_delayedRunConfiguration->project());

            RunControl *control = runner->run(m_delayedRunConfiguration, m_runMode);
            m_outputPane->createNewOutputWindow(control);
            m_outputPane->popup(false);
            m_outputPane->showTabFor(control);

            connect(control, SIGNAL(addToOutputWindow(RunControl *, const QString &)),
                    this, SLOT(addToApplicationOutputWindow(RunControl *, const QString &)));
            connect(control, SIGNAL(error(RunControl *, const QString &)),
                    this, SLOT(addErrorToApplicationOutputWindow(RunControl *, const QString &)));
            connect(control, SIGNAL(finished()),
                    this, SLOT(runControlFinished()));

            if (m_runMode == ProjectExplorer::Constants::DEBUGMODE)
                m_debuggingRunControl = control;

            control->start();
            updateRunAction();
        }
    } else {
        if (m_buildManager->tasksAvailable())
            m_buildManager->showTaskWindow();
    }

    m_delayedRunConfiguration = QSharedPointer<RunConfiguration>(0);
    m_runMode = QString::null;
}

void ProjectExplorerPlugin::updateTaskActions()
{
    m_taskAction->setEnabled(m_buildManager->tasksAvailable());
}

void ProjectExplorerPlugin::setCurrent(Project *project, QString filePath, Node *node)
{
    if (debug)
        qDebug() << "ProjectExplorer - setting path to " << (node ? node->path() : filePath)
                << " and project to " << (project ? project->name() : "0");

    if (node)
        filePath = node->path();
    else
        node = m_session->nodeForFile(filePath);

    bool projectChanged = false;
    if (m_currentProject != project) {
        int oldContext = -1;
        int newContext = -1;
        int oldLanguageID = -1;
        int newLanguageID = -1;
        if (m_currentProject) {
            oldContext = m_currentProject->projectManager()->projectContext();
            oldLanguageID = m_currentProject->projectManager()->projectLanguage();
        }
        if (project) {
            newContext = project->projectManager()->projectContext();
            newLanguageID = project->projectManager()->projectLanguage();
        }
        m_core->removeAdditionalContext(oldContext);
        m_core->removeAdditionalContext(oldLanguageID);
        m_core->addAdditionalContext(newContext);
        m_core->addAdditionalContext(newLanguageID);
        m_core->updateContext();

        m_currentProject = project;

        projectChanged = true;
    }

    if (projectChanged || m_currentNode != node) {
        m_currentNode = node;
        if (debug)
            qDebug() << "ProjectExplorer - currentNodeChanged(" << node->path() << ", " << (project ? project->name() : "0") << ")";
        emit currentNodeChanged(m_currentNode, project);
    }
    if (projectChanged) {
        if (debug)
            qDebug() << "ProjectExplorer - currentProjectChanged(" << (project ? project->name() : "0") << ")";
        // Enable the right VCS
        if (const Core::IFile *projectFile = project ? project->file() : static_cast<const Core::IFile *>(0)) {
            m_core->vcsManager()->setVCSEnabled(QFileInfo(projectFile->fileName()).absolutePath());
        } else {
            m_core->vcsManager()->setAllVCSEnabled();
        }

        emit currentProjectChanged(project);
        updateActions();
    }

    m_core->fileManager()->setCurrentFile(filePath);
}

void ProjectExplorerPlugin::updateActions()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::updateActions";

    bool enableBuildActions = m_currentProject && ! (m_buildManager->isBuilding(m_currentProject));
    bool hasProjects = !m_session->projects().isEmpty();
    bool building = m_buildManager->isBuilding();

    if (debug)
        qDebug()<<"BuildManager::isBuilding()"<<building;

    m_unloadAction->setEnabled(m_currentProject != 0);
    if (m_currentProject == 0) {
        m_unloadAction->setText(tr("Unload Project"));
        m_buildAction->setText(tr("Build Project"));
    } else {
        m_unloadAction->setText(tr("Unload Project \"%1\"").arg(m_currentProject->name()));
        m_buildAction->setText(tr("Build Project \"%1\"").arg(m_currentProject->name()));
    }

    m_buildAction->setEnabled(enableBuildActions);
    m_rebuildAction->setEnabled(enableBuildActions);
    m_cleanAction->setEnabled(enableBuildActions);
    m_clearSession->setEnabled(hasProjects && !building);
    m_buildSessionAction->setEnabled(hasProjects && !building);
    m_rebuildSessionAction->setEnabled(hasProjects && !building);
    m_cleanSessionAction->setEnabled(hasProjects && !building);
    m_cancelBuildAction->setEnabled(building);
    m_dependenciesAction->setEnabled(hasProjects && !building);

    updateRunAction();

    updateTaskActions();
}

// NBS TODO check projectOrder()
// what we want here is all the projects pro depends on
QStringList ProjectExplorerPlugin::allFilesWithDependencies(Project *pro)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::allFilesWithDependencies(" << pro->file()->fileName() << ")";

    QStringList filesToSave;
    foreach (Project *p, m_session->projectOrder(pro)) {
        FindAllFilesVisitor filesVisitor;
        p->rootProjectNode()->accept(&filesVisitor);
        filesToSave << filesVisitor.filePaths();
    }
    qSort(filesToSave);
    return filesToSave;
}

bool ProjectExplorerPlugin::saveModifiedFiles(const QList<Project *> & projects)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::saveModifiedFiles";

    QList<Core::IFile *> modifiedFi = m_core->fileManager()->modifiedFiles();
    QMap<QString, Core::IFile *> modified;

    QStringList allFiles;
    foreach (Project *pro, projects)
        allFiles << allFilesWithDependencies(pro);

    foreach (Core::IFile * fi, modifiedFi)
        modified.insert(fi->fileName(), fi);

    QList<Core::IFile *> filesToSave;

    QMap<QString, Core::IFile *>::const_iterator mit = modified.constBegin();
    QStringList::const_iterator ait = allFiles.constBegin();
    QMap<QString, Core::IFile *>::const_iterator mend = modified.constEnd();
    QStringList::const_iterator aend = allFiles.constEnd();

    while (mit != mend && ait != aend) {
        if (mit.key() < *ait)
            ++mit;
        else if (*ait < mit.key())
            ++ait;
        else {
            filesToSave.append(mit.value());
            ++ait;
            ++mit;
        }
    }

    if (!filesToSave.isEmpty()) {
        bool cancelled;
        m_core->fileManager()->saveModifiedFiles(filesToSave, &cancelled,
            tr("The following dependencies are modified, do you want to save them?"));
        if (cancelled) {
            return false;
        }
    }
    return true;
}

//NBS handle case where there is no activeBuildConfiguration
// because someone delete all build configurations

void ProjectExplorerPlugin::buildProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::buildProject";

    if (saveModifiedFiles(QList<Project *>() << m_currentProject))
        buildManager()->buildProject(m_currentProject, m_currentProject->activeBuildConfiguration());
}

void ProjectExplorerPlugin::buildSession()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::buildSession";

    const QList<Project *> & projects = m_session->projectOrder();
    if (saveModifiedFiles(projects)) {
        QStringList configurations;
        foreach (const Project * pro, projects)
            configurations << pro->activeBuildConfiguration();

        m_buildManager->buildProjects(projects, configurations);
    }
}

void ProjectExplorerPlugin::rebuildProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::rebuildProject";

    if (saveModifiedFiles(QList<Project *>() << m_currentProject)) {
        m_buildManager->cleanProject(m_currentProject, m_currentProject->activeBuildConfiguration());
        m_buildManager->buildProject(m_currentProject, m_currentProject->activeBuildConfiguration());
    }
}

void ProjectExplorerPlugin::rebuildSession()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::rebuildSession";

    const QList<Project *> & projects = m_session->projectOrder();
    if (saveModifiedFiles(projects)) {
        QStringList configurations;
        foreach (const Project * pro, projects)
            configurations << pro->activeBuildConfiguration();

        m_buildManager->cleanProjects(projects, configurations);
        m_buildManager->buildProjects(projects, configurations);
    }
}

void ProjectExplorerPlugin::cleanProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::cleanProject";

    if (saveModifiedFiles(QList<Project *>() << m_currentProject))
        m_buildManager->cleanProject(m_currentProject, m_currentProject->activeBuildConfiguration());
}

void ProjectExplorerPlugin::cleanSession()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::cleanSession";

    const QList<Project *> & projects = m_session->projectOrder();
    if (saveModifiedFiles(projects)) {
        QStringList configurations;
        foreach (const Project * pro, projects)
            configurations << pro->activeBuildConfiguration();

        m_buildManager->cleanProjects(projects, configurations);
    }
}

void ProjectExplorerPlugin::runProject()
{
    Project *pro = startupProject();
    if (!pro)
        return;

    if (saveModifiedFiles(QList<Project *>() << pro)) {
        m_runMode = ProjectExplorer::Constants::RUNMODE;

        m_delayedRunConfiguration = pro->activeRunConfiguration();
        //NBS TODO make the build project step take into account project dependencies
        m_buildManager->buildProject(pro, pro->activeBuildConfiguration());
    }
}

void ProjectExplorerPlugin::debugProject()
{
    Project *pro = startupProject();
    if (!pro || m_debuggingRunControl )
        return;

    if (saveModifiedFiles(QList<Project *>() << pro)) {
        m_runMode = ProjectExplorer::Constants::DEBUGMODE;
        m_delayedRunConfiguration = pro->activeRunConfiguration();
        //NBS TODO make the build project step take into account project dependencies
        m_buildManager->buildProject(pro, pro->activeBuildConfiguration());
        updateRunAction();
    }
}

void ProjectExplorerPlugin::addToApplicationOutputWindow(RunControl *rc, const QString &line)
{
    m_outputPane->appendOutput(rc, line);
}

void ProjectExplorerPlugin::addErrorToApplicationOutputWindow(RunControl *rc, const QString &error)
{
    m_outputPane->appendOutput(rc, error);
}

void ProjectExplorerPlugin::runControlFinished()
{
    if (sender() == m_debuggingRunControl)
        m_debuggingRunControl = 0;

    updateRunAction();
}

void ProjectExplorerPlugin::startupProjectChanged()
{
    static QPointer<Project> previousStartupProject = 0;
    Project *project = startupProject();
    if (project == previousStartupProject)
        return;

    if (previousStartupProject) {
        disconnect(previousStartupProject, SIGNAL(activeRunConfigurationChanged()),
                   this, SLOT(updateRunAction()));
    }

    previousStartupProject = project;

    if (project) {
        connect(project, SIGNAL(activeRunConfigurationChanged()),
                this, SLOT(updateRunAction()));
    }

    updateRunAction();
}

// NBS TODO implement more than one runner
IRunConfigurationRunner *ProjectExplorerPlugin::findRunner(QSharedPointer<RunConfiguration> config, const QString &mode)
{
    const QList<IRunConfigurationRunner *> runners = m_core->pluginManager()->getObjects<IRunConfigurationRunner>();
    foreach (IRunConfigurationRunner *runner, runners)
        if (runner->canRun(config, mode))
            return runner;
    return 0;
}

void ProjectExplorerPlugin::updateRunAction()
{
    const Project *project = startupProject();
    const bool canRun = project && findRunner(project->activeRunConfiguration(), ProjectExplorer::Constants::RUNMODE);
    const bool canDebug = project && !m_debuggingRunControl && findRunner(project->activeRunConfiguration(), ProjectExplorer::Constants::DEBUGMODE);
    const bool building = m_buildManager->isBuilding();
    m_runAction->setEnabled(canRun && !building);
    m_debugAction->setEnabled(canDebug && !building);
}

void ProjectExplorerPlugin::cancelBuild()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::cancelBuild";

    if (m_buildManager->isBuilding())
        m_buildManager->cancel();
}

void ProjectExplorerPlugin::editDependencies()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::editDependencies";

    m_session->editDependencies();
}

void ProjectExplorerPlugin::addToRecentProjects(const QString &fileName)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::addToRecentProjects(" << fileName << ")";

    if (fileName.isEmpty())
        return;
    QString prettyFileName(QDir::toNativeSeparators(fileName));
    m_recentProjects.removeAll(prettyFileName);
    if (m_recentProjects.count() > m_maxRecentProjects)
        m_recentProjects.removeLast();
    m_recentProjects.prepend(prettyFileName);
    QFileInfo fi(prettyFileName);
    m_lastOpenDirectory = fi.absolutePath();
}

void ProjectExplorerPlugin::updateRecentProjectMenu()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::updateRecentProjectMenu";

    Core::IActionContainer *aci =
        m_core->actionManager()->actionContainer(Constants::M_RECENTPROJECTS);
    QMenu *menu = aci->menu();
    menu->clear();
    m_recentProjectsActions.clear();

    menu->setEnabled(!m_recentProjects.isEmpty());

    //projects (ignore sessions, they used to be in this list)
    foreach (const QString &s, m_recentProjects) {
        if (s.endsWith(".qws"))
            continue;
        QAction *action = menu->addAction(s);
        m_recentProjectsActions.insert(action, s);
        connect(action, SIGNAL(triggered()), this, SLOT(openRecentProject()));
    }
}

void ProjectExplorerPlugin::openRecentProject()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::openRecentProject()";

    QAction *a = qobject_cast<QAction*>(sender());
    if (m_recentProjectsActions.contains(a)) {
        const QString fileName = m_recentProjectsActions.value(a);
        openProject(fileName);
    }
}

void ProjectExplorerPlugin::invalidateProject(Project *project)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::invalidateProject" << project->name();
    if (m_currentProject == project) {
        //
        // Workaround for a bug in QItemSelectionModel
        // - currentChanged etc are not emitted if the
        // item is removed from the underlying data model
        //
        setCurrent(0, QString(), 0);
    }

    disconnect(project, SIGNAL(fileListChanged()), this, SIGNAL(fileListChanged()));
}

void ProjectExplorerPlugin::goToTaskWindow()
{
    m_buildManager->gotoTaskWindow();
}

void ProjectExplorerPlugin::updateContextMenuActions()
{
    if (ProjectNode *projectNode = qobject_cast<ProjectNode*>(m_currentNode)) {
        const bool addFilesEnabled = projectNode->supportedActions().contains(ProjectNode::AddFile);
        m_addExistingFilesAction->setEnabled(addFilesEnabled);
        m_addNewFileAction->setEnabled(addFilesEnabled);
    } else if (FileNode *fileNode = qobject_cast<FileNode*>(m_currentNode)) {
        const bool removeFileEnabled = fileNode->projectNode()->supportedActions().contains(ProjectNode::RemoveFile);
        m_removeFileAction->setEnabled(removeFileEnabled);
    }
}

void ProjectExplorerPlugin::addNewFile()
{
    if (!m_currentNode && m_currentNode->nodeType() == ProjectNodeType)
        return;
    const QString location = QFileInfo(m_currentNode->path()).dir().absolutePath();
    m_core->showNewItemDialog(tr("New File", "Title of dialog"),
                              Core::BaseFileWizard::findWizardsOfKind(Core::IWizard::FileWizard)
                              + Core::BaseFileWizard::findWizardsOfKind(Core::IWizard::ClassWizard),
                              location);
}

void ProjectExplorerPlugin::addExistingFiles()
{
    if (!m_currentNode && m_currentNode->nodeType() == ProjectNodeType)
        return;
    ProjectNode *projectNode = qobject_cast<ProjectNode*>(m_currentNode);
    const QString dir = QFileInfo(m_currentNode->path()).dir().absolutePath();
    QStringList fileNames = QFileDialog::getOpenFileNames(m_core->mainWindow(), tr("Add Existing Files"), dir);
    if (fileNames.isEmpty())
        return;

    QHash<FileType, QString> fileTypeToFiles;
    foreach (const QString &fileName, fileNames) {
        FileType fileType = typeForFileName(m_core->mimeDatabase(), QFileInfo(fileName));
        fileTypeToFiles.insertMulti(fileType, fileName);
    }

    QStringList notAdded;
    foreach (const FileType type, fileTypeToFiles.uniqueKeys()) {
        projectNode->addFiles(type, fileTypeToFiles.values(type), &notAdded);
    }
    if (!notAdded.isEmpty()) {
        QString message = tr("Could not add following files to project %1:\n").arg(projectNode->name());
        QString files = notAdded.join("\n");
        QMessageBox::warning(m_core->mainWindow(), tr("Add files to project failed"),
                             message + files);
        foreach (const QString &file, notAdded)
            fileNames.removeOne(file);
    }

    if (Core::IVersionControl *vcManager = m_core->vcsManager()->findVersionControlForDirectory(dir))
        if (vcManager->supportsOperation(Core::IVersionControl::AddOperation)) {
            const QString files = fileNames.join(QString(QLatin1Char('\n')));
            QMessageBox::StandardButton button =
                QMessageBox::question(m_core->mainWindow(), tr("Add to Version Control"),
                                      tr("Add files\n%1\nto version control (%2)?").arg(files, vcManager->name()),
                                      QMessageBox::Yes | QMessageBox::No);
            if (button == QMessageBox::Yes) {
                QStringList notAddedToVc;
                foreach (const QString &file, fileNames) {
                    if (!vcManager->vcsAdd(file))
                        notAddedToVc << file;
                }

                if (!notAddedToVc.isEmpty()) {
                    const QString message = tr("Could not add following files to version control (%1)\n").arg(vcManager->name());
                    const QString filesNotAdded = notAddedToVc.join(QString(QLatin1Char('\n')));
                    QMessageBox::warning(m_core->mainWindow(), tr("Add files to version control failed"),
                                         message + filesNotAdded);
                }
            }
        }
}

void ProjectExplorerPlugin::openFile()
{
    if (m_currentNode)
        return;
    m_core->editorManager()->openEditor(m_currentNode->path());
    m_core->editorManager()->ensureEditorManagerVisible();
}

void ProjectExplorerPlugin::removeFile()
{
    if (!m_currentNode && m_currentNode->nodeType() == FileNodeType)
        return;
    FileNode *fileNode = qobject_cast<FileNode*>(m_currentNode);

    const QString filePath = m_currentNode->path();
    const QString fileDir = QFileInfo(filePath).dir().absolutePath();
    RemoveFileDialog removeFileDialog(filePath, m_core->mainWindow());

    if (removeFileDialog.exec() == QDialog::Accepted) {
        const bool deleteFile = removeFileDialog.isDeleteFileChecked();

        // remove from project
        ProjectNode *projectNode = fileNode->projectNode();
        Q_ASSERT(projectNode);

        if (!projectNode->removeFiles(fileNode->fileType(), QStringList(filePath))) {
            QMessageBox::warning(m_core->mainWindow(), tr("Remove file failed"),
                                 tr("Could not remove file %1 from project %2.").arg(filePath).arg(projectNode->name()));
            return;
        }

        // remove from version control
        m_core->vcsManager()->showDeleteDialog(filePath);

        // remove from file system
        if (deleteFile) {
            QFile file(filePath);

            if (file.exists()) {
                // could have been deleted by vc
                if (!file.remove())
                    QMessageBox::warning(m_core->mainWindow(), tr("Delete file failed"),
                                         tr("Could not delete file %1.").arg(filePath));
            }
        }
    }
}

void ProjectExplorerPlugin::renameFile()
{
    QWidget *focusWidget = QApplication::focusWidget();
    while (focusWidget) {
        ProjectTreeWidget *treeWidget = qobject_cast<ProjectTreeWidget*>(focusWidget);
        if (treeWidget) {
            treeWidget->editCurrentItem();
            break;
        }
        focusWidget = focusWidget->parentWidget();
    }
}

void ProjectExplorerPlugin::populateBuildConfigurationMenu()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::populateBuildConfigurationMenu";

    // delete the old actiongroup and all actions that are children of it
    delete m_buildConfigurationActionGroup;
    m_buildConfigurationActionGroup = new QActionGroup(m_buildConfigurationMenu);
    m_buildConfigurationMenu->clear();
    if (Project *pro = m_currentProject) {
        const QString &activeBuildConfiguration = pro->activeBuildConfiguration();
        foreach (const QString &buildConfiguration, pro->buildConfigurations()) {
            QString displayName = pro->displayNameFor(buildConfiguration);
            QAction *act = new QAction(displayName, m_buildConfigurationActionGroup);
            if (debug)
                qDebug() << "BuildConfiguration " << buildConfiguration << "active: " << activeBuildConfiguration;
            act->setCheckable(true);
            act->setChecked(buildConfiguration == activeBuildConfiguration);
            act->setData(buildConfiguration);
            m_buildConfigurationMenu->addAction(act);
        }
        m_buildConfigurationMenu->setEnabled(true);
    } else {
        m_buildConfigurationMenu->setEnabled(false);
    }
}

void ProjectExplorerPlugin::buildConfigurationMenuTriggered(QAction *action)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::buildConfigurationMenuTriggered";

    m_currentProject->setActiveBuildConfiguration(action->data().toString());
}

void ProjectExplorerPlugin::populateRunConfigurationMenu()
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::populateRunConfigurationMenu";

    delete m_runConfigurationActionGroup;
    m_runConfigurationActionGroup = new QActionGroup(m_runConfigurationMenu);
    m_runConfigurationMenu->clear();

    const Project *startupProject = m_session->startupProject();
    QSharedPointer<RunConfiguration> activeRunConfiguration
            = (startupProject) ? startupProject->activeRunConfiguration() : QSharedPointer<RunConfiguration>(0);

    foreach (const Project *pro, m_session->projects()) {
        foreach (QSharedPointer<RunConfiguration> runConfiguration, pro->runConfigurations()) {
            const QString title = QString("%1 (%2)").arg(pro->name(), runConfiguration->name());
            QAction *act = new QAction(title, m_runConfigurationActionGroup);
            act->setCheckable(true);
            act->setData(qVariantFromValue(runConfiguration));
            act->setChecked(runConfiguration == activeRunConfiguration);
            m_runConfigurationMenu->addAction(act);
            if (debug)
                qDebug() << "RunConfiguration" << runConfiguration << "project:" << pro->name()
                         << "active:" << (runConfiguration == activeRunConfiguration);
        }
    }

    m_runConfigurationMenu->setDisabled(m_runConfigurationMenu->actions().isEmpty());
}

void ProjectExplorerPlugin::runConfigurationMenuTriggered(QAction *action)
{
    if (debug)
        qDebug() << "ProjectExplorerPlugin::runConfigurationMenuTriggered" << action;

        QSharedPointer<RunConfiguration> runConfiguration = qVariantValue<QSharedPointer<RunConfiguration> >(action->data());
        runConfiguration->project()->setActiveRunConfiguration(runConfiguration);
        setStartupProject(runConfiguration->project());
}

void ProjectExplorerPlugin::populateOpenWithMenu()
{
    typedef QList<Core::IEditorFactory*> EditorFactoryList;

    m_openWithMenu->clear();

    bool anyMatches = false;
    const QString fileName = currentNode()->path();

    if (const Core::MimeType mt = m_core->mimeDatabase()->findByFile(QFileInfo(fileName))) {
        const EditorFactoryList factories = m_core->editorManager()->editorFactories(mt, false);
        anyMatches = !factories.empty();
        if (anyMatches) {
            const QList<Core::IEditor *> editorsOpenForFile = m_core->editorManager()->editorsForFileName(fileName);
            // Add all suitable editors
            foreach (Core::IEditorFactory *editorFactory, factories) {
                // Add action to open with this very editor factory
                QString const actionTitle(editorFactory->kind());
                QAction * const action = m_openWithMenu->addAction(actionTitle);
                action->setData(qVariantFromValue(editorFactory));
                // File already open in an editor -> only enable that entry since
                // we currently do not support opening a file in two editors at once
                if (!editorsOpenForFile.isEmpty()) {
                    bool enabled = false;
                    foreach (Core::IEditor * const openEditor, editorsOpenForFile) {
                        if (editorFactory->kind() == QLatin1String(openEditor->kind()))
                            enabled = true;
                        break;
                    }
                    action->setEnabled(enabled);
                }
            }
        }
    }
    m_openWithMenu->setEnabled(anyMatches);
}

void ProjectExplorerPlugin::openWithMenuTriggered(QAction *action)
{
    if (!action) {
        qWarning() << "ProjectExplorerPlugin::openWithMenuTriggered no action, can't happen.";
        return;
    }
    Core::IEditorFactory * const editorFactory = qVariantValue<Core::IEditorFactory *>(action->data());
    if (!editorFactory) {
        qWarning() << "Editor Factory not attached to action, can't happen"<<editorFactory;
        return;
    }
    m_core->editorManager()->openEditor(currentNode()->path(), editorFactory->kind());
    m_core->editorManager()->ensureEditorManagerVisible();
}

void ProjectExplorerPlugin::updateSessionMenu()
{
    m_sessionMenu->clear();
    QActionGroup *ag = new QActionGroup(m_sessionMenu);
    connect(ag, SIGNAL(triggered(QAction *)), this, SLOT(setSession(QAction *)));
    const QString &activeSession = m_session->activeSession();
    foreach (const QString &session, m_session->sessions()) {
        QAction *act = ag->addAction(session);
        act->setCheckable(true);
        if (session == activeSession)
            act->setChecked(true);
    }
    m_sessionMenu->addActions(ag->actions());
    m_sessionMenu->addSeparator();
    m_sessionMenu->addAction(m_sessionManagerAction);

    m_sessionMenu->setEnabled(true);
}

void ProjectExplorerPlugin::setSession(QAction *action)
{
    QString session = action->text();
    if (session != m_session->activeSession())
        m_session->loadSession(session);
}

Q_EXPORT_PLUGIN(ProjectExplorerPlugin)
