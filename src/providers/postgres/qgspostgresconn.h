/***************************************************************************
  qgspostgresconn.h  -  connection class to PostgreSQL/PostGIS
                             -------------------
    begin                : 2011/01/28
    copyright            : (C) 2011 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPOSTGRESCONN_H
#define QGSPOSTGRESCONN_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QMutex>

#include "qgis.h"
#include "qgsdatasourceuri.h"
#include "qgswkbtypes.h"
#include "qgsconfig.h"

extern "C"
{
#include <libpq-fe.h>
}

class QgsField;

//! Spatial column types
enum QgsPostgresGeometryColumnType
{
  SctNone,
  SctGeometry,
  SctGeography,
  SctTopoGeometry,
  SctPcPatch,
  SctRaster
};

enum QgsPostgresPrimaryKeyType
{
  PktUnknown,
  PktInt,
  PktUint64,
  PktTid,
  PktOid,
  PktFidMap
};

//! Schema properties structure
struct QgsPostgresSchemaProperty
{
  QString name;
  QString description;
  QString owner;
};

//! Layer Property structure
// TODO: Fill to Postgres/PostGIS specifications
struct QgsPostgresLayerProperty
{
  // Postgres/PostGIS layer properties
  QList<QgsWkbTypes::Type>          types;
  QString                       schemaName;
  QString                       tableName;
  QString                       geometryColName;
  QgsPostgresGeometryColumnType geometryColType;
  QStringList                   pkCols;
  QList<int>                    srids;
  unsigned int                  nSpCols;
  QString                       sql;
  QString                       relKind;
  bool                          isView = false;
  bool                          isMaterializedView = false;
  bool                          isRaster = false;
  QString                       tableComment;

  // TODO: rename this !
  int size() const { Q_ASSERT( types.size() == srids.size() ); return types.size(); }

  QString defaultName() const
  {
    QString n = tableName;
    if ( nSpCols > 1 ) n += '.' + geometryColName;
    return n;
  }

  QgsPostgresLayerProperty at( int i ) const
  {
    QgsPostgresLayerProperty property;

    Q_ASSERT( i >= 0 && i < size() );

    property.types << types[ i ];
    property.srids << srids[ i ];
    property.schemaName         = schemaName;
    property.tableName          = tableName;
    property.geometryColName    = geometryColName;
    property.geometryColType    = geometryColType;
    property.pkCols             = pkCols;
    property.nSpCols            = nSpCols;
    property.sql                = sql;
    property.relKind            = relKind;
    property.isView             = isView;
    property.isRaster           = isRaster;
    property.isMaterializedView = isMaterializedView;
    property.tableComment       = tableComment;

    return property;
  }

#ifdef QGISDEBUG
  QString toString() const
  {
    QString typeString;
    const auto constTypes = types;
    for ( QgsWkbTypes::Type type : constTypes )
    {
      if ( !typeString.isEmpty() )
        typeString += '|';
      typeString += QString::number( type );
    }
    QString sridString;
    const auto constSrids = srids;
    for ( int srid : constSrids )
    {
      if ( !sridString.isEmpty() )
        sridString += '|';
      sridString += QString::number( srid );
    }

    return QStringLiteral( "%1.%2.%3 type=%4 srid=%5 pkCols=%6 sql=%7 nSpCols=%8" )
           .arg( schemaName,
                 tableName,
                 geometryColName,
                 typeString,
                 sridString,
                 pkCols.join( QStringLiteral( "|" ) ),
                 sql )
           .arg( nSpCols );
  }
#endif
};

class QgsPostgresResult
{
  public:
    explicit QgsPostgresResult( PGresult *result = nullptr ) : mRes( result ) {}
    ~QgsPostgresResult();

    QgsPostgresResult &operator=( PGresult *result );
    QgsPostgresResult &operator=( const QgsPostgresResult &src );

    QgsPostgresResult( const QgsPostgresResult &rh ) = delete;

    ExecStatusType PQresultStatus();
    QString PQresultErrorMessage();

    int PQntuples();
    QString PQgetvalue( int row, int col );
    bool PQgetisnull( int row, int col );

    int PQnfields();
    QString PQfname( int col );
    Oid PQftable( int col );
    Oid PQftype( int col );
    int PQfmod( int col );
    int PQftablecol( int col );
    Oid PQoidValue();

    PGresult *result() const { return mRes; }

  private:
    PGresult *mRes = nullptr;

};


class QgsPostgresConn : public QObject
{
    Q_OBJECT

  public:
    /*
     * \param shared allow using a shared connection. Should never be
     *        called from a thread other than the main one.
     *        An assertion guards against such programmatic error.
     */
    static QgsPostgresConn *connectDb( const QString &connInfo, bool readOnly, bool shared = true, bool transaction = false );

    void ref() { ++mRef; }
    void unref();

    //! Gets postgis version string
    QString postgisVersion();

    //! Gets status of GEOS capability
    bool hasGEOS();

    //! Gets status of topology capability
    bool hasTopology();

    //! Gets status of Pointcloud capability
    bool hasPointcloud();

    //! Gets status of Raster capability
    bool hasRaster();

    //! Gets status of GIST capability
    bool hasGIST();

    //! Gets status of PROJ4 capability
    bool hasPROJ();

    //! encode wkb in hex
    bool useWkbHex() { return mUseWkbHex; }

    //! major PostGIS version
    int majorVersion() { return mPostgisVersionMajor; }

    //! minor PostGIS version
    int minorVersion() { return mPostgisVersionMinor; }

    //! PostgreSQL version
    int pgVersion() { return mPostgresqlVersion; }

    //! run a query and free result buffer
    bool PQexecNR( const QString &query );

    //! cursor handling
    bool openCursor( const QString &cursorName, const QString &declare );
    bool closeCursor( const QString &cursorName );

    QString uniqueCursorName();

#if 0
    PGconn *pgConnection() { return mConn; }
#endif

    //
    // libpq wrapper
    //

    // run a query and check for errors, thread-safe
    PGresult *PQexec( const QString &query, bool logError = true, bool retry = true ) const;
    void PQfinish();
    QString PQerrorMessage() const;
    int PQstatus() const;
    PGresult *PQprepare( const QString &stmtName, const QString &query, int nParams, const Oid *paramTypes );
    PGresult *PQexecPrepared( const QString &stmtName, const QStringList &params );

    /**
     * PQsendQuery is used for asynchronous queries (with PQgetResult)
     * Thread safety must be ensured by the caller by calling QgsPostgresConn::lock() and QgsPostgresConn::unlock()
     */
    int PQsendQuery( const QString &query );

    /**
     * PQgetResult is used for asynchronous queries (with PQsendQuery)
     * Thread safety must be ensured by the caller by calling QgsPostgresConn::lock() and QgsPostgresConn::unlock()
     */
    PGresult *PQgetResult();

    bool begin();
    bool commit();
    bool rollback();


    // cancel running query
    bool cancel();

    /**
     * Double quote a PostgreSQL identifier for placement in a SQL string.
     */
    static QString quotedIdentifier( const QString &ident );

    /**
     * Quote a value for placement in a SQL string.
     */
    static QString quotedValue( const QVariant &value );

    /**
     * Quote a json(b) value for placement in a SQL string.
     * \note a null value will be represented as a NULL and not as a json null.
     */
    static QString quotedJsonValue( const QVariant &value );

    /**
     * Gets the list of supported layers
     * \param layers list to store layers in
     * \param searchGeometryColumnsOnly only look for geometry columns which are
     * contained in the geometry_columns metatable
     * \param searchPublicOnly
     * \param allowGeometrylessTables
     * \param schema restrict layers to layers within specified schema
     * \returns true if layers were fetched successfully
     */
    bool supportedLayers( QVector<QgsPostgresLayerProperty> &layers,
                          bool searchGeometryColumnsOnly = true,
                          bool searchPublicOnly = true,
                          bool allowGeometrylessTables = false,
                          const QString &schema = QString() );

    /**
     * Gets the list of database schemas
     * \param schemas list to store schemas in
     * \returns true if schemas where fetched successfully
     * \since QGIS 2.7
     */
    bool getSchemas( QList<QgsPostgresSchemaProperty> &schemas );

    void retrieveLayerTypes( QgsPostgresLayerProperty &layerProperty, bool useEstimatedMetadata );

    /**
     * Gets information about the spatial tables
     * \param searchGeometryColumnsOnly only look for geometry columns which are
     * contained in the geometry_columns metatable
     * \param searchPublicOnly
     * \param allowGeometrylessTables
     * \param schema restrict tables to those within specified schema
     * \returns true if tables were successfully queried
     */
    bool getTableInfo( bool searchGeometryColumnsOnly, bool searchPublicOnly, bool allowGeometrylessTables,
                       const QString &schema = QString() );

    qint64 getBinaryInt( QgsPostgresResult &queryResult, int row, int col );

    QString fieldExpression( const QgsField &fld, QString expr = "%1" );

    QString connInfo() const { return mConnInfo; }

    /**
     * Returns the underlying database.
     *
     * \since QGIS 3.0
     */
    QString currentDatabase() const;

    static const int GEOM_TYPE_SELECT_LIMIT;

    static QString displayStringForWkbType( QgsWkbTypes::Type wkbType );
    static QString displayStringForGeomType( QgsPostgresGeometryColumnType geomType );
    static QgsWkbTypes::Type wkbTypeFromPostgis( const QString &dbType );

    static QString postgisWkbTypeName( QgsWkbTypes::Type wkbType );
    static int postgisWkbTypeDim( QgsWkbTypes::Type wkbType );
    static void postgisWkbType( QgsWkbTypes::Type wkbType, QString &geometryType, int &dim );

    static QString postgisTypeFilter( QString geomCol, QgsWkbTypes::Type wkbType, bool castToGeometry );

    static QgsWkbTypes::Type wkbTypeFromGeomType( QgsWkbTypes::GeometryType geomType );
    static QgsWkbTypes::Type wkbTypeFromOgcWkbType( unsigned int ogcWkbType );

    static QStringList connectionList();
    static QString selectedConnection();
    static void setSelectedConnection( const QString &connName );
    static QgsDataSourceUri connUri( const QString &connName );
    static bool publicSchemaOnly( const QString &connName );
    static bool geometryColumnsOnly( const QString &connName );
    static bool dontResolveType( const QString &connName );
    static bool allowGeometrylessTables( const QString &connName );
    static bool allowProjectsInDatabase( const QString &connName );
    static void deleteConnection( const QString &connName );

    //! A connection needs to be locked when it uses transactions, see QgsPostgresConn::{begin,commit,rollback}
    void lock() { mLock.lock(); }
    void unlock() { mLock.unlock(); }

  private:
    QgsPostgresConn( const QString &conninfo, bool readOnly, bool shared, bool transaction );
    ~QgsPostgresConn() override;

    int mRef;
    int mOpenCursors;
    PGconn *mConn = nullptr;
    QString mConnInfo;

    //! GEOS capability
    bool mGeosAvailable;

    //! Topology capability
    bool mTopologyAvailable;

    //! PostGIS version string
    QString mPostgisVersionInfo;

    //! Are mPostgisVersionMajor, mPostgisVersionMinor, mGeosAvailable, mGistAvailable, mProjAvailable, mTopologyAvailable valid?
    bool mGotPostgisVersion;

    //! PostgreSQL version
    int mPostgresqlVersion;

    //! PostGIS major version
    int mPostgisVersionMajor;

    //! PostGIS minor version
    int mPostgisVersionMinor;

    //! GIST capability
    bool mGistAvailable;

    //! PROJ4 capability
    bool mProjAvailable;

    //! pointcloud support available
    bool mPointcloudAvailable;

    //! raster support available
    bool mRasterAvailable;

    //! encode wkb in hex
    bool mUseWkbHex;

    bool mReadOnly;

    static QMap<QString, QgsPostgresConn *> sConnectionsRW;
    static QMap<QString, QgsPostgresConn *> sConnectionsRO;

    //! Count number of spatial columns in a given relation
    void addColumnInfo( QgsPostgresLayerProperty &layerProperty, const QString &schemaName, const QString &viewName, bool fetchPkCandidates );

    //! List of the supported layers
    QVector<QgsPostgresLayerProperty> mLayersSupported;

    /**
     * Flag indicating whether data from binary cursors must undergo an
     * endian conversion prior to use
     \note

     XXX Umm, it'd be helpful to know what we're swapping from and to.
     XXX Presumably this means swapping from big-endian (network) byte order
     XXX to little-endian; but the inverse transaction is possible, too, and
     XXX that's not reflected in this variable
     */
    bool mSwapEndian;
    void deduceEndian();

    int mNextCursorId;

    bool mShared; //!< Whether the connection is shared by more providers (must not be if going to be used in worker threads)

    bool mTransaction;

    mutable QMutex mLock;
};

// clazy:excludeall=qstring-allocations

#endif
