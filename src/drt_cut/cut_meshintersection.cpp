
#include "cut_meshintersection.H"

void GEO::CUT::MeshIntersection::AddElement( int eid,
                                             const std::vector<int> & nids,
                                             const Epetra_SerialDenseMatrix & xyz,
                                             DRT::Element::DiscretizationType distype )
{
  for ( std::vector<Teuchos::RCP<MeshHandle> >::iterator i=cut_mesh_.begin();
        i!=cut_mesh_.end();
        ++i )
  {
    MeshHandle & cut_mesh_handle = **i;
    Mesh & cut_mesh = cut_mesh_handle.LinearMesh();
    if ( cut_mesh.WithinBB( xyz ) )
    {
      int numnode = nids.size();
      if ( numnode != xyz.N() )
      {
        throw std::runtime_error( "node coordiante number mismatch" );
      }

      // make sure all nodes are there
      for ( int i=0; i<numnode; ++i )
      {
        NormalMesh().GetNode( nids[i], &xyz( 0, i ) );
//         if ( n==NULL )
//         {
//           // if there is no node with that id but a node at the given
//           // location, the element is illegal and cannot be created
//           return;
//         }
      }

      // create element
      mesh_.CreateElement( eid, nids, distype );

      return;
    }
  }
}

void GEO::CUT::MeshIntersection::AddCutSide( int sid,
                                             const std::vector<int> & nids,
                                             DRT::Element::DiscretizationType distype,
                                             int mi )
{
  // create side
  cut_mesh_[mi]->CreateSide( sid, nids, distype );
}

void GEO::CUT::MeshIntersection::AddCutSide( int sid,
                                             const std::vector<int> & nids,
                                             const Epetra_SerialDenseMatrix & xyz,
                                             DRT::Element::DiscretizationType distype,
                                             int mi )
{
  Mesh & cut_mesh = CutMesh( mi );

  int numnode = nids.size();
  if ( numnode != xyz.N() )
  {
    throw std::runtime_error( "node coordiante number mismatch" );
  }

//   std::set<Point*> nodalpoints;

  // make sure all nodes are there
  for ( int i=0; i<numnode; ++i )
  {
    cut_mesh.GetNode( nids[i], &xyz( 0, i ) );
//     nodalpoints.insert( n->point() );
//     if ( n==NULL )
//     {
//       // if there is no node with that id but a node at the given location,
//       // the side is illegal and cannot be created
//       return;
//     }
  }

//   // do not create degenerated cut sides
//   if ( nodalpoints.size() < nids.size() )
//     return;

  // create side
  cut_mesh_[mi]->CreateSide( sid, nids, distype );
}

void GEO::CUT::MeshIntersection::Cut( bool include_inner )
{
  Status();

//   std::vector<Teuchos::RCP<CellGenerator> > cutgens;
//   for ( int i=cut_mesh_.size()-1; i>0; --i )
//   {
//     cutgens.push_back( Teuchos::rcp( new TetCutGenerator( *this, generator, CutMesh( i ), pp_ ) ) );
//     generator = &*cutgens.back();
//   }

  std::set<Element*> elements_done;

  // loop cut sides and cut against elements at the same position in space
  for ( std::vector<Teuchos::RCP<MeshHandle> >::iterator i=cut_mesh_.begin();
        i!=cut_mesh_.end();
        ++i )
  {
    MeshHandle & cut_mesh_handle = **i;
    Mesh & cut_mesh = cut_mesh_handle.LinearMesh();
    cut_mesh.Cut( NormalMesh(), elements_done );
  }

  NormalMesh().MakeFacets();
  NormalMesh().MakeVolumeCells();

  // find inside and outside positions of nodes
  NormalMesh().FindNodePositions();

  // find number and connection of dofsets at nodes from cut volumes
  NormalMesh().FindNodalDOFSets( include_inner );

  //Status();

  NormalMesh().CreateIntegrationCells();

#ifdef DEBUGCUTLIBRARY
  NormalMesh().TestElementVolume();
#endif

  Status();
}

GEO::CUT::Node * GEO::CUT::MeshIntersection::GetNode( int nid ) const
{
  return mesh_.GetNode( nid );
}

GEO::CUT::ElementHandle * GEO::CUT::MeshIntersection::GetElement( int eid ) const
{
  return mesh_.GetElement( eid );
}

GEO::CUT::SideHandle * GEO::CUT::MeshIntersection::GetCutSide( int sid, int mi ) const
{
  SideHandle * cut_side = cut_mesh_[mi]->GetSide( sid );
  if ( cut_side!=NULL )
  {
    return cut_side;
  }
  throw std::runtime_error( "no such side" );
}

void GEO::CUT::MeshIntersection::Status()
{
#ifdef DEBUG
  NormalMesh().Status();
  for ( std::vector<Teuchos::RCP<MeshHandle> >::iterator i=cut_mesh_.begin();
        i!=cut_mesh_.end();
        ++i )
  {
    MeshHandle & cut_mesh_handle = **i;
    Mesh & cut_mesh = cut_mesh_handle.LinearMesh();
    cut_mesh.Status();
  }

#ifdef DEBUGCUTLIBRARY
  NormalMesh().DumpGmsh( "mesh.pos" );
  int count = 0;
  for ( std::vector<Teuchos::RCP<MeshHandle> >::iterator i=cut_mesh_.begin();
        i!=cut_mesh_.end();
        ++i )
  {
    MeshHandle & cut_mesh_handle = **i;
    Mesh & cut_mesh = cut_mesh_handle.LinearMesh();
    std::stringstream str;
    str << "cut_mesh" << count << ".pos";
    cut_mesh.DumpGmsh( str.str().c_str() );
    count++;
  }

  NormalMesh().DumpGmshIntegrationcells( "integrationcells.pos" );
#endif
#endif
}
