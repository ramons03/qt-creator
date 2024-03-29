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

#include "modulespage.h"

#include "speinfo.h"

#include <utils/qtcassert.h>

#include <QtCore/QDebug>

#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QWidget>

#include <math.h>

using namespace Qt4ProjectManager::Internal;

ModulesPage::ModulesPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select required modules"));
    QLabel *label = new QLabel(tr("Select the modules you want to include in your "
        "project. The recommended modules for this project are selected by default."));
    label->setWordWrap(true);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(label);
    vlayout->addItem(new QSpacerItem(0, 20));

    QGridLayout *layout = new QGridLayout;

    const QList<SPEInfoItem*> infoItemsList = *SPEInfo::list(SPEInfoItem::QtModule);
    int itemId = 0;
    int rowsCount = (infoItemsList.count() + 1) / 2;
    foreach (const SPEInfoItem *infoItem, infoItemsList) {
        QCheckBox *moduleCheckBox = new QCheckBox(infoItem->name());
        moduleCheckBox->setToolTip(infoItem->description());
        moduleCheckBox->setWhatsThis(infoItem->description());
        registerField(infoItem->id(), moduleCheckBox);
        int row = itemId % rowsCount;
        int column = itemId / rowsCount;
        layout->addWidget(moduleCheckBox, row, column);
        m_moduleCheckBoxMap[infoItem->id()] = moduleCheckBox;
        itemId++;
    }

    vlayout->addLayout(layout);
    setLayout(vlayout);
}

// Return the key that goes into the Qt config line for a module
QString ModulesPage::idOfModule(const QString &module)
{
    const QList<SPEInfoItem*> infoItemsList = *SPEInfo::list(SPEInfoItem::QtModule);
    foreach (const SPEInfoItem *infoItem, infoItemsList)
        if (infoItem->name().startsWith(module))
            return infoItem->id();
    return QString();
}

QString ModulesPage::selectedModules() const
{
    return modules(true);
}

QString ModulesPage::deselectedModules() const
{
    return modules(false);
}

void ModulesPage::setModuleSelected(const QString &module, bool selected) const
{
    QCheckBox *checkBox = m_moduleCheckBoxMap[module];
    Q_ASSERT(checkBox);
    checkBox->setCheckState(selected?Qt::Checked:Qt::Unchecked);
}

void ModulesPage::setModuleEnabled(const QString &module, bool enabled) const
{
    QCheckBox *checkBox = m_moduleCheckBoxMap[module];
    Q_ASSERT(checkBox);
    checkBox->setEnabled(enabled);
}

QString ModulesPage::modules(bool selected) const
{
    QStringList modules;

    const QList<SPEInfoItem*> infoItemsList = *SPEInfo::list(SPEInfoItem::QtModule);
    foreach (const SPEInfoItem *infoItem, infoItemsList) {
        if (selected != infoItem->data(SPEInfoItem::keyIncludedByDefault).toBool()
            && selected == field(infoItem->id()).toBool())
            modules << infoItem->id();
    }

    return modules.join(QString(QLatin1Char(' ')));
}
