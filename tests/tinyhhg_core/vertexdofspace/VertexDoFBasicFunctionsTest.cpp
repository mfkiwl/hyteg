

#include "core/DataTypes.h"
#include "core/Environment.h"
#include "core/debug/CheckFunctions.h"
#include "core/debug/TestSubsystem.h"
#include "core/timing/all.h"

#include "tinyhhg_core/p1functionspace/VertexDoFFunction.hpp"
#include "tinyhhg_core/primitivestorage/SetupPrimitiveStorage.hpp"

namespace hhg {

using walberla::real_c;

static void testP2BasicFunctions()
{
  const uint_t minLevel = 2;
  const uint_t maxLevel = 4;

  MeshInfo mesh = MeshInfo::fromGmshFile( "../../data/meshes/tri_1el.msh" );

  SetupPrimitiveStorage setupStorage( mesh, uint_c( walberla::mpi::MPIManager::instance()->numProcesses() ) );

  std::shared_ptr< PrimitiveStorage > storage = std::make_shared< PrimitiveStorage >( setupStorage );

  auto x = std::make_shared< vertexdof::VertexDoFFunction< real_t > >( "x", storage, minLevel, maxLevel );
  auto y = std::make_shared< vertexdof::VertexDoFFunction< real_t > >( "y", storage, minLevel, maxLevel );

  std::vector< PrimitiveID > faces;
  storage->getFaceIDs( faces );
  Face * face = storage->getFace( faces[0] );

  real_t * faceVertexDataX = face->getData( x->getFaceDataID() )->getPointer( maxLevel );
  real_t * faceVertexDataY = face->getData( y->getFaceDataID() )->getPointer( maxLevel );

  // Interpolate

  std::function<real_t(const hhg::Point3D&)> expr = []( const Point3D & ) -> real_t { return real_c( 2 ); };

  walberla::WcTimingPool timer;

  timer["Interpolate"].start();
  x->interpolate( expr, maxLevel, DoFType::All );
  y->interpolate( expr, maxLevel, DoFType::All );
  timer["Interpolate"].end();


  for ( const auto & it : vertexdof::macroface::Iterator( maxLevel ) ){
    WALBERLA_CHECK_FLOAT_EQUAL( faceVertexDataX[vertexdof::macroface::index( maxLevel, it.col(), it.row())], real_c( 2 ) );

    WALBERLA_CHECK_FLOAT_EQUAL( faceVertexDataY[vertexdof::macroface::index( maxLevel, it.col(), it.row())], real_c( 2 ) );
  }

  // Assign

  timer["Assign"].start();
  //y->assign( { 3.0, 2.0 }, { x.get(), y.get() }, maxLevel, DoFType::All );
  y->assign( { 3.0, 2.0 }, { x.get(), y.get() }, maxLevel, DoFType::All );
  timer["Assign"].end();


  for ( const auto & it : vertexdof::macroface::Iterator( maxLevel ) ){
    WALBERLA_CHECK_FLOAT_EQUAL( faceVertexDataY[vertexdof::macroface::index( maxLevel, it.col(), it.row())], real_c( 10 ) );
  }

  // Add

  timer["Add"].start();
  y->add( {{ 4.0, 3.0 }}, {{ x.get(), y.get() }}, maxLevel, DoFType::All );
  timer["Add"].end();


  // Dot

  timer["Dot"].start();
  const real_t scalarProduct = y->dot( *x, maxLevel, DoFType::All );
  timer["Dot"].end();

  WALBERLA_CHECK_FLOAT_EQUAL( scalarProduct, real_c( levelinfo::num_microvertices_per_face( maxLevel ) * 48 * 2 ) );

  WALBERLA_LOG_INFO_ON_ROOT( timer );


}

} // namespace hhg


int main( int argc, char* argv[] )
{
   walberla::debug::enterTestMode();

   walberla::Environment walberlaEnv(argc, argv);
   walberla::logging::Logging::instance()->setLogLevel( walberla::logging::Logging::PROGRESS );
   walberla::MPIManager::instance()->useWorldComm();
   hhg::testP2BasicFunctions();

   return EXIT_SUCCESS;
}