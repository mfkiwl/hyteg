/*
 * Copyright (c) 2017-2019 Marcus Mohr.
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
#pragma once

#include "hyteg/celldofspace/CellDoFIndexing.hpp"
#include "hyteg/communication/Syncing.hpp"
#include "hyteg/forms/form_fenics_base/P1FenicsForm.hpp"
#include "hyteg/forms/form_hyteg_generated/P1FormMass.hpp"
#include "hyteg/p1functionspace/P1Elements.hpp"
#include "hyteg/p1functionspace/P1Function.hpp"
#include "hyteg/p1functionspace/VertexDoFMacroFace.hpp"

namespace hyteg {

using walberla::real_t;

template < class P1Form >
class P1ElementwiseOperator : public Operator< P1Function< real_t >, P1Function< real_t > >
{
 public:
   P1ElementwiseOperator( const std::shared_ptr< PrimitiveStorage >& storage,
                          size_t                                     minLevel,
                          size_t                                     maxLevel,
                          bool                                       needsDiagEntries = true );

   void apply( const P1Function< real_t >& src,
               const P1Function< real_t >& dst,
               size_t                      level,
               DoFType                     flag,
               UpdateType                  updateType = Replace ) const;

   std::unique_ptr< P1Function< real_t > > diagonalValues_;

   void smooth_jac( const P1Function< real_t >& dst,
                    const P1Function< real_t >& rhs,
                    const P1Function< real_t >& src,
                    real_t                      omega,
                    size_t                      level,
                    DoFType                     flag ) const;

#ifdef HYTEG_BUILD_WITH_PETSC
   /// Assemble operator as sparse matrix for PETSc
   ///
   /// \param mat   PETSc's own matrix data structure
   /// \param src   P2Function for determining column indices
   /// \param dst   P2Function for determining row indices
   /// \param level level in mesh hierarchy for which local operator is to be assembled
   /// \param flag  ignored
   ///
   /// \note src and dst are legal to and often will be the same function object
   void assembleLocalMatrix( Mat&                          mat,
                             const P1Function< PetscInt >& src,
                             const P1Function< PetscInt >& dst,
                             uint_t                        level,
                             DoFType                       flag ) const;
#endif

   /// Trigger (re)computation of diagonal matrix entries (central operator weights)
   ///
   /// \param level   grid level on which the computation should be run
   /// \param invert  flag, if true, inverse values will be computed and stored
   void computeDiagonalOperatorValues( uint_t level, bool invert );

 private:
   /// compute product of element local vector with element matrix
   ///
   /// \param face           face primitive we operate on
   /// \param level          level on which we operate in mesh hierarchy
   /// \param xIdx           column index of vertex specifying micro-element
   /// \param yIdx           row index of vertex specifying micro-element
   /// \param element        element specification w.r.t. to micro-vertex
   /// \param srcVertexData  pointer to DoF data on micro-vertices (for reading data)
   /// \param dstVertexData  pointer to DoF data on micro-vertices (for writing data)
   ///
   /// \note The src and dst data arrays must not be identical.
   void localMatrixVectorMultiply2D( const Face&                                face,
                                     const uint_t                               level,
                                     const uint_t                               xIdx,
                                     const uint_t                               yIdx,
                                     const P1Elements::P1Elements2D::P1Element& element,
                                     const real_t* const                        srcVertexData,
                                     real_t* const                              dstVertexData ) const;

   /// compute product of element local vector with element matrix
   ///
   /// \param cell           cell primitive we operate on
   /// \param level          level on which we operate in mesh hierarchy
   /// \param microCell      index associated with the current element = micro-cell
   /// \param cType          type of micro-cell (WHITE_UP, BLUE_DOWN, ...)
   /// \param srcVertexData  pointer to DoF data on micro-vertices (for reading data)
   /// \param dstVertexData  pointer to DoF data on micro-vertices (for writing data)
   ///
   /// \note The src and dst data arrays must not be identical.
   void localMatrixVectorMultiply3D( const Cell&             cell,
                                     const uint_t            level,
                                     const indexing::Index&  microCell,
                                     const celldof::CellType cType,
                                     const real_t* const     srcVertexData,
                                     real_t* const           dstVertexData ) const;

   /// Compute contributions to operator diagonal for given micro-face
   ///
   /// \param face           face primitive we operate on
   /// \param level          level on which we operate in mesh hierarchy
   /// \param xIdx           column index of vertex specifying micro-element
   /// \param yIdx           row index of vertex specifying micro-element
   /// \param element        element specification w.r.t. to micro-vertex
   /// \param dstVertexData  pointer to DoF data on micro-vertices (for writing data)
   void computeLocalDiagonalContributions2D( const Face&                                face,
                                             const uint_t                               level,
                                             const uint_t                               xIdx,
                                             const uint_t                               yIdx,
                                             const P1Elements::P1Elements2D::P1Element& element,
                                             real_t* const                              dstVertexData );

   /// Compute contributions to operator diagonal for given micro-face
   ///
   /// \param cell           cell primitive we operate on
   /// \param level          level on which we operate in mesh hierarchy
   /// \param microCell      index associated with the current element = micro-cell
   /// \param cType          type of micro-cell (WHITE_UP, BLUE_DOWN, ...)
   /// \param vertexData     pointer to DoF data for storing diagonal values for vertex DoF stencils
   void computeLocalDiagonalContributions3D( const Cell&             cell,
                                             const uint_t            level,
                                             const indexing::Index&  microCell,
                                             const celldof::CellType cType,
                                             real_t* const           vertexData );

#ifdef HYTEG_BUILD_WITH_PETSC
   void localMatrixAssembly2D( Mat&                                       mat,
                               const Face&                                face,
                               const uint_t                               level,
                               const uint_t                               xIdx,
                               const uint_t                               yIdx,
                               const P1Elements::P1Elements2D::P1Element& element,
                               const PetscInt* const                      srcIdx,
                               const PetscInt* const                      dstIdx ) const;

   void localMatrixAssembly3D( Mat&                    mat,
                               const Cell&             cell,
                               const uint_t            level,
                               const indexing::Index&  microCell,
                               const celldof::CellType cType,
                               const PetscInt* const   srcIdx,
                               const PetscInt* const   dstIdx ) const;
#endif
};

typedef P1ElementwiseOperator<
    P1FenicsForm< p1_diffusion_cell_integral_0_otherwise, p1_tet_diffusion_cell_integral_0_otherwise > >
    P1ElementwiseLaplaceOperator;

typedef P1ElementwiseOperator< P1FenicsForm< p1_mass_cell_integral_0_otherwise, p1_tet_mass_cell_integral_0_otherwise > >
    P1ElementwiseMassOperator;

typedef P1ElementwiseOperator< P1Form_mass > P1ElementwiseBlendingMassOperator;

} // namespace hyteg