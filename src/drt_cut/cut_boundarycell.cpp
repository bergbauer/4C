
#include "cut_boundarycell.H"
#include "cut_volumecell.H"
#include "cut_facet.H"


void GEO::CUT::Tri3BoundaryCell::CreateCell( Mesh & mesh, VolumeCell * cell, Facet * facet, const std::vector<Point*> & side )
{
  cell->NewTri3Cell( mesh, facet, side );
}

void GEO::CUT::Quad4BoundaryCell::CreateCell( Mesh & mesh, VolumeCell * cell, Facet * facet, const std::vector<Point*> & side )
{
  cell->NewQuad4Cell( mesh, facet, side );
}

#if 0
void GEO::CUT::Tri3BoundaryCell::CollectCoordinates( const std::vector<Point*> & side,
                                                     std::map<Facet*, std::vector<Epetra_SerialDenseMatrix> > & sides_xyz )
{
  if ( side.size()!=3 )
  {
    throw std::runtime_error( "wrong number of points" );
  }

  std::set<Facet*> facets;
  FindCommonFacets( side[0], side[1], side[2], facets );

  Facet * f;
  if ( facets.size()==1 )
  {
    f = *facets.begin();
  }
  else
  {
    Facet * found = NULL;
    for ( std::set<Facet*>::iterator i=facets.begin(); i!=facets.end(); ++i )
    {
      Facet * f = *i;
      if ( f->IsTriangle( side ) )
      {
        if ( found==NULL )
        {
          found = f;
        }
        else
        {
          throw std::runtime_error( "not unique" );
        }
      }
    }

    if ( found==NULL )
    {
      throw std::runtime_error( "not a valid cut side" );
    }

    f = found;
  }

  if ( f->SideId() < 0 )
  {
    return;
  }

  std::vector<Epetra_SerialDenseMatrix> & side_xyz = sides_xyz[f];
  side_xyz.push_back( Epetra_SerialDenseMatrix( 3, 3 ) );
  Epetra_SerialDenseMatrix & xyz = side_xyz.back();

  for ( int j=0; j<3; ++j )
  {
    Point * p = side[j];
    p->Coordinates( &xyz( 0, j ) );
  }
}

void GEO::CUT::Quad4BoundaryCell::CollectCoordinates( const std::vector<Point*> & side,
                                                      std::map<Facet*, std::vector<Epetra_SerialDenseMatrix> > & sides_xyz )
{
  if ( side.size()!=4 )
  {
    throw std::runtime_error( "wrong number of points" );
  }

  std::set<Facet*> facets;
  FindCommonFacets( side[0], side[1], side[2], side[3], facets );

  if ( facets.size()!=1 )
  {
    throw std::runtime_error( "not a valid cut side" );
  }

  Facet * f = *facets.begin();
  if ( f->SideId() < 0 )
  {
    return;
  }

  std::vector<Epetra_SerialDenseMatrix> & side_xyz = sides_xyz[f];
  side_xyz.push_back( Epetra_SerialDenseMatrix( 3, 4 ) );
  Epetra_SerialDenseMatrix & xyz = side_xyz.back();

  for ( int j=0; j<4; ++j )
  {
    Point * p = side[j];
    p->Coordinates( &xyz( 0, j ) );
  }
}
#endif

double GEO::CUT::Tri3BoundaryCell::Area() const
{
  throw std::runtime_error( "" );
}

double GEO::CUT::Quad4BoundaryCell::Area() const
{
  throw std::runtime_error( "" );
}

void GEO::CUT::Tri3BoundaryCell::DumpGmsh( std::ofstream & file )
{
  file << "ST(";
  for ( int i=0; i<3; ++i )
  {
    if ( i > 0 )
      file << ",";
    file << xyz_( 0, i ) << ","
         << xyz_( 1, i ) << ","
         << xyz_( 2, i );
  }
  file << "){";
  for ( int i=0; i<3; ++i )
  {
    if ( i > 0 )
      file << ",";
    file << -1;
  }
  file << "};\n";
}

void GEO::CUT::Quad4BoundaryCell::DumpGmsh( std::ofstream & file )
{
  file << "SQ(";
  for ( int i=0; i<4; ++i )
  {
    if ( i > 0 )
      file << ",";
    file << xyz_( 0, i ) << ","
         << xyz_( 1, i ) << ","
         << xyz_( 2, i );
  }
  file << "){";
  for ( int i=0; i<4; ++i )
  {
    if ( i > 0 )
      file << ",";
    file << -1;
  }
  file << "};\n";
}
