/***************************************************************************
 *   Copyright (c) 2017 WandererFan <wandererfan@gmail.com>                *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#include <cmath>
#endif // #ifndef _PreComp_

#include <Base/Console.h>
#include <Base/Vector3D.h>

#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/Command.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/ViewProvider.h>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>

#include <Mod/TechDraw/App/HatchLine.h>
#include <Mod/TechDraw/App/DrawGeomHatch.h>
#include <Mod/TechDraw/App/DrawView.h>
#include <Mod/TechDraw/App/DrawUtil.h>

#include "ViewProviderGeomHatch.h"
#include "TaskGeomHatch.h"
#include <Mod/TechDraw/Gui/ui_TaskGeomHatch.h>

using namespace Gui;
using namespace TechDraw;
using namespace TechDrawGui;

TaskGeomHatch::TaskGeomHatch(TechDraw::DrawGeomHatch* inHatch, TechDrawGui::ViewProviderGeomHatch* inVp, bool mode) :
    ui(new Ui_TaskGeomHatch),
    m_hatch(inHatch),
    m_Vp(inVp),
    m_createMode(mode)
{
    ui->setupUi(this);
    connect(ui->fcFile, SIGNAL(fileNameSelected( const QString & )), this, SLOT(onFileChanged(void)));

    m_source = m_hatch->Source.getValue();
    getParameters();
    initUi();
}

void TaskGeomHatch::initUi()
{
    ui->fcFile->setFileName(QString::fromUtf8(m_file.data(), m_file.size()));
    std::vector<std::string> names = PATLineSpec::getPatternList(m_file);
    QStringList qsNames = listToQ(names);

    ui->cbName->addItems(qsNames);
    int nameIndex = ui->cbName->findText(QString::fromUtf8(m_name.data(), m_name.size()));
    if (nameIndex > -1) {
        ui->cbName->setCurrentIndex(nameIndex);
    } else {
        Base::Console().Warning("Warning - Pattern name *%s* not found in current PAT File\n", m_name.c_str());
    }
    connect(ui->cbName, SIGNAL(currentIndexChanged(int)), this, SLOT(onNameChanged()));

    ui->sbScale->setValue(m_scale);
    ui->sbScale->setSingleStep(0.1);
    connect(ui->sbScale, SIGNAL(valueChanged(double)), this, SLOT(onScaleChanged()));
    ui->sbWeight->setValue(m_weight);
    ui->sbWeight->setSingleStep(0.1);
    connect(ui->sbWeight, SIGNAL(valueChanged(double)), this, SLOT(onLineWeightChanged()));
    ui->ccColor->setColor(m_color.asValue<QColor>());
    connect(ui->ccColor, SIGNAL(changed()), this, SLOT(onColorChanged()));

    ui->dsbRotation->setValue(m_rotation);
    connect(ui->dsbRotation, SIGNAL(valueChanged(double)), this, SLOT(onRotationChanged()));
    ui->dsbOffsetX->setValue(m_offset.x);
    connect(ui->dsbOffsetX, SIGNAL(valueChanged(double)), this, SLOT(onOffsetChanged()));
    ui->dsbOffsetY->setValue(m_offset.y);
    connect(ui->dsbOffsetY, SIGNAL(valueChanged(double)), this, SLOT(onOffsetChanged()));
}

void TaskGeomHatch::onFileChanged()
{
    m_file = ui->fcFile->fileName().toUtf8().constData();
    std::vector<std::string> names = PATLineSpec::getPatternList(m_file);
    QStringList qsNames = listToQ(names);
    ui->cbName->clear();
    ui->cbName->addItems(qsNames);
    m_hatch->FilePattern.setValue(m_file);
    onNameChanged();                      //pattern name from old file is not
                                          //necessarily present in new file!
}

void TaskGeomHatch::onNameChanged()
{
    QString cText = ui->cbName->currentText();
    m_name = cText.toUtf8().constData();
    m_hatch->NamePattern.setValue(m_name);
}

void TaskGeomHatch::onScaleChanged()
{
    m_scale = ui->sbScale->value().getValue();
    m_hatch->ScalePattern.setValue(ui->sbScale->value().getValue());
    TechDraw::DrawView* dv = static_cast<TechDraw::DrawView*>(m_source);
    dv->requestPaint();
}

void TaskGeomHatch::onRotationChanged()
{
    m_rotation = ui->dsbRotation->value();
    m_hatch->PatternRotation.setValue(m_rotation);
    TechDraw::DrawView* dv = static_cast<TechDraw::DrawView*>(m_source);
    dv->requestPaint();
}

void TaskGeomHatch::onOffsetChanged()
{
    Base::Vector3d offset(ui->dsbOffsetX->value(), ui->dsbOffsetY->value(), 0.0);
    m_offset = offset;
    m_hatch->PatternOffset.setValue(m_offset);
    TechDraw::DrawView* dv = static_cast<TechDraw::DrawView*>(m_source);
    dv->requestPaint();
}

void TaskGeomHatch::onLineWeightChanged()
{
    m_weight =ui->sbWeight->value().getValue();
    m_Vp->WeightPattern.setValue(ui->sbWeight->value().getValue());
    TechDraw::DrawView* dv = static_cast<TechDraw::DrawView*>(m_source);
    dv->requestPaint();
}

void TaskGeomHatch::onColorChanged()
{
    m_color.setValue<QColor>(ui->ccColor->color());
    m_Vp->ColorPattern.setValue(m_color);
}

bool TaskGeomHatch::accept()
{
//    Base::Console().Message("TGH::accept()\n");
    updateValues();
    Gui::Command::doCommand(Gui::Command::Gui, "Gui.ActiveDocument.resetEdit()");
    m_hatch->recomputeFeature();                     //create the hatch lines
    TechDraw::DrawView* dv = static_cast<TechDraw::DrawView*>(m_source);
    dv->requestPaint();
    return true;
}

bool TaskGeomHatch::reject()
{
    if (getCreateMode()) {
        std::string HatchName = m_hatch->getNameInDocument();
        Gui::Command::doCommand(Gui::Command::Gui, "App.activeDocument().removeObject('%s')", HatchName.c_str());
        Gui::Command::doCommand(Gui::Command::Gui, "Gui.ActiveDocument.resetEdit()");
        m_source->touch();
        m_source->getDocument()->recompute();
    } else {
        m_hatch->FilePattern.setValue(m_origFile);
        m_hatch->NamePattern.setValue(m_origName);
        m_hatch->ScalePattern.setValue(m_origScale);
        m_hatch->PatternRotation.setValue(m_origRotation);
        m_hatch->PatternOffset.setValue(m_origOffset);
        m_Vp->ColorPattern.setValue(m_origColor);
        m_Vp->WeightPattern.setValue(m_origWeight);
    }
    return false;
}

void TaskGeomHatch::getParameters()
{
    m_file   = m_hatch->FilePattern.getValue();
    m_name   = m_hatch->NamePattern.getValue();
    m_scale  = m_hatch->ScalePattern.getValue();
    m_rotation = m_hatch->PatternRotation.getValue();
    m_offset   = m_hatch->PatternOffset.getValue();
    m_color  = m_Vp->ColorPattern.getValue();
    m_weight = m_Vp->WeightPattern.getValue();
    if (!getCreateMode()) {
        m_origFile   = m_hatch->FilePattern.getValue();
        m_origName   = m_hatch->NamePattern.getValue();
        m_origScale  = m_hatch->ScalePattern.getValue();
        m_origColor  = m_Vp->ColorPattern.getValue();
        m_origWeight = m_Vp->WeightPattern.getValue();
        m_origRotation = m_hatch->PatternRotation.getValue();
        m_origOffset   = m_hatch->PatternOffset.getValue();
    }
}

//move values from screen to DocObjs
void TaskGeomHatch::updateValues()
{
//    Base::Console().Message("TGH::updateValues()\n");
    m_file = (ui->fcFile->fileName()).toUtf8().constData();
    m_hatch->FilePattern.setValue(m_file);
    QString cText = ui->cbName->currentText();
    m_name = cText.toUtf8().constData();
    m_hatch->NamePattern.setValue(m_name);
    m_scale = ui->sbScale->value().getValue();
    m_hatch->ScalePattern.setValue(m_scale);
    m_color.setValue<QColor>(ui->ccColor->color());
    m_Vp->ColorPattern.setValue(m_color);
    m_weight = ui->sbWeight->value().getValue();
    m_Vp->WeightPattern.setValue(m_weight);
    m_hatch->PatternRotation.setValue(ui->dsbRotation->value());
}

QStringList TaskGeomHatch::listToQ(std::vector<std::string> inList)
{
    QStringList result;
    for (auto& s: inList) {
        QString qs = QString::fromUtf8(s.data(), s.size());
        result.append(qs);
    }
    return result;
}

void TaskGeomHatch::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TaskDlgGeomHatch::TaskDlgGeomHatch(TechDraw::DrawGeomHatch* inHatch, TechDrawGui::ViewProviderGeomHatch* inVp, bool mode) :
    TaskDialog(),
    viewProvider(nullptr)
{
    widget  = new TaskGeomHatch(inHatch, inVp, mode);
    taskbox = new Gui::TaskView::TaskBox(Gui::BitmapFactory().pixmap("TechDraw_TreeView"),
                                         widget->windowTitle(), true, nullptr);
    taskbox->groupLayout()->addWidget(widget);
    Content.push_back(taskbox);
}

TaskDlgGeomHatch::~TaskDlgGeomHatch()
{
}

void TaskDlgGeomHatch::setCreateMode(bool mode)
{
    widget->setCreateMode(mode);
}

void TaskDlgGeomHatch::update()
{
    //widget->updateTask();
}

//==== calls from the TaskView ===============================================================
void TaskDlgGeomHatch::open()
{
}

void TaskDlgGeomHatch::clicked(int i)
{
    Q_UNUSED(i);
}

bool TaskDlgGeomHatch::accept()
{
    widget->accept();
    return true;
}

bool TaskDlgGeomHatch::reject()
{
    widget->reject();
    return true;
}

#include <Mod/TechDraw/Gui/moc_TaskGeomHatch.cpp>
