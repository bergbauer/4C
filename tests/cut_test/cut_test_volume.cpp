
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "cut_test_utils.H"

#include "../../src/drt_cut/cut_options.H"
#include "../../src/drt_cut/cut_mesh.H"

void test_cut_volumes()
{
  GEO::CUT::Options options;

  // this is meant to be used with matching boundaries. Thus, no
  // inside/outside positions.
  options.SetFindPositions( false );

  GEO::CUT::Mesh mesh1( options );
  GEO::CUT::Mesh mesh2( options, 1, mesh1.Points() );

  create_hex8_mesh( mesh1, 4, 4, 4 );
  create_hex8_mesh( mesh2, 3, 5, 2 );

  mesh2.CreateSideIds();

  mesh1.Status();
  mesh2.Status();

  GEO::CUT::plain_element_set elements_done;

  mesh2.Cut( mesh1, elements_done );

  cutmesh( mesh1 );

  mesh2.AssignOtherVolumeCells( mesh1 );
}

void test_cut_volumes2()
{
  GEO::CUT::Options options;

  // this is meant to be used with matching boundaries. Thus, no
  // inside/outside positions.
  options.SetFindPositions( false );

  GEO::CUT::Mesh mesh1( options );
  GEO::CUT::Mesh mesh2( options, 1, mesh1.Points() );

#if 1
  create_hex8_mesh( mesh1, 1, 1, 1 );
  create_hex8_mesh( mesh2, 3, 3, 3 );
//   create_hex8_mesh( mesh1, 1, 1, 1 );
//   create_hex8_mesh( mesh2, 2, 2, 3 );
#else
  create_hex8_mesh( mesh1, 4, 4, 4 );
  create_hex8_mesh( mesh2, 8, 9, 10 );
#endif

  mesh2.CreateSideIds();

  mesh1.Status();
  mesh2.Status();

  GEO::CUT::plain_element_set elements_done;

  mesh2.Cut( mesh1, elements_done );

  cutmesh( mesh1 );

  mesh2.AssignOtherVolumeCells( mesh1 );
}
