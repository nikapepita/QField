/***************************************************************************
                            qgismobileapp.cpp
                              -------------------
              begin                : Wed Apr 04 10:48:28 CET 2012
              copyright            : (C) 2012 by Marco Bernasocchi
              email                : marco@bernawebdesign.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>

#include <unistd.h>
#include <stdlib.h>

#include <proj.h>

#include <QFontDatabase>
#include <QStandardPaths>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlApplicationEngine>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QFileDialog> // Until native looking QML dialogs are implemented (Qt5.4?)
#include <QtWidgets/QMenu> // Until native looking QML dialogs are implemented (Qt5.4?)
#include <QtWidgets/QMenuBar>
#include <QStandardItemModel>
#include <QPrinter>
#include <QPrintDialog>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStyleHints>

#include <qgslayertreemodel.h>
#include <qgslocalizeddatapathregistry.h>
#include <qgsproject.h>
#include <qgsfeature.h>
#include <qgsvectorlayer.h>
#include <qgssnappingutils.h>
#include <qgsunittypes.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmapthemecollection.h>
#include <qgsprintlayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutitemmap.h>
#include <qgslocator.h>
#include <qgslocatormodel.h>
#include <qgsfield.h>
#include <qgsfieldconstraints.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayereditbuffer.h>
#include <qgsexpressionfunction.h>

#include "qgsquickmapsettings.h"
#include "qgsquickmapcanvasmap.h"
#include "qgsquickcoordinatetransformer.h"
#include "qgsquickmaptransform.h"

#include "qgsnetworkaccessmanager.h"

#include "qgismobileapp.h"

#include "appinterface.h"
#include "featurelistmodelselection.h"
#include "featurelistextentcontroller.h"
#include "modelhelper.h"
#include "rubberband.h"
#include "rubberbandmodel.h"
#include "qgsofflineediting.h"
#include "messagelogmodel.h"
#include "attributeformmodel.h"
#include "geometry.h"
#include "featuremodel.h"
#include "layertreemapcanvasbridge.h"
#include "identifytool.h"
#include "submodel.h"
#include "expressionvariablemodel.h"
#include "badlayerhandler.h"
#include "snappingutils.h"
#include "snappingresult.h"
#include "layertreemodel.h"
#include "legendimageprovider.h"
#include "featurelistmodel.h"
#include "qgsrelationmanager.h"
#include "distancearea.h"
#include "printlayoutlistmodel.h"
#include "vertexmodel.h"
#include "maptoscreen.h"
#include "projectsource.h"
#include "locatormodelsuperbridge.h"
#include "qgsgeometrywrapper.h"
#include "linepolygonhighlight.h"
#include "valuemapmodel.h"
#include "recentprojectlistmodel.h"
#include "referencingfeaturelistmodel.h"
#include "featurechecklistmodel.h"
#include "geometryeditorsmodel.h"
#include "geometryutils.h"
#include "trackingmodel.h"
#include "fileutils.h"
#include "featureutils.h"
#include "expressionevaluator.h"
#include "stringutils.h"
#include "urlutils.h"
#include "changelogcontents.h"

#define QUOTE(string) _QUOTE(string)
#define _QUOTE(string) #string


QgisMobileapp::QgisMobileapp( QgsApplication *app, QObject *parent )
  : QQmlApplicationEngine( parent )
  , mIface( new AppInterface( this ) )
  , mFirstRenderingFlag( true )
{
#if 0
  QSurfaceFormat format;
  format.setSamples( 8 );
  setFormat( format );
  create();
#endif

  //set the authHandler to qfield-handler
  std::unique_ptr<QgsNetworkAuthenticationHandler> handler;
  mAuthRequestHandler = new QFieldAppAuthRequestHandler();
  handler.reset( mAuthRequestHandler );
  QgsNetworkAccessManager::instance()->setAuthHandler( std::move( handler ) );

  if ( !PlatformUtilities::instance()->qfieldDataDir().isEmpty() )
  {
    //set localized data paths
    QStringList localizedDataPaths;
    localizedDataPaths << PlatformUtilities::instance()->qfieldDataDir() + QStringLiteral( "basemaps/" );
    QgsApplication::instance()->localizedDataPathRegistry()->setPaths( localizedDataPaths );

    // set fonts
    const QString fontsPath  = PlatformUtilities::instance()->qfieldDataDir() + QStringLiteral( "fonts/" );
    QDir fontsDir( fontsPath );
    if ( fontsDir.exists() )
    {
      const QStringList fonts = fontsDir.entryList( QStringList() << "*.ttf" << "*.TTF" << "*.otf" << "*.OTF", QDir::Files );
      for ( auto font : fonts )
      {
        QFontDatabase::addApplicationFont( fontsPath + font );
      }
    }
  }

  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-Bold.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-BoldItalic.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-Condensed.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-Italic.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-Regular.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/Cadastra-Semibolditalic.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/CadastraSymbol-Mask.ttf" );
  QFontDatabase::addApplicationFont( ":/fonts/CadastraSymbol-Regular.ttf" );

  mProject = QgsProject::instance();
  mGpkgFlusher = qgis::make_unique<QgsGpkgFlusher>( mProject );
  mFlatLayerTree = new FlatLayerTreeModel( mProject->layerTreeRoot(), mProject, this );
  mLegendImageProvider = new LegendImageProvider( mFlatLayerTree->layerTreeModel() );
  mTrackingModel = new TrackingModel;

  // cppcheck-suppress leakReturnValNotUsed
  initDeclarative();

  if ( !PlatformUtilities::instance()->qfieldDataDir().isEmpty() )
  {
    // add extra proj search path to allow copying of transformation grid files
    QString path( proj_info().searchpath );
    QStringList paths;
#ifdef Q_OS_WIN
    paths = path.split( ';' );
#else
    paths = path.split( ':' );
#endif

    // thin out duplicates from paths -- see https://github.com/OSGeo/proj.4/pull/1498
    QSet<QString> existing;
    QStringList searchPaths;
    searchPaths.reserve( paths.count() );
    for ( QString &p : paths )
    {
      if ( existing.contains( p ) )
        continue;

      existing.insert( p );
      searchPaths << p;
    }

    searchPaths << QStringLiteral( "%1/proj/" ).arg( PlatformUtilities::instance()->qfieldDataDir() );
    char **newPaths = new char *[searchPaths.count()];
    for ( int i = 0; i < searchPaths.count(); ++i )
    {
      newPaths[i] = strdup( searchPaths.at( i ).toUtf8().constData() );
    }
    proj_context_set_search_paths( nullptr, searchPaths.count(), newPaths );
    for ( int i = 0; i < searchPaths.count(); ++i )
    {
      free( newPaths[i] );
    }
    delete [] newPaths;

    setenv( "PGSYSCONFDIR", PlatformUtilities::instance()->qfieldDataDir().toUtf8(), true );
  }

  PlatformUtilities::instance()->setScreenLockPermission( false );

  load( QUrl( "qrc:/qml/qgismobileapp.qml" ) );

  connect( this, &QQmlApplicationEngine::quit, app, &QgsApplication::quit );

  mMapCanvas = rootObjects().first()->findChild<QgsQuickMapCanvasMap *>();
  mMapCanvas->mapSettings()->setProject( mProject );

  mFlatLayerTree->layerTreeModel()->setLegendMapViewData( mMapCanvas->mapSettings()->mapSettings().mapUnitsPerPixel(),
      static_cast< int >( std::round( mMapCanvas->mapSettings()->outputDpi() ) ), mMapCanvas->mapSettings()->mapSettings().scale() );

  Q_ASSERT_X( mMapCanvas, "QML Init", "QgsQuickMapCanvasMap not found. It is likely that we failed to load the QML files. Check debug output for related messages." );

  connect( mProject, &QgsProject::readProject, this, &QgisMobileapp::onReadProject );

  mLayerTreeCanvasBridge = new LayerTreeMapCanvasBridge( mFlatLayerTree, mMapCanvas->mapSettings(), mTrackingModel, this );
  connect( this, &QgisMobileapp::loadProjectStarted, mIface, &AppInterface::loadProjectStarted );
  connect( this, &QgisMobileapp::loadProjectEnded, mIface, &AppInterface::loadProjectEnded );
  QTimer::singleShot( 1, this, &QgisMobileapp::onAfterFirstRendering );

  mOfflineEditing = new QgsOfflineEditing();

  mSettings.setValue( "/Map/searchRadiusMM", 5 );

  mAppMissingGridHandler = new AppMissingGridHandler( this );
}

void QgisMobileapp::initDeclarative()
{

#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QResource::registerResource(QStringLiteral("assets:/android_rcc_bundle.rcc"));
#endif
  addImportPath( QStringLiteral( "qrc:/qml/imports" ) );

  // Register QGIS QML types
  qmlRegisterType<QgsSnappingUtils>( "org.qgis", 1, 0, "SnappingUtils" );
  qmlRegisterType<QgsMapLayerProxyModel>( "org.qgis", 1, 0, "MapLayerModel" );
  qmlRegisterType<QgsVectorLayer>( "org.qgis", 1, 0, "VectorLayer" );
  qmlRegisterType<QgsMapThemeCollection>( "org.qgis", 1, 0, "MapThemeCollection" );
  qmlRegisterType<QgsLocatorProxyModel>( "org.qgis", 1, 0, "QgsLocatorProxyModel" );
  qmlRegisterType<QgsVectorLayerEditBuffer>( "org.qgis", 1, 0, "QgsVectorLayerEditBuffer" );

  qRegisterMetaType<QgsGeometry>( "QgsGeometry" );
  qRegisterMetaType<QgsFeature>( "QgsFeature" );
  qRegisterMetaType<QgsPoint>( "QgsPoint" );
  qRegisterMetaType<QgsPointXY>( "QgsPointXY" );
  qRegisterMetaType<QgsPointSequence>( "QgsPointSequence" );
  qRegisterMetaType<QgsCoordinateTransformContext>( "QgsCoordinateTransformContext" );
  qRegisterMetaType<QgsWkbTypes::GeometryType>( "QgsWkbTypes::GeometryType" ); // could be removed since we have now qmlRegisterUncreatableType<QgsWkbTypes> ?
  qRegisterMetaType<QgsWkbTypes::Type>( "QgsWkbTypes::Type" ); // could be removed since we have now qmlRegisterUncreatableType<QgsWkbTypes> ?
  qRegisterMetaType<QgsMapLayerType>( "QgsMapLayerType" ); // could be removed since we have now qmlRegisterUncreatableType<QgsWkbTypes> ?
  qRegisterMetaType<QgsFeatureId>( "QgsFeatureId" );
  qRegisterMetaType<QgsAttributes>( "QgsAttributes" );
  qRegisterMetaType<QgsSnappingConfig>( "QgsSnappingConfig" );
  qRegisterMetaType<QgsUnitTypes::DistanceUnit>( "QgsUnitTypes::DistanceUnit" );
  qRegisterMetaType<QgsUnitTypes::AreaUnit>( "QgsUnitTypes::AreaUnit" );
  qRegisterMetaType<QgsRelation>( "QgsRelation" );
  qRegisterMetaType<QgsField>( "QgsField" );
  qRegisterMetaType<QVariant::Type>( "QVariant::Type" );
  qRegisterMetaType<QgsDefaultValue>( "QgsDefaultValue" );
  qRegisterMetaType<QgsFieldConstraints>( "QgsFieldConstraints" );
  qRegisterMetaType<QgsGeometry::OperationResult>( "QgsGeometry::OperationResult" );

  qmlRegisterUncreatableType<QgsProject>( "org.qgis", 1, 0, "Project", "" );
  qmlRegisterUncreatableType<QgsCoordinateReferenceSystem>( "org.qgis", 1, 0, "CoordinateReferenceSystem", "" );
  qmlRegisterUncreatableType<QgsUnitTypes>( "org.qgis", 1, 0, "QgsUnitTypes", "" );
  qmlRegisterUncreatableType<QgsRelationManager>( "org.qgis", 1, 0, "RelationManager", "The relation manager is available from the QgsProject. Try `qgisProject.relationManager`" );
  qmlRegisterUncreatableType<QgsWkbTypes>( "org.qgis", 1, 0, "QgsWkbTypes", "" );
  qmlRegisterUncreatableType<QgsMapLayer>( "org.qgis", 1, 0, "MapLayer", "" );
  qmlRegisterUncreatableType<QgsVectorLayer>( "org.qgis", 1, 0, "VectorLayerStatic", "" );
  qmlRegisterUncreatableType<QgsGeometry>( "org.qgis", 1, 0, "QgsGeometryStatic", "" );

  // Register QgsQuick QML types
  qmlRegisterType<QgsQuickMapCanvasMap>( "org.qgis", 1, 0, "MapCanvasMap" );
  qmlRegisterType<QgsQuickMapSettings>( "org.qgis", 1, 0, "MapSettings" );
  qmlRegisterType<QgsQuickCoordinateTransformer>( "org.qfield", 1, 0, "CoordinateTransformer" );

  REGISTER_SINGLETON( "Utils", QgsQuickUtils, "Utils" );

  qmlRegisterType<QgsQuickMapTransform>( "org.qgis", 1, 0, "MapTransform" );

  // Register QField QML types
  qmlRegisterType<MultiFeatureListModel>( "org.qgis", 1, 0, "MultiFeatureListModel" );
  qmlRegisterType<FeatureListModel>( "org.qgis", 1, 0, "FeatureListModel" );
  qmlRegisterType<FeatureListModelSelection>( "org.qgis", 1, 0, "FeatureListModelSelection" );
  qmlRegisterType<FeatureListExtentController>( "org.qgis", 1, 0, "FeaturelistExtentController" );
  qmlRegisterType<Geometry>( "org.qgis", 1, 0, "Geometry" );
  qmlRegisterType<ModelHelper>( "org.qgis", 1, 0, "ModelHelper" );
  qmlRegisterType<Rubberband>( "org.qgis", 1, 0, "Rubberband" );
  qmlRegisterType<RubberbandModel>( "org.qgis", 1, 0, "RubberbandModel" );
  qmlRegisterType<PictureSource>( "org.qgis", 1, 0, "PictureSource" );
  qmlRegisterType<ProjectSource>( "org.qgis", 1, 0, "ProjectSource" );
  qmlRegisterType<ViewStatus>( "org.qgis", 1, 0, "ViewStatus" );
  qmlRegisterType<MessageLogModel>( "org.qgis", 1, 0, "MessageLogModel" );
  qmlRegisterType<AttributeFormModel>( "org.qfield", 1, 0, "AttributeFormModel" );
  qmlRegisterType<FeatureModel>( "org.qfield", 1, 0, "FeatureModel" );
  qmlRegisterType<IdentifyTool>( "org.qfield", 1, 0, "IdentifyTool" );
  qmlRegisterType<SubModel>( "org.qfield", 1, 0, "SubModel" );
  qmlRegisterType<ExpressionVariableModel>( "org.qfield", 1, 0, "ExpressionVariableModel" );
  qmlRegisterType<BadLayerHandler>( "org.qfield", 1, 0, "BadLayerHandler" );
  qmlRegisterType<SnappingUtils>( "org.qfield", 1, 0, "SnappingUtils" );
  qmlRegisterType<DistanceArea>( "org.qfield", 1, 0, "DistanceArea" );
  qmlRegisterType<FocusStack>( "org.qfield", 1, 0, "FocusStack" );
  qmlRegisterType<PrintLayoutListModel>( "org.qfield", 1, 0, "PrintLayoutListModel" );
  qmlRegisterType<VertexModel>( "org.qfield", 1, 0, "VertexModel" );
  qmlRegisterType<MapToScreen>( "org.qfield", 1, 0, "MapToScreen" );
  qmlRegisterType<LocatorModelSuperBridge>( "org.qfield", 1, 0, "LocatorModelSuperBridge" );
  qmlRegisterType<LocatorActionsModel>( "org.qfield", 1, 0, "LocatorActionsModel" );
  qmlRegisterType<LinePolygonHighlight>( "org.qfield", 1, 0, "LinePolygonHighlight" );
  qmlRegisterType<QgsGeometryWrapper>( "org.qfield", 1, 0, "QgsGeometryWrapper" );
  qmlRegisterType<ValueMapModel>( "org.qfield", 1, 0, "ValueMapModel" );
  qmlRegisterType<RecentProjectListModel>( "org.qgis", 1, 0, "RecentProjectListModel" );
  qmlRegisterType<ReferencingFeatureListModel>( "org.qgis", 1, 0, "ReferencingFeatureListModel" );
  qmlRegisterType<FeatureCheckListModel>( "org.qgis", 1, 0, "FeatureCheckListModel" );
  qmlRegisterType<GeometryEditorsModel>( "org.qfield", 1, 0, "GeometryEditorsModel" );
  qmlRegisterType<ExpressionEvaluator>( "org.qfield", 1, 0, "ExpressionEvaluator" );
  qmlRegisterType<ChangelogContents>( "org.qfield", 1, 0, "ChangelogContents" );
  REGISTER_SINGLETON( "org.qfield", GeometryEditorsModel, "GeometryEditorsModelSingleton" );
  REGISTER_SINGLETON( "org.qfield", GeometryUtils, "GeometryUtils" );
  REGISTER_SINGLETON( "org.qfield", FeatureUtils, "FeatureUtils" );
  REGISTER_SINGLETON( "org.qfield", FileUtils, "FileUtils" );
  REGISTER_SINGLETON( "org.qfield", StringUtils, "StringUtils" );
  REGISTER_SINGLETON( "org.qfield", UrlUtils, "UrlUtils" );

  qmlRegisterUncreatableType<AppInterface>( "org.qgis", 1, 0, "QgisInterface", "QgisInterface is only provided by the environment and cannot be created ad-hoc" );
  qmlRegisterUncreatableType<Settings>( "org.qgis", 1, 0, "Settings", "" );
  qmlRegisterUncreatableType<PlatformUtilities>( "org.qgis", 1, 0, "PlatformUtilities", "" );
  qmlRegisterUncreatableType<FlatLayerTreeModel>( "org.qfield", 1, 0, "FlatLayerTreeModel", "The FlatLayerTreeModel is available as context property `flatLayerTree`." );
  qmlRegisterUncreatableType<TrackingModel>( "org.qfield", 1, 0, "TrackingModel", "The TrackingModel is available as context property `trackingModel`." );
  qmlRegisterUncreatableType<QgsGpkgFlusher>( "org.qfield", 1, 0, "QgsGpkgFlusher", "The gpkgFlusher is available as context property `gpkgFlusher`" );

  qRegisterMetaType<SnappingResult>( "SnappingResult" );

  // Calculate device pixels
  qreal dpi = std::max( QApplication::desktop()->logicalDpiY(), QApplication::desktop()->logicalDpiY() );
  dpi *= QApplication::desktop()->devicePixelRatioF();

  // Register some globally available variables
  rootContext()->setContextProperty( "ppi", dpi );
  rootContext()->setContextProperty( "mouseDoubleClickInterval", QApplication::styleHints()->mouseDoubleClickInterval() );
  rootContext()->setContextProperty( "qgisProject", mProject );
  rootContext()->setContextProperty( "iface", mIface );
  rootContext()->setContextProperty( "settings", &mSettings );
  rootContext()->setContextProperty( "version", QString( QUOTE( VERSTR ) ) );
  rootContext()->setContextProperty( "versionCode", QString( "" VERSIONCODE ) );
  rootContext()->setContextProperty( "flatLayerTree", mFlatLayerTree );
  rootContext()->setContextProperty( "platformUtilities", PlatformUtilities::instance() );
  rootContext()->setContextProperty( "CrsFactory", QVariant::fromValue<QgsCoordinateReferenceSystem>( mCrsFactory ) );
  rootContext()->setContextProperty( "UnitTypes", QVariant::fromValue<QgsUnitTypes>( mUnitTypes ) );
  rootContext()->setContextProperty( "ExifTools", QVariant::fromValue<QgsExifTools>( mExifTools ) );
  rootContext()->setContextProperty( "LocatorModelNoGroup", QgsLocatorModel::NoGroup );
  rootContext()->setContextProperty( "gpkgFlusher", mGpkgFlusher.get() );

// Check QGIS Version
#if VERSION_INT >= 30600
  rootContext()->setContextProperty( "qfieldAuthRequestHandler", mAuthRequestHandler );
#endif
  rootContext()->setContextProperty( "trackingModel", mTrackingModel );

  addImageProvider( QLatin1String( "legend" ), mLegendImageProvider );
}

void QgisMobileapp::loadProjectQuirks()
{
  // force update of canvas, without automatic changes to extent and OTF projections
  bool autoEnableCrsTransform = mLayerTreeCanvasBridge->autoEnableCrsTransform();
  bool autoSetupOnFirstLayer = mLayerTreeCanvasBridge->autoSetupOnFirstLayer();
  mLayerTreeCanvasBridge->setAutoEnableCrsTransform( false );
  mLayerTreeCanvasBridge->setAutoSetupOnFirstLayer( false );

  mLayerTreeCanvasBridge->setCanvasLayers();

  if ( autoEnableCrsTransform )
    mLayerTreeCanvasBridge->setAutoEnableCrsTransform( true );

  if ( autoSetupOnFirstLayer )
    mLayerTreeCanvasBridge->setAutoSetupOnFirstLayer( true );
}

void QgisMobileapp::removeRecentProject( const QString &path )
{
  QList<QPair<QString, QString>> projects = recentProjects();
  for ( int idx = 0; idx < projects.count(); idx++ )
  {
    if ( projects.at( idx ).second == path )
    {
      projects.removeAt( idx );
      break;
    }
  }
  saveRecentProjects( projects );
}

QList<QPair<QString, QString>> QgisMobileapp::recentProjects()
{
  QSettings settings;
  QList<QPair<QString, QString>> projects;

  settings.beginGroup( "/qgis/recentProjects" );
  const QStringList projectKeysList = settings.childGroups();
  QList<int> projectKeys;
  // This is overdoing it since we're clipping the recent projects list to five items at the moment, but might as well be futureproof
  for ( const QString &key : projectKeysList )
  {
    projectKeys.append( key.toInt() );
  }
  for ( int i = 0; i < projectKeys.count(); i++ )
  {
    settings.beginGroup( QString::number( projectKeys.at( i ) ) );
    projects << qMakePair( settings.value( QStringLiteral( "title" ) ).toString(), settings.value( QStringLiteral( "path" ) ).toString() );
    settings.endGroup();
  }
  settings.endGroup();
  return projects;
}

void QgisMobileapp::saveRecentProjects( QList<QPair<QString, QString>> &projects )
{
  QSettings settings;
  settings.remove( QStringLiteral( "/qgis/recentProjects" ) );
  for ( int idx = 0; idx < projects.count() && idx < 5; idx++ )
  {
    settings.beginGroup( QStringLiteral( "/qgis/recentProjects/%1" ).arg( idx ) );
    settings.setValue( QStringLiteral( "title" ), projects.at( idx ).first );
    settings.setValue( QStringLiteral( "path" ), projects.at( idx ).second );
    settings.endGroup();
  }
}

void QgisMobileapp::onReadProject( const QDomDocument &doc )
{
  Q_UNUSED( doc )
  QMap<QgsVectorLayer *, QgsFeatureRequest> requests;

  QList<QPair<QString, QString>> projects = recentProjects();
  QFileInfo fi( mProject->fileName() );
  QPair<QString, QString> project = qMakePair( mProject->title().isEmpty() ? fi.completeBaseName() : mProject->title(), mProject->fileName() );
  if ( projects.contains( project ) )
    projects.removeAt( projects.indexOf( project ) );
  projects.insert( 0, project );
  saveRecentProjects( projects );

  const QList<QgsMapLayer *> mapLayers { mProject->mapLayers().values() };
  for ( QgsMapLayer *layer : mapLayers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
    if ( vl )
    {
      const QVariant itinerary = vl->customProperty( "qgisMobile/itinerary" );
      if ( itinerary.isValid() )
      {
        requests.insert( vl, QgsFeatureRequest().setFilterExpression( itinerary.toString() ) );
      }
    }
  }
  if ( requests.count() )
  {
    qDebug() << QString( "Loading itinerary for %1 layers." ).arg( requests.count() );
    mIface->openFeatureForm();
  }
}

void QgisMobileapp::onAfterFirstRendering()
{
  // This should get triggered exactly once, so we disconnect it right away
  // disconnect( this, &QgisMobileapp::afterRendering, this, &QgisMobileapp::onAfterFirstRendering );

  if ( mFirstRenderingFlag )
  {
    if ( qApp->arguments().count() > 1 )
    {
      loadProjectFile( qApp->arguments().last() );
    }
    else if ( !PlatformUtilities::instance()->qgsProject().isNull() )
    {
      loadProjectFile( PlatformUtilities::instance()->qgsProject() );
    }
    mFirstRenderingFlag = false;
  }
}

void QgisMobileapp::loadLastProject()
{
  QVariant lastProjectFile = QSettings().value( "/qgis/recentProjects/0/path" );
  if ( lastProjectFile.isValid() )
    loadProjectFile( lastProjectFile.toString() );
}

void QgisMobileapp::loadProjectFile( const QString &path )
{
// Check QGIS Version
#if VERSION_INT >= 30600
  mAuthRequestHandler->clearStoredRealms();
#endif
  reloadProjectFile( path );
}

void QgisMobileapp::reloadProjectFile( const QString &path )
{
  mProject->removeAllMapLayers();
  mTrackingModel->reset();

  emit loadProjectStarted( path );
  mProject->read( path );

  // load fonts in same directory
  QDir fontDir = QDir::cleanPath( QFileInfo( path ).absoluteDir().path() + QDir::separator() + ".fonts" );
  QStringList fontExts = QStringList() << "*.ttf" << "*.TTF" << "*.otf" << "*.OTF";
  const QStringList fontFiles = fontDir.entryList( fontExts, QDir::Files );
  for ( const QString &fontFile : fontFiles )
  {
    int id = QFontDatabase::addApplicationFont( QDir::cleanPath( fontDir.path() + QDir::separator() + fontFile ) );
    if ( id < 0 )
      QgsMessageLog::logMessage( tr( "Could not load font %1" ).arg( fontFile ) );
    else
      QgsMessageLog::logMessage( tr( "Loading font %1" ).arg( fontFile ) );
  }

  loadProjectQuirks();

  emit loadProjectEnded();
}

void QgisMobileapp::print( int layoutIndex )
{
  const QList<QgsPrintLayout *> projectLayouts( mProject->layoutManager()->printLayouts() );

  const auto &layoutToPrint = projectLayouts.at( layoutIndex );

  if ( layoutToPrint->pageCollection()->pageCount() == 0 )
    return;

  layoutToPrint->referenceMap()->zoomToExtent( mMapCanvas->mapSettings()->visibleExtent() );

  QPrinter printer;
  QString documentsLocation = QStringLiteral( "%1/QField" ).arg( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) );
  QDir documentsDir( documentsLocation );
  if ( !documentsDir.exists() )
    documentsDir.mkpath( "." );

  printer.setOutputFileName( documentsLocation  + '/' + layoutToPrint->name() + QStringLiteral( ".pdf" ) );

  QgsLayoutExporter::PrintExportSettings printSettings;
  printSettings.rasterizeWholeImage = layoutToPrint->customProperty( QStringLiteral( "rasterize" ), false ).toBool();

  layoutToPrint->refresh();

  QgsLayoutExporter exporter = QgsLayoutExporter( layoutToPrint );
  exporter.print( printer, printSettings );

  PlatformUtilities::instance()->open( printer.outputFileName() );
}

bool QgisMobileapp::event( QEvent *event )
{
  if ( event->type() == QEvent::Close )
    QgsApplication::instance()->quit();

  return QQmlApplicationEngine::event( event );
}

QgisMobileapp::~QgisMobileapp()
{
  delete mOfflineEditing;
  mProject->removeAllMapLayers();
  // Reintroduce when created on the heap
  delete mProject;
  delete mAppMissingGridHandler;
}

