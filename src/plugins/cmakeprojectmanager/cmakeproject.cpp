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

#include "cmakeproject.h"

#include "cmakeprojectconstants.h"
#include "cmakeprojectnodes.h"
#include "cmakerunconfiguration.h"
#include "cmakestep.h"
#include "makestep.h"

#include <extensionsystem/pluginmanager.h>
#include <cpptools/cppmodelmanagerinterface.h>
#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QProcess>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;


// QtCreator CMake Generator wishlist:
// Which make targets we need to build to get all executables
// What is the make we need to call
// What is the actual compiler executable
// DEFINES

// Open Questions
// Who sets up the environment for cl.exe ? INCLUDEPATH and so on


CMakeProject::CMakeProject(CMakeManager *manager, const QString &fileName)
    : m_manager(manager), m_fileName(fileName), m_rootNode(new CMakeProjectNode(m_fileName))
{

    m_file = new CMakeFile(this, fileName);
    QDir dir = QFileInfo(m_fileName).absoluteDir();
    QString cbpFile = findCbpFile(dir);
    if (cbpFile.isEmpty()) {
        createCbpFile(dir);
        cbpFile = findCbpFile(dir);
    }

    //TODO move this parsing to a seperate method, which is also called if the CMakeList.txt is updated
    CMakeCbpParser cbpparser;
    if (cbpparser.parseCbpFile(cbpFile)) {
        // TODO do a intelligent updating of the tree
        buildTree(m_rootNode, cbpparser.fileList());
        foreach (ProjectExplorer::FileNode *fn, cbpparser.fileList())
            m_files.append(fn->path());
        m_files.sort();

        m_targets = cbpparser.targets();
        qDebug()<<"Printing targets";
        foreach(CMakeTarget ct, m_targets) {
            qDebug()<<ct.title<<" with executable:"<<ct.executable;
            qDebug()<<"WD:"<<ct.workingDirectory;
            qDebug()<<ct.makeCommand<<ct.makeCleanCommand;
            qDebug()<<"";
        }

        CppTools::CppModelManagerInterface *modelmanager = ExtensionSystem::PluginManager::instance()->getObject<CppTools::CppModelManagerInterface>();
        if (modelmanager) {
            CppTools::CppModelManagerInterface::ProjectInfo pinfo = modelmanager->projectInfo(this);
            pinfo.includePaths = cbpparser.includeFiles();
            // TODO we only want C++ files, not all other stuff that might be in the project
            pinfo.sourceFiles = m_files;
            // TODO defines
            // TODO gcc preprocessor files
            modelmanager->updateProjectInfo(pinfo);
        }
    } else {
        // TODO report error
    }
}

CMakeProject::~CMakeProject()
{
    delete m_rootNode;
}

QString CMakeProject::findCbpFile(const QDir &directory)
{
    // Find the cbp file
    //   TODO the cbp file is named like the project() command in the CMakeList.txt file
    //   so this method below could find the wrong cbp file, if the user changes the project()
    //   name
    foreach (const QString &cbpFile , directory.entryList()) {
        if (cbpFile.endsWith(".cbp"))
            return directory.path() + "/" + cbpFile;
    }
    return QString::null;
}

void CMakeProject::createCbpFile(const QDir &directory)
{
    // We create a cbp file, only if we didn't find a cbp file in the base directory
    // Yet that can still override cbp files in subdirectories
    // And we are creating tons of files in the source directories
    // All of that is not really nice.
    // The mid term plan is to move away from the CodeBlocks Generator and use our own
    // QtCreator generator, which actually can be very similar to the CodeBlock Generator

    // TODO we need to pass on the same paremeters as the cmakestep
    QProcess cmake;
    cmake.setWorkingDirectory(directory.absolutePath());
    cmake.start("cmake", QStringList() << "-GCodeBlocks - Unix Makefiles");
    cmake.waitForFinished();
}

void CMakeProject::buildTree(CMakeProjectNode *rootNode, QList<ProjectExplorer::FileNode *> list)
{
    //m_rootNode->addFileNodes(fileList, m_rootNode);
    qSort(list.begin(), list.end(), ProjectExplorer::ProjectNode::sortNodesByPath);
    foreach (ProjectExplorer::FileNode *fn, list) {
        // Get relative path to rootNode
        QString parentDir = QFileInfo(fn->path()).absolutePath();
        ProjectExplorer::FolderNode *folder = findOrCreateFolder(rootNode, parentDir);
        rootNode->addFileNodes(QList<ProjectExplorer::FileNode *>()<< fn, folder);
    }
    //m_rootNode->addFileNodes(list, rootNode);
}

ProjectExplorer::FolderNode *CMakeProject::findOrCreateFolder(CMakeProjectNode *rootNode, QString directory)
{
    QString relativePath = QDir(QFileInfo(rootNode->path()).path()).relativeFilePath(directory);
    QStringList parts = relativePath.split("/");
    ProjectExplorer::FolderNode *parent = rootNode;
    foreach (const QString &part, parts) {
        // Find folder in subFolders
        bool found = false;
        foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes()) {
            if (QFileInfo(folder->path()).fileName() == part) {
                // yeah found something :)
                parent = folder;
                found = true;
                break;
            }
        }
        if (!found) {
            // No FolderNode yet, so create it
            ProjectExplorer::FolderNode *tmp = new ProjectExplorer::FolderNode(part);
            rootNode->addFolderNodes(QList<ProjectExplorer::FolderNode *>() << tmp, parent);
            parent = tmp;
        }
    }
    return parent;
}

QString CMakeProject::name() const
{
    // TODO
    return "";
}

Core::IFile *CMakeProject::file() const
{
    return m_file;
}

ProjectExplorer::IProjectManager *CMakeProject::projectManager() const
{
    return m_manager;
}

QList<Core::IFile *> CMakeProject::dependencies()
{
    return QList<Core::IFile *>();
}

QList<ProjectExplorer::Project *> CMakeProject::dependsOn()
{
    return QList<Project *>();
}

bool CMakeProject::isApplication() const
{
    return true;
}

ProjectExplorer::Environment CMakeProject::environment(const QString &buildConfiguration) const
{
    Q_UNUSED(buildConfiguration)
    //TODO
    return ProjectExplorer::Environment::systemEnvironment();
}

QString CMakeProject::buildDirectory(const QString &buildConfiguration) const
{
    Q_UNUSED(buildConfiguration)
    //TODO
    return QFileInfo(m_fileName).absolutePath();
}

ProjectExplorer::BuildStepConfigWidget *CMakeProject::createConfigWidget()
{
    return new CMakeBuildSettingsWidget;
}

QList<ProjectExplorer::BuildStepConfigWidget*> CMakeProject::subConfigWidgets()
{
    return QList<ProjectExplorer::BuildStepConfigWidget*>();
}

// This method is called for new build configurations
// You should probably set some default values in this method
 void CMakeProject::newBuildConfiguration(const QString &buildConfiguration)
 {
     Q_UNUSED(buildConfiguration);
     //TODO
 }

ProjectExplorer::ProjectNode *CMakeProject::rootProjectNode() const
{
    return m_rootNode;
}


QStringList CMakeProject::files(FilesMode fileMode) const
{
    Q_UNUSED(fileMode);
    // TODO
    return m_files;
}

void CMakeProject::saveSettingsImpl(ProjectExplorer::PersistentSettingsWriter &writer)
{
    // TODO
    Project::saveSettingsImpl(writer);
}

void CMakeProject::restoreSettingsImpl(ProjectExplorer::PersistentSettingsReader &reader)
{
    // TODO
    Project::restoreSettingsImpl(reader);
    if (buildConfigurations().isEmpty()) {
        // No build configuration, adding those
        CMakeStep *cmakeStep = new CMakeStep(this);
        MakeStep *makeStep = new MakeStep(this);

        insertBuildStep(0, cmakeStep);
        insertBuildStep(1, makeStep);

        addBuildConfiguration("AllTargets");
        setActiveBuildConfiguration("AllTargets");
        makeStep->setValue("AllTargets", "buildTargets", QStringList() << "all");

        // Create build configurations of m_targets
        qDebug()<<"Create build configurations of m_targets";
        bool setActive = false;
        foreach(const CMakeTarget &ct, m_targets) {
            QSharedPointer<ProjectExplorer::RunConfiguration> rc(new CMakeRunConfiguration(this, ct.executable, ct.workingDirectory));
            addRunConfiguration(rc);
            // The first one gets the honour of beeing the active one
            if (!setActive) {
                setActiveRunConfiguration(rc);
                setActive = true;
            }
        }
        setActiveBuildConfiguration("all");

    }
    // Restoring is fine
}


CMakeFile::CMakeFile(CMakeProject *parent, QString fileName)
    : Core::IFile(parent), m_project(parent), m_fileName(fileName)
{

}

bool CMakeFile::save(const QString &fileName)
{
    // TODO
    // Once we have an texteditor open for this file, we probably do
    // need to implement this, don't we.
    Q_UNUSED(fileName);
    return false;
}

QString CMakeFile::fileName() const
{
    return m_fileName;
}

QString CMakeFile::defaultPath() const
{
    return QString();
}

QString CMakeFile::suggestedFileName() const
{
    return QString();
}

QString CMakeFile::mimeType() const
{
    return Constants::CMAKEMIMETYPE;
}


bool CMakeFile::isModified() const
{
    return false;
}

bool CMakeFile::isReadOnly() const
{
    return true;
}

bool CMakeFile::isSaveAsAllowed() const
{
    return false;
}

void CMakeFile::modified(ReloadBehavior *behavior)
{
    Q_UNUSED(behavior);
}


CMakeBuildSettingsWidget::CMakeBuildSettingsWidget()
{

}

QString CMakeBuildSettingsWidget::displayName() const
{
    return "CMake";
}

void CMakeBuildSettingsWidget::init(const QString &buildConfiguration)
{
    Q_UNUSED(buildConfiguration);
    // TODO
}

bool CMakeCbpParser::parseCbpFile(const QString &fileName)
{
    QFile fi(fileName);
    if (fi.exists() && fi.open(QFile::ReadOnly)) {
        setDevice(&fi);

        while (!atEnd()) {
            readNext();
            if (name() == "CodeBlocks_project_file") {
                parseCodeBlocks_project_file();
            } else if (isStartElement()) {
                parseUnknownElement();
            }
        }
        fi.close();
        m_includeFiles.sort();
        m_includeFiles.removeDuplicates();
        return true;
    }
    return false;
}

void CMakeCbpParser::parseCodeBlocks_project_file()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Project") {
            parseProject();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseProject()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Unit") {
            parseUnit();
        } else if (name() == "Build") {
            parseBuild();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseBuild()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Target") {
            parseTarget();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTarget()
{
    m_targetType = false;
    m_target.clear();

    if (attributes().hasAttribute("title"))
        m_target.title = attributes().value("title").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            if (m_targetType || m_target.title == "all") {
                m_targets.append(m_target);
            }
            return;
        } else if (name() == "Compiler") {
            parseCompiler();
        } else if (name() == "Option") {
            parseTargetOption();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetOption()
{
    if (attributes().hasAttribute("output"))
        m_target.executable = attributes().value("output").toString();
    else if (attributes().hasAttribute("type") && attributes().value("type") == "1")
        m_targetType = true;
    else if (attributes().hasAttribute("working_dir"))
        m_target.workingDirectory = attributes().value("working_dir").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "MakeCommand") {
            parseMakeCommand();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseMakeCommand()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Build") {
            parseTargetBuild();
        } else if (name() == "Clean") {
            parseTargetClean();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetBuild()
{
    if (attributes().hasAttribute("command"))
        m_target.makeCommand = attributes().value("command").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetClean()
{
    if (attributes().hasAttribute("command"))
        m_target.makeCleanCommand = attributes().value("command").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseCompiler()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Add") {
            parseAdd();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseAdd()
{
    m_includeFiles.append(attributes().value("directory").toString());
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseUnit()
{
    //qDebug()<<stream.attributes().value("filename");
    QString fileName = attributes().value("filename").toString();
    if (!fileName.endsWith(".rule"))
        m_fileList.append( new ProjectExplorer::FileNode(fileName, ProjectExplorer::SourceType, false));
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseUnknownElement()
{
    Q_ASSERT(isStartElement());

    while (!atEnd()) {
        readNext();

        if (isEndElement())
            break;

        if (isStartElement())
            parseUnknownElement();
    }
}

QList<ProjectExplorer::FileNode *> CMakeCbpParser::fileList()
{
    return m_fileList;
}

QStringList CMakeCbpParser::includeFiles()
{
    return m_includeFiles;
}

QList<CMakeTarget> CMakeCbpParser::targets()
{
    return m_targets;
}

void CMakeTarget::clear()
{
    executable = QString::null;
    makeCommand = QString::null;
    makeCleanCommand = QString::null;
    workingDirectory = QString::null;
    title = QString::null;
}

