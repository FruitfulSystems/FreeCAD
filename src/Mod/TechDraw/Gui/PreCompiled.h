/***************************************************************************
 *   Copyright (c) 2007 Jürgen Riegel <juergen.riegel@web.de>              *
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

#ifndef DRAWINGGUI_PRECOMPILED_H
#define DRAWINGGUI_PRECOMPILED_H

#include <FCConfig.h>

#ifdef _MSC_VER
# pragma warning(disable : 4005)
#endif

#ifdef FC_OS_WIN32
# define NOMINMAX
#endif

#ifdef _PreComp_

// standard
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

// STL
#include <algorithm>
#include <bitset>
#include <list>
#include <map>
#include <queue>
#include <regex>
#include <set>
#include <stack>
#include <string>
#include <vector>

#ifdef FC_OS_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef small
#endif

// Qt Toolkit
#ifndef _QtAll__
# include <Gui/QtAll.h>
#endif
#include <QGLWidget>
#include <QPaintEngine>
#include <QSvgGenerator>
#include <QTextList>
#include <QXmlQuery>
#include <QXmlResultItems>

// OpenCasCade
#include <BRepAdaptor_Surface.hxx>
#include <BRepLProp_SLProps.hxx>
#include <gp_Dir.hxx>

// Open Inventor
#include <Inventor/misc/SoChildList.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#endif //_PreComp_

#endif // DRAWINGGUI_PRECOMPILED_H
