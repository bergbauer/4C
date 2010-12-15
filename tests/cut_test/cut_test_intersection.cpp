
#include <iostream>

#include "cut_test_utils.H"

#include "../../src/drt_cut/cut_meshintersection.H"
#include "../../src/drt_fem_general/drt_utils_local_connectivity_matrices.H"

void test_hex8_quad4_mesh_many()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad4_xyze( 3, 4 );

  nids.clear();
  nids.push_back(0);
  quad4_xyze(0,0) = 0.666276;
  quad4_xyze(1,0) = 0.828602;
  quad4_xyze(2,0) = 0.02525;
  nids.push_back(1);
  quad4_xyze(0,1) = 0.673325;
  quad4_xyze(1,1) = 0.847231;
  quad4_xyze(2,1) = 0.02525;
  nids.push_back(2);
  quad4_xyze(0,2) = 0.673325;
  quad4_xyze(1,2) = 0.847231;
  quad4_xyze(2,2) = -0.02525;
  nids.push_back(3);
  quad4_xyze(0,3) = 0.666276;
  quad4_xyze(1,3) = 0.828602;
  quad4_xyze(2,3) = -0.02525;

  intersection.AddCutSide( 1, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(4);
  quad4_xyze(0,0) = 0.680311;
  quad4_xyze(1,0) = 0.86595;
  quad4_xyze(2,0) = 0.02525;
  nids.push_back(5);
  quad4_xyze(0,1) = 0.68731;
  quad4_xyze(1,1) = 0.884695;
  quad4_xyze(2,1) = 0.02525;
  nids.push_back(6);
  quad4_xyze(0,2) = 0.68731;
  quad4_xyze(1,2) = 0.884695;
  quad4_xyze(2,2) = -0.02525;
  nids.push_back(7);
  quad4_xyze(0,3) = 0.680311;
  quad4_xyze(1,3) = 0.86595;
  quad4_xyze(2,3) = -0.02525;

  intersection.AddCutSide( 2, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(1);
  quad4_xyze(0,0) = 0.673325;
  quad4_xyze(1,0) = 0.847231;
  quad4_xyze(2,0) = 0.02525;
  nids.push_back(4);
  quad4_xyze(0,1) = 0.680311;
  quad4_xyze(1,1) = 0.86595;
  quad4_xyze(2,1) = 0.02525;
  nids.push_back(7);
  quad4_xyze(0,2) = 0.680311;
  quad4_xyze(1,2) = 0.86595;
  quad4_xyze(2,2) = -0.02525;
  nids.push_back(2);
  quad4_xyze(0,3) = 0.673325;
  quad4_xyze(1,3) = 0.847231;
  quad4_xyze(2,3) = -0.02525;

  intersection.AddCutSide( 3, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(6);
  quad4_xyze(0,0) = 0.68731;
  quad4_xyze(1,0) = 0.884695;
  quad4_xyze(2,0) = -0.02525;
  nids.push_back(5);
  quad4_xyze(0,1) = 0.68731;
  quad4_xyze(1,1) = 0.884695;
  quad4_xyze(2,1) = 0.02525;
  nids.push_back(8);
  quad4_xyze(0,2) = 0.702923;
  quad4_xyze(1,2) = 0.878831;
  quad4_xyze(2,2) = 0.02525;
  nids.push_back(9);
  quad4_xyze(0,3) = 0.702923;
  quad4_xyze(1,3) = 0.878831;
  quad4_xyze(2,3) = -0.02525;

  intersection.AddCutSide( 4, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(9);
  quad4_xyze(0,0) = 0.702923;
  quad4_xyze(1,0) = 0.878831;
  quad4_xyze(2,0) = -0.02525;
  nids.push_back(8);
  quad4_xyze(0,1) = 0.702923;
  quad4_xyze(1,1) = 0.878831;
  quad4_xyze(2,1) = 0.02525;
  nids.push_back(10);
  quad4_xyze(0,2) = 0.718519;
  quad4_xyze(1,2) = 0.87295;
  quad4_xyze(2,2) = 0.02525;
  nids.push_back(11);
  quad4_xyze(0,3) = 0.718519;
  quad4_xyze(1,3) = 0.87295;
  quad4_xyze(2,3) = -0.02525;

  intersection.AddCutSide( 5, nids, quad4_xyze, DRT::Element::quad4 );

  Epetra_SerialDenseMatrix hex8_xyze( 3, 8 );

  hex8_xyze(0,0) = 0.707126021;
  hex8_xyze(1,0) = 0.885084629;
  hex8_xyze(2,0) = -0.0250000004;
  hex8_xyze(0,1) = 0.669295728;
  hex8_xyze(1,1) = 0.88072747;
  hex8_xyze(2,1) = -0.0250000004;
  hex8_xyze(0,2) = 0.669837534;
  hex8_xyze(1,2) = 0.831363618;
  hex8_xyze(2,2) = -0.0250000004;
  hex8_xyze(0,3) = 0.713419497;
  hex8_xyze(1,3) = 0.846297681;
  hex8_xyze(2,3) = -0.0250000004;
  hex8_xyze(0,4) = 0.707126021;
  hex8_xyze(1,4) = 0.885084629;
  hex8_xyze(2,4) = 0.0250000004;
  hex8_xyze(0,5) = 0.669295728;
  hex8_xyze(1,5) = 0.88072747;
  hex8_xyze(2,5) = 0.0250000004;
  hex8_xyze(0,6) = 0.669837534;
  hex8_xyze(1,6) = 0.831363618;
  hex8_xyze(2,6) = 0.0250000004;
  hex8_xyze(0,7) = 0.713419497;
  hex8_xyze(1,7) = 0.846297681;
  hex8_xyze(2,7) = 0.0250000004;

  nids.clear();
  for ( int i=0; i<8; ++i )
    nids.push_back( i );

  intersection.AddElement( 1, nids, hex8_xyze, DRT::Element::hex8 );

  intersection.Cut();
}

void test_hex8_quad4_mesh_edgecut()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad4_xyze( 3, 4 );

  nids.clear();
  nids.push_back(0);
  quad4_xyze(0,0) = 0.5399141482156006866;
  quad4_xyze(1,0) = 0.77952300801931839747;
  quad4_xyze(2,0) = 0.025250000899999999748;
  nids.push_back(1);
  quad4_xyze(0,1) = 0.53991414821296357385;
  quad4_xyze(1,1) = 0.77952300803036944643;
  quad4_xyze(2,1) = -0.025250000899999999748;
  nids.push_back(2);
  quad4_xyze(0,2) = 0.5400635612166613253;
  quad4_xyze(1,2) = 0.79951197365630055636;
  quad4_xyze(2,2) = -0.025250000899999999748;
  nids.push_back(3);
  quad4_xyze(0,3) = 0.54006356122364373995;
  quad4_xyze(1,3) = 0.79951197365214676793;
  quad4_xyze(2,3) = 0.025250000899999999748;

  intersection.AddCutSide( 1, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(4);
  quad4_xyze(0,0) = 0.53955076912075083939;
  quad4_xyze(1,0) = 0.73952115635249415782;
  quad4_xyze(2,0) = 0.025250000899999999748;
  nids.push_back(5);
  quad4_xyze(0,1) = 0.53955076912314114956;
  quad4_xyze(1,1) = 0.73952115637886628452;
  quad4_xyze(2,1) = -0.025250000899999999748;
  nids.push_back(6);
  quad4_xyze(0,2) = 0.53974580830286555955;
  quad4_xyze(1,2) = 0.75952575976383629452;
  quad4_xyze(2,2) = -0.025250000899999999748;
  nids.push_back(7);
  quad4_xyze(0,3) = 0.53974580831400054137;
  quad4_xyze(1,3) = 0.75952575975287395238;
  quad4_xyze(2,3) = 0.025250000899999999748;

  intersection.AddCutSide( 2, nids, quad4_xyze, DRT::Element::quad4 );

  nids.clear();
  nids.push_back(7);
  quad4_xyze(0,0) = 0.53974580831400054137;
  quad4_xyze(1,0) = 0.75952575975287395238;
  quad4_xyze(2,0) = 0.025250000899999999748;
  nids.push_back(6);
  quad4_xyze(0,1) = 0.53974580830286555955;
  quad4_xyze(1,1) = 0.75952575976383629452;
  quad4_xyze(2,1) = -0.025250000899999999748;
  nids.push_back(1);
  quad4_xyze(0,2) = 0.53991414821296357385;
  quad4_xyze(1,2) = 0.77952300803036944643;
  quad4_xyze(2,2) = -0.025250000899999999748;
  nids.push_back(0);
  quad4_xyze(0,3) = 0.5399141482156006866;
  quad4_xyze(1,3) = 0.77952300801931839747;
  quad4_xyze(2,3) = 0.025250000899999999748;

  intersection.AddCutSide( 3, nids, quad4_xyze, DRT::Element::quad4 );

  Epetra_SerialDenseMatrix hex8_xyze( 3, 8 );

  hex8_xyze(0,0) = 0.53135645399999997807;
  hex8_xyze(1,0) = 0.75096273400000002063;
  hex8_xyze(2,0) = -0.025000000399999999789;
  hex8_xyze(0,1) = 0.56228893999999995934;
  hex8_xyze(1,1) = 0.74424672099999999997;
  hex8_xyze(2,1) = -0.025000000399999999789;
  hex8_xyze(0,2) = 0.57124853099999994832;
  hex8_xyze(1,2) = 0.7783957719999999858;
  hex8_xyze(2,2) = -0.025000000399999999789;
  hex8_xyze(0,3) = 0.53994804600000001482;
  hex8_xyze(1,3) = 0.78398263499999998327;
  hex8_xyze(2,3) = -0.025000000399999999789;
  hex8_xyze(0,4) = 0.53135645399999997807;
  hex8_xyze(1,4) = 0.75096273400000002063;
  hex8_xyze(2,4) = 0.025000000399999999789;
  hex8_xyze(0,5) = 0.56228893999999995934;
  hex8_xyze(1,5) = 0.74424672099999999997;
  hex8_xyze(2,5) = 0.025000000399999999789;
  hex8_xyze(0,6) = 0.57124853099999994832;
  hex8_xyze(1,6) = 0.7783957719999999858;
  hex8_xyze(2,6) = 0.025000000399999999789;
  hex8_xyze(0,7) = 0.53994804600000001482;
  hex8_xyze(1,7) = 0.78398263499999998327;
  hex8_xyze(2,7) = 0.025000000399999999789;

  nids.clear();
  for ( int i=0; i<8; ++i )
    nids.push_back( i );

  intersection.AddElement( 1, nids, hex8_xyze, DRT::Element::hex8 );

  intersection.Cut();
}

void test_hex8_quad4_mesh_edgecut2()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad4_xyze( 3, 4 );

  nids.clear();
  nids.push_back(0);
  quad4_xyze(0,0) = 0.53991414821573446847;
  quad4_xyze(1,0) = 0.7795230080202141254;
  quad4_xyze(2,0) = 0.025250000899999999748;
  nids.push_back(1);
  quad4_xyze(0,1) = 0.53991414821143801639;
  quad4_xyze(1,1) = 0.77952300802897134258;
  quad4_xyze(2,1) = -0.025250000899999999748;
  nids.push_back(2);
  quad4_xyze(0,2) = 0.54006356121681575733;
  quad4_xyze(1,2) = 0.79951197365563708708;
  quad4_xyze(2,2) = -0.025250000899999999748;
  nids.push_back(3);
  quad4_xyze(0,3) = 0.54006356122155441124;
  quad4_xyze(1,3) = 0.79951197365235482373;
  quad4_xyze(2,3) = 0.025250000899999999748;

  intersection.AddCutSide( 1, nids, quad4_xyze, DRT::Element::quad4 );

  Epetra_SerialDenseMatrix hex8_xyze( 3, 8 );

  hex8_xyze(0,0) = 0.53135645399999997807;
  hex8_xyze(1,0) = 0.75096273400000002063;
  hex8_xyze(2,0) = -0.025000000399999999789;
  hex8_xyze(0,1) = 0.53994804600000001482;
  hex8_xyze(1,1) = 0.78398263499999998327;
  hex8_xyze(2,1) = -0.025000000399999999789;
  hex8_xyze(0,2) = 0.50934117999999994897;
  hex8_xyze(1,2) = 0.78892862799999996515;
  hex8_xyze(2,2) = -0.025000000399999999789;
  hex8_xyze(0,3) = 0.50265735400000000066;
  hex8_xyze(1,3) = 0.75846225000000000449;
  hex8_xyze(2,3) = -0.025000000399999999789;
  hex8_xyze(0,4) = 0.53135645399999997807;
  hex8_xyze(1,4) = 0.75096273400000002063;
  hex8_xyze(2,4) = 0.025000000399999999789;
  hex8_xyze(0,5) = 0.53994804600000001482;
  hex8_xyze(1,5) = 0.78398263499999998327;
  hex8_xyze(2,5) = 0.025000000399999999789;
  hex8_xyze(0,6) = 0.50934117999999994897;
  hex8_xyze(1,6) = 0.78892862799999996515;
  hex8_xyze(2,6) = 0.025000000399999999789;
  hex8_xyze(0,7) = 0.50265735400000000066;
  hex8_xyze(1,7) = 0.75846225000000000449;
  hex8_xyze(2,7) = 0.025000000399999999789;

  nids.clear();
  for ( int i=0; i<8; ++i )
    nids.push_back( i );

  intersection.AddElement( 1, nids, hex8_xyze, DRT::Element::hex8 );

  intersection.Cut();
}

void test_hex8_quad4_mesh_inner()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad4_xyze( 3, 4 );

  nids.clear();
  nids.push_back(0);
  quad4_xyze(0,0) = 0.162023;
  quad4_xyze(1,0) = -0.0373312;
  quad4_xyze(2,0) = 0.0413739;
  nids.push_back(1);
  quad4_xyze(0,1) = 0.13655;
  quad4_xyze(1,1) = -0.0086656;
  quad4_xyze(2,1) = -0.00797866;
  nids.push_back(2);
  quad4_xyze(0,2) = 0.149697;
  quad4_xyze(1,2) = 0.046873;
  quad4_xyze(2,2) = 0.0174941;
  nids.push_back(3);
  quad4_xyze(0,3) = 0.17517;
  quad4_xyze(1,3) = 0.0182074;
  quad4_xyze(2,3) = 0.0668466;

  intersection.AddCutSide( 1, nids, quad4_xyze, DRT::Element::quad4 );

  Epetra_SerialDenseMatrix hex8_xyze( 3, 8 );

  hex8_xyze(0,0) = 0.0833333;
  hex8_xyze(1,0) = -0.0833333;
  hex8_xyze(2,0) = 0.0833333;
  hex8_xyze(0,1) = 0.0833333;
  hex8_xyze(1,1) = -0.0833333;
  hex8_xyze(2,1) = 1.38778e-17;
  hex8_xyze(0,2) = 0.0833333;
  hex8_xyze(1,2) = -3.46945e-18;
  hex8_xyze(2,2) = 1.38778e-17;
  hex8_xyze(0,3) = 0.0833333;
  hex8_xyze(1,3) = -6.93889e-18;
  hex8_xyze(2,3) = 0.0833333;
  hex8_xyze(0,4) = 0.166667;
  hex8_xyze(1,4) = -0.0833333;
  hex8_xyze(2,4) = 0.0833333;
  hex8_xyze(0,5) = 0.166667;
  hex8_xyze(1,5) = -0.0833333;
  hex8_xyze(2,5) = 1.38778e-17;
  hex8_xyze(0,6) = 0.166667;
  hex8_xyze(1,6) = -1.73472e-18;
  hex8_xyze(2,6) = 1.38778e-17;
  hex8_xyze(0,7) = 0.166667;
  hex8_xyze(1,7) = 0;
  hex8_xyze(2,7) = 0.0833333;

  nids.clear();
  for ( int i=0; i<8; ++i )
    nids.push_back( i );

  intersection.AddElement( 1, nids, hex8_xyze, DRT::Element::hex8 );

  intersection.Cut();
}

void test_hex27_quad9_simple()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad9_xyze( 3, 9 );
  Epetra_SerialDenseMatrix hex27_xyze( 3, 27 );

  nids.reserve( 9 );
  for ( int i=0; i<9; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<2; ++j )
    {
      quad9_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_quad9_nodes_reference[i][j];
    }
    quad9_xyze( 2, i ) = 0;
  }

  intersection.AddCutSide( 1, nids, quad9_xyze, DRT::Element::quad9 );

  nids.clear();
  nids.reserve( 27 );
  for ( int i=0; i<27; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<3; ++j )
    {
      hex27_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_hex27_nodes_reference[i][j];
    }
  }

  // shrink hex27 element

  for ( int i=0; i<27; ++i )
  {
    for ( int j=0; j<3; ++j )
    {
      hex27_xyze( j, i ) *= 0.5;
    }
  }

  intersection.AddElement( 1, nids, hex27_xyze, DRT::Element::hex27 );

  intersection.Cut();
}

void test_hex20_quad9_simple()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad9_xyze( 3, 9 );
  Epetra_SerialDenseMatrix hex20_xyze( 3, 20 );

  nids.reserve( 9 );
  for ( int i=0; i<9; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<2; ++j )
    {
      quad9_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_quad9_nodes_reference[i][j];
    }
    quad9_xyze( 2, i ) = 0;
  }

  intersection.AddCutSide( 1, nids, quad9_xyze, DRT::Element::quad9 );

  nids.clear();
  nids.reserve( 20 );
  for ( int i=0; i<20; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<3; ++j )
    {
      hex20_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_hex27_nodes_reference[i][j];
    }
  }

  // shrink hex20 element

  for ( int i=0; i<20; ++i )
  {
    for ( int j=0; j<3; ++j )
    {
      hex20_xyze( j, i ) *= 0.5;
    }
  }

  intersection.AddElement( 1, nids, hex20_xyze, DRT::Element::hex20 );

  intersection.Cut();
}

void test_hex20_quad9_moved()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad9_xyze( 3, 9 );
  Epetra_SerialDenseMatrix hex20_xyze( 3, 20 );

  nids.reserve( 9 );
  for ( int i=0; i<9; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<2; ++j )
    {
      quad9_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_quad9_nodes_reference[i][j];
    }
    quad9_xyze( 2, i ) = 0;
  }

  // move quad9 element

  for ( int i=0; i<9; ++i )
  {
    quad9_xyze( 2, i ) = 0.1 + 0.5*quad9_xyze( 0, i );
    quad9_xyze( 0, i ) += 0.1;
    quad9_xyze( 1, i ) += 0.1;
  }

  intersection.AddCutSide( 1, nids, quad9_xyze, DRT::Element::quad9 );

  nids.clear();
  nids.reserve( 20 );
  for ( int i=0; i<20; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<3; ++j )
    {
      hex20_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_hex27_nodes_reference[i][j];
    }
  }

  // shrink hex20 element

  for ( int i=0; i<20; ++i )
  {
    for ( int j=0; j<3; ++j )
    {
      hex20_xyze( j, i ) *= 0.5;
    }
  }

  intersection.AddElement( 1, nids, hex20_xyze, DRT::Element::hex20 );

  intersection.Cut();
}

void test_tet10_quad9_simple()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad9_xyze( 3, 9 );
  Epetra_SerialDenseMatrix tet10_xyze( 3, 10 );

  nids.reserve( 9 );
  for ( int i=0; i<9; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<2; ++j )
    {
      quad9_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_quad9_nodes_reference[i][j];
    }
    quad9_xyze( 2, i ) = 0.2;
  }

  intersection.AddCutSide( 1, nids, quad9_xyze, DRT::Element::quad9 );

  nids.clear();
  nids.reserve( 10 );
  for ( int i=0; i<10; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<3; ++j )
    {
      tet10_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_tet10_nodes_reference[i][j];
    }
  }

  // shrink tet10 element

  for ( int i=0; i<10; ++i )
  {
    for ( int j=0; j<3; ++j )
    {
      tet10_xyze( j, i ) *= 0.5;
    }
  }

  intersection.AddElement( 1, nids, tet10_xyze, DRT::Element::tet10 );

  intersection.Cut();
}

void test_tet10_quad9_moved()
{
  GEO::CUT::MeshIntersection intersection;

  std::vector<int> nids;

  Epetra_SerialDenseMatrix quad9_xyze( 3, 9 );
  Epetra_SerialDenseMatrix tet10_xyze( 3, 10 );

  nids.reserve( 9 );
  for ( int i=0; i<9; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<2; ++j )
    {
      quad9_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_quad9_nodes_reference[i][j];
    }
    quad9_xyze( 2, i ) = 0;
  }

  // move quad9 element

  for ( int i=0; i<9; ++i )
  {
    quad9_xyze( 2, i ) = 0.1 + 0.5*quad9_xyze( 0, i );
    quad9_xyze( 0, i ) += 0.1;
    quad9_xyze( 1, i ) += 0.1;
  }

  intersection.AddCutSide( 1, nids, quad9_xyze, DRT::Element::quad9 );

  nids.clear();
  nids.reserve( 10 );
  for ( int i=0; i<10; ++i )
  {
    nids.push_back( i );
    for ( int j=0; j<3; ++j )
    {
      tet10_xyze( j, i ) = DRT::UTILS::eleNodeNumbering_tet10_nodes_reference[i][j];
    }
  }

  // shrink tet10 element

  for ( int i=0; i<10; ++i )
  {
    for ( int j=0; j<3; ++j )
    {
      tet10_xyze( j, i ) *= 0.5;
    }
  }

  intersection.AddElement( 1, nids, tet10_xyze, DRT::Element::tet10 );

  intersection.Cut();
}
