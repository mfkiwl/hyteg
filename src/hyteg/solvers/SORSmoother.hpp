#pragma once

#include "hyteg/solvers/Solver.hpp"
#include "core/DataTypes.h"

namespace hyteg {

template < class OperatorType >
class SORSmoother : public Solver< OperatorType >
{
 public:
   SORSmoother( const real_t & relax )
   : relax_( relax )
   , flag_( hyteg::Inner | hyteg::NeumannBoundary )
   {}

   void solve( const OperatorType&                   A,
               const typename OperatorType::srcType& x,
               const typename OperatorType::dstType& b,
               const walberla::uint_t                level ) override
   {
      A.smooth_sor( x, b, relax_, level, flag_ );
   }

 private:
   real_t  relax_;
   DoFType flag_;
};

} // namespace hyteg