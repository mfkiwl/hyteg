/*
 * Copyright (c) 2017-2024 Dominik Thoennes, Nils Kohl, Marcus Mohr.
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

#include "hyteg/primitives/Cell.hpp"

#include "core/mpi/BufferDataTypeExtensions.h"

#include "hyteg/geometry/Intersection.hpp"
#include "hyteg/types/Matrix.hpp"

namespace hyteg {

Cell::Cell( const PrimitiveID&                                 primitiveID,
            const std::vector< PrimitiveID >&                  vertexIDs,
            const std::vector< PrimitiveID >&                  edgeIDs,
            const std::vector< PrimitiveID >&                  faceIDs,
            const std::array< Point3D, 4 >&                    coordinates,
            const std::array< std::map< uint_t, uint_t >, 6 >& edgeLocalVertexToCellLocalVertexMaps,
            const std::array< std::map< uint_t, uint_t >, 4 >& faceLocalVertexToCellLocalVertexMaps )
: Primitive( primitiveID )
, coordinates_( coordinates )
, edgeLocalVertexToCellLocalVertexMaps_( edgeLocalVertexToCellLocalVertexMaps )
, faceLocalVertexToCellLocalVertexMaps_( faceLocalVertexToCellLocalVertexMaps )
{
   WALBERLA_ASSERT_EQUAL( vertexIDs.size(), 4, "Only tetrahedron cells are supported (number of vertices mismatches)." );
   WALBERLA_ASSERT_EQUAL( edgeIDs.size(), 6, "Only tetrahedron cells are supported (number of edges mismatches)." );
   WALBERLA_ASSERT_EQUAL( faceIDs.size(), 4, "Only tetrahedron cells are supported (number of faces mismatches)." );

   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[0].size(), 2 );
   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[1].size(), 2 );
   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[2].size(), 2 );
   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[3].size(), 2 );
   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[4].size(), 2 );
   WALBERLA_ASSERT_EQUAL( edgeLocalVertexToCellLocalVertexMaps[5].size(), 2 );

   WALBERLA_ASSERT_EQUAL( faceLocalVertexToCellLocalVertexMaps[0].size(), 3 );
   WALBERLA_ASSERT_EQUAL( faceLocalVertexToCellLocalVertexMaps[1].size(), 3 );
   WALBERLA_ASSERT_EQUAL( faceLocalVertexToCellLocalVertexMaps[2].size(), 3 );
   WALBERLA_ASSERT_EQUAL( faceLocalVertexToCellLocalVertexMaps[3].size(), 3 );

   neighborVertices_.assign( vertexIDs.begin(), vertexIDs.end() );
   neighborEdges_.assign( edgeIDs.begin(), edgeIDs.end() );
   neighborFaces_.assign( faceIDs.begin(), faceIDs.end() );

   faceInwardNormals_[0] = tetrahedronInwardNormal( coordinates_[1], coordinates_[2], coordinates_[3], coordinates_[0] );
   faceInwardNormals_[1] = tetrahedronInwardNormal( coordinates_[0], coordinates_[2], coordinates_[3], coordinates_[1] );
   faceInwardNormals_[2] = tetrahedronInwardNormal( coordinates_[1], coordinates_[0], coordinates_[3], coordinates_[2] );
   faceInwardNormals_[3] = tetrahedronInwardNormal( coordinates_[1], coordinates_[2], coordinates_[0], coordinates_[3] );
}

uint_t Cell::getLocalFaceID( const PrimitiveID& faceID ) const
{
   WALBERLA_ASSERT( neighborPrimitiveExists( faceID ) );
   for ( uint_t localFaceID = 0; localFaceID < 4; localFaceID++ )
   {
      if ( neighborFaces_[localFaceID] == faceID )
      {
         return localFaceID;
      }
   }
   return std::numeric_limits< uint_t >::max();
}

uint_t Cell::getLocalEdgeID( const PrimitiveID& edgeID ) const
{
   WALBERLA_ASSERT( neighborPrimitiveExists( edgeID ) );
   for ( uint_t localEdgeID = 0; localEdgeID < 6; localEdgeID++ )
   {
      if ( neighborEdges_[localEdgeID] == edgeID )
      {
         return localEdgeID;
      }
   }
   return std::numeric_limits< uint_t >::max();
}

uint_t Cell::getLocalVertexID( const PrimitiveID& vertexID ) const
{
   WALBERLA_ASSERT( neighborPrimitiveExists( vertexID ) );
   for ( uint_t localVertexID = 0; localVertexID < getNumNeighborVertices(); localVertexID++ )
   {
      if ( neighborVertices_[localVertexID] == vertexID )
      {
         return localVertexID;
      }
   }
   return std::numeric_limits< uint_t >::max();
}

void Cell::serializeSubclass( walberla::mpi::SendBuffer& sendBuffer ) const
{
   sendBuffer << coordinates_;
   sendBuffer << edgeLocalVertexToCellLocalVertexMaps_;
   sendBuffer << faceLocalVertexToCellLocalVertexMaps_;
   sendBuffer << faceInwardNormals_;
   sendBuffer << indirectNeighborCellIDsOverVertices_;
   sendBuffer << indirectNeighborCellIDsOverFaces_;
}

void Cell::deserializeSubclass( walberla::mpi::RecvBuffer& recvBuffer )
{
   recvBuffer >> coordinates_;
   recvBuffer >> edgeLocalVertexToCellLocalVertexMaps_;
   recvBuffer >> faceLocalVertexToCellLocalVertexMaps_;
   recvBuffer >> faceInwardNormals_;
   recvBuffer >> indirectNeighborCellIDsOverVertices_;
   recvBuffer >> indirectNeighborCellIDsOverFaces_;
}

real_t Cell::getVolume() const
{
   Matrix4r mat;
   for ( int rowIdx = 0; rowIdx < 4; ++rowIdx )
   {
      for ( int colIdx = 0; colIdx < 3; ++colIdx )
      {
         mat( rowIdx, colIdx ) = coordinates_[static_cast< uint_t >( rowIdx )][colIdx];
      }
      mat( rowIdx, 3 ) = real_c( 1 );
   }

   return std::abs( mat.determinant() ) / real_c( 6 );
}

real_t Cell::getInsphereRadius() const
{
   Point3D a = coordinates_[0];
   Point3D b = coordinates_[1];
   Point3D c = coordinates_[2];
   Point3D d = coordinates_[3];

   real_t lengthNormalABC = ( b - a ).cross( c - a ).norm();
   real_t lengthNormalABD = ( b - a ).cross( d - a ).norm();
   real_t lengthNormalACD = ( c - a ).cross( d - a ).norm();
   real_t lengthNormalBCD = ( c - b ).cross( d - b ).norm();

   return real_c( 6 ) * getVolume() / ( lengthNormalABC + lengthNormalABD + lengthNormalACD + lengthNormalBCD );
}

} // namespace hyteg
