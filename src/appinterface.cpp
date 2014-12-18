/***************************************************************************
                            appinterface.cpp
                              -------------------
              begin                : 10.12.2014
              copyright            : (C) 2014 by Matthias Kuhn
              email                : matthias.kuhn (at) opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "appinterface.h"
#include "qgismobileapp.h"

#include <qgsmaptoolidentify.h>

AppInterface::AppInterface( QgisMobileapp* app )
  : mApp( app )
{
}

void AppInterface::identifyFeatures( const QPointF point )
{
  mApp->identifyFeatures( point );
}

void AppInterface::openProjectDialog()
{
  mApp->openProjectDialog();
}

void AppInterface::openFeatureForm()
{
  emit openFeatureFormRequested();
}
