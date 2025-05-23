/*
 * Copyright (c) 2021-2022 Marcus Mohr.
 *
 * This file is part of HyTeG
 * (see https://i10git.cs.fau.de/hyteg/hyteg).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/DataTypes.h"
#include "core/Environment.h"
#include "core/debug/CheckFunctions.h"
#include "core/debug/TestSubsystem.h"
#include "core/mpi/all.h"
#include "core/timing/all.h"

#include "hyteg/eigen/EigenWrapper.hpp"
#include "hyteg/types/PointND.hpp"
#include "hyteg/types/Matrix.hpp"

// Perform some basic tests of interfacing with Eigen

using walberla::int_c;
using walberla::real_c;
using walberla::real_t;
using walberla::uint_t;

int main( int argc, char* argv[] )
{
   walberla::debug::enterTestMode();

   walberla::Environment walberlaEnv( argc, argv );
   walberla::logging::Logging::instance()->setLogLevel( walberla::logging::Logging::PROGRESS );
   walberla::MPIManager::instance()->useWorldComm();

   WALBERLA_LOG_INFO_ON_ROOT( "---------------------------" );
   WALBERLA_LOG_INFO_ON_ROOT( " Testing Eigen Interfacing" );
   WALBERLA_LOG_INFO_ON_ROOT( "---------------------------" );

   using vec3D = Eigen::Vector3d;
   // using mat3D = Eigen::Matrix3d;

   vec3D vec1;
   vec3D vec2{ real_c( 1 ), real_c( 1 ), real_c( -1 ) };

   for ( uint_t i = 0; i < 3; ++i )
   {
      vec1( int_c( i ) ) = real_c( i + 1 );
   }

   real_t dotProduct = vec1.dot( vec2 );

   WALBERLA_CHECK_FLOAT_EQUAL( dotProduct, real_c( 0 ) );

   WALBERLA_LOG_INFO_ON_ROOT( "---------------------------" );
   WALBERLA_LOG_INFO_ON_ROOT( " Testing (De)Serialisation" );
   WALBERLA_LOG_INFO_ON_ROOT( "---------------------------" );

   const int                                 numRows = 3;
   const int                                 numCols = 4;
   hyteg::Matrix< real_t, numRows, numCols > matMPI, matRef;
   for ( int i = 0; i < numRows; ++i )
   {
      for ( int j = 0; j < numCols; ++j )
      {
         matMPI( i, j ) = real_c( 0.5 ) * real_c( i + j );
      }
   }

   WALBERLA_LOG_INFO_ON_ROOT( "Matrix before test:\n" << matMPI );

   // check (de)serialisation works at least formally
   walberla::mpi::SendBuffer sendBuffer;
   sendBuffer << matMPI;
   walberla::mpi::RecvBuffer recvBuffer( sendBuffer );
   recvBuffer >> matRef;
   WALBERLA_CHECK_FLOAT_EQUAL( ( matMPI - matRef ).cwiseAbs().maxCoeff(), real_c( 0 ) );

   // real check with communication (requires >= 2 MPI procs to make sense)
   const auto rank   = walberla::mpi::MPIManager::instance()->rank();
   const auto nProcs = walberla::mpi::MPIManager::instance()->numProcesses();
   if ( nProcs > 1 )
   {
      if ( rank == 0 )
      {
         matMPI *= real_c( 2 );
      }
      walberla::mpi::broadcastObject( matMPI );
      WALBERLA_CHECK_FLOAT_EQUAL( ( matMPI - real_c( 2 ) * matRef ).cwiseAbs().maxCoeff(), real_c( 0 ) );
   }

   WALBERLA_LOG_INFO_ON_ROOT( "-------------------------------------" );
   WALBERLA_LOG_INFO_ON_ROOT( " Testing Container (De)Serialisation" );
   WALBERLA_LOG_INFO_ON_ROOT( "-------------------------------------" );

   std::vector< vec3D > sendVec1{ vec1, vec2 };
   std::vector< vec3D > sendVec2{ vec1 };

   walberla::mpi::SendBuffer sBuf;
   sBuf << sendVec1 << sendVec2 << real_c( 42 );

   std::vector< vec3D >      recvVec1;
   std::vector< vec3D >      recvVec2;
   walberla::mpi::RecvBuffer rBuf( sBuf );
   real_t                    recvVal;
   rBuf >> recvVec1 >> recvVec2 >> recvVal;

   WALBERLA_CHECK_FLOAT_EQUAL( recvVal, real_c( 42 ) );
   WALBERLA_CHECK_FLOAT_EQUAL( ( recvVec2[0] - sendVec2[0] ).norm(), real_c( 0 ) );

   return EXIT_SUCCESS;
}
