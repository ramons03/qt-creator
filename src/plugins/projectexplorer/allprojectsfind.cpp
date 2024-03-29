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

#include "allprojectsfind.h"

#include "project.h"
#include "projectexplorer.h"

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QtGui/QGridLayout>

using namespace Find;
using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;
using namespace TextEditor;

AllProjectsFind::AllProjectsFind(ProjectExplorerPlugin *plugin, Core::ICore *core, SearchResultWindow *resultWindow)
    : BaseFileFind(core, resultWindow),
    m_plugin(plugin),
    m_configWidget(0)
{
    connect(m_plugin, SIGNAL(fileListChanged()), this, SIGNAL(changed()));
}

QString AllProjectsFind::name() const
{
    return tr("All Projects");
}

bool AllProjectsFind::isEnabled() const
{
    return BaseFileFind::isEnabled()
            && m_plugin->session() != 0
            && m_plugin->session()->projects().count() > 0;
}

QKeySequence AllProjectsFind::defaultShortcut() const
{
    return QKeySequence("Ctrl+Shift+F");
}

QStringList AllProjectsFind::files()
{
    Q_ASSERT(m_plugin->session());
    QList<QRegExp> filterRegs;
    QStringList nameFilters = fileNameFilters();
    foreach (const QString &filter, nameFilters) {
        filterRegs << QRegExp(filter, Qt::CaseInsensitive, QRegExp::Wildcard);
    }
    QStringList files;
    QStringList projectFiles;
    foreach (const Project *project, m_plugin->session()->projects()) {
        projectFiles = project->files(Project::AllFiles);
        if (!filterRegs.isEmpty()) {
            foreach (const QString &file, projectFiles) {
                foreach (const QRegExp &reg, filterRegs) {
                    if (reg.exactMatch(file)) {
                        files.append(file);
                        break;
                    }
                }
            }
        } else {
            files += projectFiles;
        }
    }
    files.removeDuplicates();
    return files;
}

QWidget *AllProjectsFind::createConfigWidget()
{
    if (!m_configWidget) {
        m_configWidget = new QWidget;
        QGridLayout * const gridLayout = new QGridLayout(m_configWidget);
        gridLayout->setMargin(0);
        m_configWidget->setLayout(gridLayout);
        gridLayout->addWidget(createRegExpWidget(), 0, 1);
        QLabel * const filePatternLabel = new QLabel(tr("File &pattern:"));
        filePatternLabel->setMinimumWidth(80);
        filePatternLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        filePatternLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QWidget *patternWidget = createPatternWidget();
        filePatternLabel->setBuddy(patternWidget);
        gridLayout->addWidget(filePatternLabel, 1, 0, Qt::AlignRight);
        gridLayout->addWidget(patternWidget, 1, 1);
        m_configWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    return m_configWidget;
}

void AllProjectsFind::writeSettings(QSettings *settings)
{
    settings->beginGroup("AllProjectsFind");
    writeCommonSettings(settings);
    settings->endGroup();
}

void AllProjectsFind::readSettings(QSettings *settings)
{
    settings->beginGroup("AllProjectsFind");
    readCommonSettings(settings, "*");
    settings->endGroup();
}
