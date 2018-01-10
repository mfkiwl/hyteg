
#pragma once

#include "core/debug/Debug.h"
#include "tinyhhg_core/indexing/Common.hpp"

namespace hhg {
namespace indexing {

using walberla::uint_t;

namespace layout {

/// Required memory for the linear macro edge layout
template< uint_t width >
constexpr uint_t linearMacroEdgeSize()
{
  return width;
}

/// General linear memory layout indexing function for macro edges
template< uint_t width >
constexpr uint_t linearMacroEdgeIndex( const uint_t & col )
{
  return col;
}

}

template< uint_t width >
constexpr uint_t macroEdgeSize()
{
  return layout::linearMacroEdgeSize< width >();
}

template< uint_t width >
constexpr uint_t macroEdgeIndex( const uint_t & col )
{
  return layout::linearMacroEdgeIndex< width >( col );
}

/// Iterator over an edge.
/// Does not include ghost layers!
/// It is possible to parameterize the iterator to only iterate over a inner part of the edge.
/// This is done by setting the offset parameter to the distance to the vertices.
/// If set to zero, the iterator iterates over the whole edge (including both adjacent vertices).
class EdgeIterator
{
public:

  using iterator_category = std::input_iterator_tag;
  using value_type        = Index;
  using reference         = value_type const&;
  using pointer           = value_type const*;
  using difference_type   = ptrdiff_t;

  EdgeIterator( const uint_t & width, const uint_t & offsetToCenter = 0, const bool & end = false ) :
    width_( width ), offsetToCenter_( offsetToCenter ),
    totalNumberOfDoFs_( width - 2 * offsetToCenter ), step_( 0 )
  {
    WALBERLA_ASSERT_LESS( offsetToCenter, width, "Offset to center is beyond edge width!" );

    coordinates_.dep() = 0;
    coordinates_.row() = 0;

    coordinates_.col() = offsetToCenter;

    if ( end )
    {
      step_ = totalNumberOfDoFs_;
    }
  }

  EdgeIterator begin() { return EdgeIterator( width_, offsetToCenter_ ); }
  EdgeIterator end()   { return EdgeIterator( width_, offsetToCenter_, true ); }

  bool operator==( const EdgeIterator & other ) const
  {
    return other.step_ == step_;
  }

  bool operator!=( const EdgeIterator & other ) const
  {
    return other.step_ != step_;
  }

  reference operator*()  const { return  coordinates_; };
  pointer   operator->() const { return &coordinates_; };

  EdgeIterator & operator++() // prefix
  {
    WALBERLA_ASSERT_LESS_EQUAL( step_, totalNumberOfDoFs_, "Incrementing iterator beyond end!" );

    step_++;
    coordinates_.col()++;

    return *this;
  }

  EdgeIterator operator++( int ) // postfix
  {
    const EdgeIterator tmp( *this );
    ++*this;
    return tmp;
  }


private:

  const uint_t    width_;
  const uint_t    offsetToCenter_;
  const uint_t    totalNumberOfDoFs_;
        uint_t    step_;
        Index     coordinates_;

};
}
}