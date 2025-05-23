
###################################################################################################
#
# Module rigid particle dynamics (RPD)
#
###################################################################################################

add_library( unresolved_particles )
target_link_libraries( unresolved_particles PUBLIC blockforest core stencil vtk )
target_include_directories( unresolved_particles PUBLIC ${hyteg_SOURCE_DIR}/src/ ${hyteg_BINARY_DIR}/src/ )
add_subdirectory( collision_detection )
add_subdirectory( common )
add_subdirectory( data )
add_subdirectory( domain )
add_subdirectory( kernel )
add_subdirectory( mpi )
add_subdirectory( sorting )
add_subdirectory( vtk )
if(OPENMESH_CORE_FOUND)
    set( WALBERLA_MESAPD_CONVEX_POLYHEDRON_AVAILABLE ON CACHE INTERNAL "")
    target_link_libraries( unresolved_particles PUBLIC mesh_common )
    message( DEBUG "MESA-PD: ConvexPolyhedron shape is available (OpenMesh dependency satisfied)" )
else()
    set( WALBERLA_MESAPD_CONVEX_POLYHEDRON_AVAILABLE OFF CACHE INTERNAL "")
    message( DEBUG "MESA-PD: ConvexPolyhedron shape is unavailable (OpenMesh not found)" )
endif()