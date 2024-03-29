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

#include "buildstep.h"
#include "buildconfiguration.h"

#include <utils/qtcassert.h>

namespace ProjectExplorer {

BuildStep::BuildStep(Project * pro)
    : m_project(pro)
{
    m_configuration = new BuildConfiguration("");
}

BuildStep::~BuildStep()
{
    qDeleteAll(m_buildConfigurations);
    delete m_configuration;
}

Project * BuildStep::project() const
{
    return m_project;
}

void BuildStep::addBuildConfiguration(const QString &name)
{
    m_buildConfigurations.push_back(new BuildConfiguration(name));
}

void BuildStep::removeBuildConfiguration(const QString &name)
{
    for (int i = 0; i != m_buildConfigurations.size(); ++i)
        if (m_buildConfigurations.at(i)->name() == name) {
            delete m_buildConfigurations.at(i);
            m_buildConfigurations.removeAt(i);
            break;
        }
}

void BuildStep::copyBuildConfiguration(const QString &source, const QString &dest)
{
    for (int i = 0; i != m_buildConfigurations.size(); ++i)
        if (m_buildConfigurations.at(i)->name() == source)
            m_buildConfigurations.push_back(new BuildConfiguration(dest, m_buildConfigurations.at(i)));
}

void BuildStep::setValue(const QString &buildConfiguration, const QString &name, const QVariant &value)
{
    BuildConfiguration *bc = getBuildConfiguration(buildConfiguration);
    Q_ASSERT(bc);
    bc->setValue(name, value);
}

void BuildStep::setValue(const QString &name, const QVariant &value)
{
    m_configuration->setValue(name, value);
}

QVariant BuildStep::value(const QString &buildConfiguration, const QString &name) const
{
    BuildConfiguration *bc = getBuildConfiguration(buildConfiguration);
    if (bc)
        return bc->getValue(name);
    else
        return QVariant();
}

QVariant BuildStep::value(const QString &name) const
{
    return m_configuration->getValue(name);
}

void BuildStep::setValuesFromMap(const QMap<QString, QVariant> & values)
{
    m_configuration->setValuesFromMap(values);
}

void BuildStep::setValuesFromMap(const QString & buildConfiguration, const QMap<QString, QVariant> & values)
{
    getBuildConfiguration(buildConfiguration)->setValuesFromMap(values);
}

QMap<QString, QVariant> BuildStep::valuesToMap()
{
    return m_configuration->toMap();
}

QMap<QString, QVariant> BuildStep::valuesToMap(const QString & buildConfiguration)
{
    return getBuildConfiguration(buildConfiguration)->toMap();
}

BuildConfiguration * BuildStep::getBuildConfiguration(const QString & name) const
{
    for (int i = 0; i != m_buildConfigurations.size(); ++i)
        if (m_buildConfigurations.at(i)->name() == name)
            return m_buildConfigurations.at(i);
    return 0;
}

bool BuildStep::immutable() const
{
    return false;
}

IBuildStepFactory::IBuildStepFactory()
{

}

IBuildStepFactory::~IBuildStepFactory()
{

}

} // namespace ProjectExplorer
