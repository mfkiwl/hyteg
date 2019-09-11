
//////////////////////////////////////////////////////////////////////////////
// This file is generated! To fix issues, please fix them in the generator. //
//////////////////////////////////////////////////////////////////////////////

#pragma once
#include "all.hpp"
#include "core/Macros.h"
#include "hyteg/edgedofspace/EdgeDoFOrientation.hpp"
#include "hyteg/indexing/Common.hpp"
#include <map>
#define RESTRICT WALBERLA_RESTRICT

namespace hyteg {
namespace vertexdof {
namespace macrocell {
namespace generated {

void apply_3D_macrocell_vertexdof_to_vertexdof_add(double * RESTRICT _data_p1CellDstAdd, double const * RESTRICT const _data_p1CellSrcAdd, int32_t level, std::map< hyteg::indexing::IndexIncrement, double > p1CellStencil);

} // namespace generated
} // namespace macrocell
} // namespace vertexdof
} // namespace hyteg