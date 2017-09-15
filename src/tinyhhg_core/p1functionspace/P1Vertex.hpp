
#pragma once

#include "tinyhhg_core/levelinfo.hpp"
#include "tinyhhg_core/p1functionspace/P1Memory.hpp"
#include "tinyhhg_core/petsc/PETScWrapper.hpp"

namespace hhg {

namespace P1Vertex {

template< typename ValueType >
inline void interpolate(Vertex &vertex,
                        const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &vertexMemoryId,
                        std::function<ValueType(const hhg::Point3D &)> &expr,
                        size_t level) {
  VertexP1FunctionMemory< ValueType > *vertexMemory = vertex.getData(vertexMemoryId);
  vertexMemory->data[level][0] = expr(vertex.getCoordinates());
}

template< typename ValueType >
inline void assign(Vertex &vertex,
                   const std::vector<ValueType> &scalars,
                   const std::vector<PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex>> &srcIds,
                   const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId,
                   size_t level) {
  ValueType tmp = scalars[0]*vertex.getData(srcIds[0])->data[level][0];

  for (size_t i = 1; i < srcIds.size(); ++i) {
    tmp += scalars[i]*vertex.getData(srcIds[i])->data[level][0];
  }

  vertex.getData(dstId)->data[level][0] = tmp;
}

template< typename ValueType >
inline void add(Vertex &vertex,
                const std::vector<ValueType> &scalars,
                const std::vector<PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex>> &srcIds,
                const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId,
                size_t level) {
  ValueType tmp = 0.0;

  for (size_t i = 0; i < srcIds.size(); ++i) {
    tmp += scalars[i]*vertex.getData(srcIds[i])->data[level][0];
  }

  vertex.getData(dstId)->data[level][0] += tmp;
}

template< typename ValueType >
inline real_t dot(Vertex &vertex,
                  const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &lhsMemoryId,
                  const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &rhsMemoryId,
                  size_t level) {
  return vertex.getData(lhsMemoryId)->data[level][0]*vertex.getData(rhsMemoryId)->data[level][0];
}

template< typename ValueType >
inline void apply(Vertex &vertex,
                  const PrimitiveDataID<VertexP1StencilMemory, Vertex> &operatorId,
                  const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &srcId,
                  const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId,
                  size_t level,
                  UpdateType update) {
  auto &opr_data = vertex.getData(operatorId)->data[level];
  auto &src = vertex.getData(srcId)->data[level];
  auto &dst = vertex.getData(dstId)->data[level];

  if (update==Replace) {
    dst[0] = opr_data[0]*src[0];
  } else if (update==Add) {
    dst[0] += opr_data[0]*src[0];
  }

  for (size_t i = 0; i < vertex.getNumNeighborEdges(); ++i) {
    dst[0] += opr_data[i + 1]*src[i + 1];
  }
}

template< typename ValueType >
inline void smooth_gs(Vertex &vertex, const PrimitiveDataID<VertexP1StencilMemory, Vertex> &operatorId,
                      const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId,
                      const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &rhsId, size_t level) {
  auto &opr_data = vertex.getData(operatorId)->data[level];
  auto &dst = vertex.getData(dstId)->data[level];
  auto &rhs = vertex.getData(rhsId)->data[level];

  dst[0] = rhs[0];

  for (size_t i = 0; i < vertex.getNumNeighborEdges(); ++i) {
    dst[0] -= opr_data[i + 1]*dst[i + 1];
  }

  dst[0] /= opr_data[0];
}

template< typename ValueType >
inline void smooth_jac(Vertex &vertex, const PrimitiveDataID<VertexP1StencilMemory, Vertex> &operatorId,
                      const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId,
                      const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &rhsId,
                      const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &tmpId, size_t level) {
  auto &opr_data = vertex.getData(operatorId)->data[level];
  auto &dst = vertex.getData(dstId)->data[level];
  auto &rhs = vertex.getData(rhsId)->data[level];
  auto &tmp = vertex.getData(tmpId)->data[level];

  dst[0] = rhs[0];

  for (size_t i = 0; i < vertex.getNumNeighborEdges(); ++i) {
    dst[0] -= opr_data[i + 1]*tmp[i + 1];
  }

  dst[0] /= opr_data[0];
}

template< typename ValueType >
inline void prolongate(Vertex &vertex, const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &memoryId, size_t sourceLevel) {
  vertex.getData(memoryId)->data[sourceLevel + 1][0] =
      vertex.getData(memoryId)->data[sourceLevel][0];
}

template< typename ValueType >
inline void prolongateQuadratic(Vertex &vertex,
                                const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &memoryId,
                                size_t level) {
  prolongate(vertex, memoryId, level);
}

template< typename ValueType >
inline void restrict(Vertex &vertex, const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &memoryId, size_t level) {
  auto &vertex_data_f = vertex.getData(memoryId)->data[level];
  auto &vertex_data_c = vertex.getData(memoryId)->data[level - 1];

  vertex_data_c[0] = vertex_data_f[0];

  for (uint_t i = 0; i < vertex.getNumNeighborEdges(); ++i) {
    vertex_data_c[0] += 0.5*vertex_data_f[i+1];
    i += 1;
  }
}

template< typename ValueType >
inline void enumerate(Vertex &vertex, const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &dstId, size_t level, uint_t& num) {
  auto &dst = vertex.getData(dstId)->data[level];
  dst[0] = static_cast< ValueType >( num++ );
}

#ifdef HHG_BUILD_WITH_PETSC
inline void saveOperator(Vertex &vertex,
                         const PrimitiveDataID<VertexP1StencilMemory, Vertex> &operatorId,
                         const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &srcId,
                         const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &dstId,
                         Mat& mat,
                         uint_t level) {
  auto &opr_data = vertex.getData(operatorId)->data[level];
  auto &src = vertex.getData(srcId)->data[level];
  auto &dst = vertex.getData(dstId)->data[level];

  MatSetValues(mat,1,dst.get(),(PetscInt) (vertex.getNumNeighborEdges()+1),src.get(),opr_data.get() ,INSERT_VALUES);
}

template< typename ValueType >
inline void createVectorFromFunction(Vertex &vertex,
                         const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &srcId,
                         const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &numeratorId,
                         Vec& vec,
                         uint_t level) {

  auto &src = vertex.getData(srcId)->data[level];
  PetscInt numerator = vertex.getData(numeratorId)->data[level][0];

  VecSetValues(vec,1,&numerator,src.get(),INSERT_VALUES);

}

template< typename ValueType >
inline void createFunctionFromVector(Vertex &vertex,
                                     const PrimitiveDataID<VertexP1FunctionMemory< ValueType >, Vertex> &srcId,
                                     const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &numeratorId,
                                     Vec& vec,
                                     uint_t level) {


  PetscInt numerator = vertex.getData(numeratorId)->data[level][0];

  VecGetValues(vec,1,&numerator,vertex.getData(srcId)->data[level].get());

}

inline void applyDirichletBC(Vertex &vertex,std::vector<PetscInt> &mat, uint_t level,
                             const PrimitiveDataID<VertexP1FunctionMemory< PetscInt >, Vertex> &numeratorId){

  mat.push_back(vertex.getData(numeratorId)->data[level][0]);

}

#endif


}
}
