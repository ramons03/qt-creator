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

#include "cmakerunconfiguration.h"

#include "cmakeproject.h"
#include "cmakeprojectconstants.h"

#include <projectexplorer/environment.h>
#include <utils/qtcassert.h>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;

CMakeRunConfiguration::CMakeRunConfiguration(CMakeProject *pro, const QString &target, const QString &workingDirectory)
    : ProjectExplorer::ApplicationRunConfiguration(pro), m_target(target), m_workingDirectory(workingDirectory)
{
    setName(target);
}

CMakeRunConfiguration::~CMakeRunConfiguration()
{
}

QString CMakeRunConfiguration::type() const
{
    return Constants::CMAKERUNCONFIGURATION;
}

QString CMakeRunConfiguration::executable() const
{
    return m_target;
}

ProjectExplorer::ApplicationRunConfiguration::RunMode CMakeRunConfiguration::runMode() const
{
    return ProjectExplorer::ApplicationRunConfiguration::Gui;
}

QString CMakeRunConfiguration::workingDirectory() const
{
    return m_workingDirectory;
}

QStringList CMakeRunConfiguration::commandLineArguments() const
{
    // TODO
    return QStringList();
}

ProjectExplorer::Environment CMakeRunConfiguration::environment() const
{
    // TODO
    return ProjectExplorer::Environment::systemEnvironment();
}

void CMakeRunConfiguration::save(ProjectExplorer::PersistentSettingsWriter &writer) const
{
    ProjectExplorer::ApplicationRunConfiguration::save(writer);
    writer.saveValue("CMakeRunConfiguration.Target", m_target);
    writer.saveValue("CMakeRunConfiguration.WorkingDirectory", m_workingDirectory);
}

void CMakeRunConfiguration::restore(const ProjectExplorer::PersistentSettingsReader &reader)
{
    ProjectExplorer::ApplicationRunConfiguration::restore(reader);
    m_target = reader.restoreValue("CMakeRunConfiguration.Target").toString();
    m_workingDirectory = reader.restoreValue("CMakeRunConfiguration.WorkingDirectory").toString();
}

QWidget *CMakeRunConfiguration::configurationWidget()
{
    //TODO
    return new QWidget();
}

// Factory
CMakeRunConfigurationFactory::CMakeRunConfigurationFactory()
{

}

CMakeRunConfigurationFactory::~CMakeRunConfigurationFactory()
{

}

// used to recreate the runConfigurations when restoring settings
bool CMakeRunConfigurationFactory::canCreate(const QString &type) const
{
    if (type.startsWith(Constants::CMAKERUNCONFIGURATION))
        return true;
    return false;
}

// used to show the list of possible additons to a project, returns a list of types
QStringList CMakeRunConfigurationFactory::canCreate(ProjectExplorer::Project *project) const
{
    CMakeProject *pro = qobject_cast<CMakeProject *>(project);
    if (!pro)
        return QStringList();
    // TODO gather all targets and return them here
    return QStringList();
}

// used to translate the types to names to display to the user
QString CMakeRunConfigurationFactory::nameForType(const QString &type) const
{
    Q_ASSERT(type.startsWith(Constants::CMAKERUNCONFIGURATION));

    if (type == Constants::CMAKERUNCONFIGURATION)
        return "CMake"; // Doesn't happen
    else
        return type.mid(QString(Constants::CMAKERUNCONFIGURATION).length());
}

QSharedPointer<ProjectExplorer::RunConfiguration> CMakeRunConfigurationFactory::create(ProjectExplorer::Project *project, const QString &type)
{
    CMakeProject *pro = qobject_cast<CMakeProject *>(project);
    Q_ASSERT(pro);
    if (type == Constants::CMAKERUNCONFIGURATION) {
        // Restoring, filename will be added by restoreSettings
        QSharedPointer<ProjectExplorer::RunConfiguration> rc(new CMakeRunConfiguration(pro, QString::null, QString::null));
        return rc;
    } else {
        // Adding new
        QString file = type.mid(QString(Constants::CMAKERUNCONFIGURATION).length());
        QSharedPointer<ProjectExplorer::RunConfiguration> rc(new CMakeRunConfiguration(pro, file, QString::null));
        return rc;
    }
}
