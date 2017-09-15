#pragma once

#include "tinyhhg_core/levelinfo.hpp"
#include "tinyhhg_core/macros.hpp"
#include "BubbleToP1Memory.hpp"

namespace hhg {
namespace BubbleToP1Vertex {

inline void apply(Vertex& vertex, const PrimitiveDataID<VertexBubbleToP1StencilMemory, Vertex>& operatorId,
                  const PrimitiveDataID<VertexBubbleFunctionMemory< real_t >, Vertex> &srcId,
                  const PrimitiveDataID<VertexP1FunctionMemory< real_t >, Vertex> &dstId, size_t level, UpdateType update)
{
  auto& opr_data = vertex.getData(operatorId)->data[level];
  auto& src = vertex.getData(srcId)->data[level];
  auto& dst = vertex.getData(dstId)->data[level];

  real_t tmp = 0.0;

  for (size_t i = 0; i < vertex.getNumNeighborFaces(); ++i)
  {
    tmp += opr_data[i] * src[i];
  }

  if (update == Replace) {
    dst[0] = tmp;
  }
  else if (update == Add) {
    dst[0] += tmp;
  }
}

#ifdef HHG_BUILD_WITH_PETSC
inline void saveOperator(Vertex& vertex, const PrimitiveDataID<VertexBubbleToP1StencilMemory, Vertex>& operatorId,
                         const PrimitiveDataID<VertexBubbleFunctionMemory< PetscInt >, Vertex> &srcId,
                         const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &dstId, Mat &mat, size_t level)
{
  auto& opr_data = vertex.getData(operatorId)->data[level];
  auto& src = vertex.getData(srcId)->data[level];
  auto& dst = vertex.getData(dstId)->data[level];

  for (size_t i = 0; i < vertex.getNumNeighborFaces(); ++i)
  {
    MatSetValues(mat, 1, &dst[0], 1, &src[i], &opr_data[i], INSERT_VALUES);
  }
}
#endif

}
}