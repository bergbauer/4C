/*!
\file xdofmapcreation.cpp

\brief defines unknowns based on the intersection pattern from the xfem intersection

this is related to the physics of the fluid problem and therefore should not be part of the standard xfem routines

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>
 */
#ifdef CCADISCRET

#include "xdofmapcreation.H"
#include "enrichment_utils.H"
#include "dofkey.H"
#include "../drt_lib/drt_exporter.H"
#include "../drt_lib/drt_parobject.H"
#include "../drt_lib/drt_utils.H"



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool XFEM::EnrichmentInDofSet(
    const XFEM::Enrichment::EnrType     testenr,
    const std::set<XFEM::FieldEnr>&     fieldenrset)
{
  bool enr_in_set = false;
  for (std::set<XFEM::FieldEnr>::const_iterator fieldenr = fieldenrset.begin(); fieldenr != fieldenrset.end(); ++fieldenr)
  {
    if (fieldenr->getEnrichment().Type() == testenr)
    {
      enr_in_set = true;
      break;
    }
  }
  return enr_in_set;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool XFEM::EnrichmentInNodalDofSet(
    const int                                           gid,
    const XFEM::Enrichment::EnrType                     testenr,
    const std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet)
{
  bool enr_at_node = false;
  //check for testenrichment in the given nodalDofSet
  std::map<int, std::set<XFEM::FieldEnr> >::const_iterator setiter = nodalDofSet.find(gid);
  if (setiter != nodalDofSet.end())
  {
    const std::set<XFEM::FieldEnr>& fieldenrset = setiter->second;
    enr_at_node = XFEM::EnrichmentInDofSet(testenr, fieldenrset);
  }
  return enr_at_node;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool XFEM::ApplyNodalEnrichments(
    const DRT::Element*                           xfemele,
    const XFEM::InterfaceHandle&                  ih,
    const int&                                    label,
    const XFEM::Enrichment::EnrType               enrtype,
    const std::set<XFEM::PHYSICS::Field>&         fieldset,
    const double                                  volumeRatioLimit,
    std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet) 
{
  const double volumeratio = XFEM::DomainCoverageRatio(*xfemele,ih);
  const bool almost_empty_element = (fabs(1.0-volumeratio) < volumeRatioLimit);

  const XFEM::Enrichment voidenr(label, enrtype);

  bool skipped_element = false;
  
  if ( not almost_empty_element)  
  { // void enrichments for everybody !!!
    const int nen = xfemele->NumNode();
    const int* nodeidptrs = xfemele->NodeIds();
    for (int inen = 0; inen<nen; ++inen)
    {
      const int node_gid = nodeidptrs[inen];
      const bool anothervoidenrichment_in_set = XFEM::EnrichmentInNodalDofSet(node_gid, enrtype, nodalDofSet);
      if (not anothervoidenrichment_in_set)
      {
        for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
        {
          nodalDofSet[node_gid].insert(XFEM::FieldEnr(*field, voidenr));
        }
      }
    };
  }
  else
  { // void enrichments only in the fluid domain
    const int nen = xfemele->NumNode();
    const int* nodeidptrs = xfemele->NodeIds();
    for (int inen = 0; inen<nen; ++inen)
    {
      const int node_gid = nodeidptrs[inen];
      const LINALG::Matrix<3,1> nodalpos(ih.xfemdis()->gNode(node_gid)->X());
      const int label = ih.PositionWithinConditionNP(nodalpos);
      const bool in_fluid = (0 == label);

      if (in_fluid)
      {
        const bool anothervoidenrichment_in_set = EnrichmentInNodalDofSet(node_gid, enrtype, nodalDofSet);
        if (not anothervoidenrichment_in_set)
        {
          for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
          {
            nodalDofSet[node_gid].insert(XFEM::FieldEnr(*field, voidenr));
          }
        }
      }
    };
    skipped_element = true;
//    cout << "skipped interior void unknowns for element: "<< xfemele->Id() << ", volumeratio limit: " << std::scientific << volumeRatioLimit << ", volumeratio: abs (" << std::scientific << (1.0 - volumeratio) << " )" << endl;
  }
  return skipped_element;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool XFEM::ApplyNodalEnrichmentsNodeWise(
    const DRT::Element*                           xfemele,
    const XFEM::InterfaceHandle&                  ih,
    const int&                                    label,
    const XFEM::Enrichment::EnrType               enrtype,
    const std::set<XFEM::PHYSICS::Field>&         fieldset,
    const double                                  volumeRatioLimit,
    std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet) 
{
  const vector<double> ratios = XFEM::DomainCoverageRatioPerNode(*xfemele,ih);

  const XFEM::Enrichment voidenr(label, XFEM::Enrichment::typeVoid);

  bool skipped_element = false;
  
  const int nen = xfemele->NumNode();
  const int* nodeidptrs = xfemele->NodeIds();
  for (int inen = 0; inen<nen; ++inen)
  {
    const int node_gid = nodeidptrs[inen];
    
    const bool anothervoidenrichment_in_set = EnrichmentInNodalDofSet(node_gid, enrtype, nodalDofSet);
    
    if (not anothervoidenrichment_in_set)
    {
    
      const bool usefull_contribution = (fabs(ratios[inen]) > volumeRatioLimit);
      if ( usefull_contribution)  
      {      
        for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
        {
          nodalDofSet[node_gid].insert(XFEM::FieldEnr(*field, voidenr));
        }
      }
      else
      {
        skipped_element = true;
        cout << "skipped interior void unknowns for element: "<< xfemele->Id() << ", for node: "<< node_gid << ", volumeratio limit: " << std::scientific << volumeRatioLimit << ", volumeratio: abs (" << std::scientific << fabs(ratios[inen]) << " )" << endl;
        const LINALG::Matrix<3,1> nodalpos(ih.xfemdis()->gNode(node_gid)->X());
        const int label = ih.PositionWithinConditionNP(nodalpos);
        const bool in_fluid = (0 == label);
  
        if (in_fluid)
        {
          for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
          {
            nodalDofSet[node_gid].insert(XFEM::FieldEnr(*field, voidenr));
          }
        }
      }
    }
    else
    {
      cout << "skipping due to other voids already there" << endl;
    }
  }
  return skipped_element;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool XFEM::ApplyElementEnrichments(
    const DRT::Element*                           xfemele,
    const map<XFEM::PHYSICS::Field, DRT::Element::DiscretizationType>&  element_ansatz,
    const XFEM::InterfaceHandle&                  ih,
    const int&                                    label,
    const XFEM::Enrichment::EnrType               enrtype,
    const double                                  boundaryRatioLimit,
    std::set<XFEM::FieldEnr>&                     enrfieldset)
{
  // check, how much area for integration we have (from BoundaryIntcells)
  const double boundarysize = XFEM::BoundaryCoverageRatio(*xfemele,ih);
  const bool almost_zero_surface = (fabs(boundarysize) < boundaryRatioLimit);
  
  bool skipped_element = false;
  
  const XFEM::Enrichment enr(label, enrtype);
  //  const XFEM::Enrichment stdenr(0, XFEM::Enrichment::typeStandard);
  if ( not almost_zero_surface) 
  {
    const bool anothervoidenrichment_in_set = EnrichmentInDofSet(enrtype, enrfieldset);
    if (not anothervoidenrichment_in_set)
    {
      map<XFEM::PHYSICS::Field, DRT::Element::DiscretizationType>::const_iterator fielditer;
      for (fielditer = element_ansatz.begin();fielditer != element_ansatz.end();++fielditer)
      {
        enrfieldset.insert(XFEM::FieldEnr(fielditer->first, enr));
        //      enrfieldset.insert(XFEM::FieldEnr(fielditer->first, stdenr));
      }
    }
  }
  else
  {
    skipped_element = true;
//    cout << "skipped stress unknowns for element: "<< xfemele->Id() << ", boundary size: " << boundarysize << endl;
  }
  return skipped_element;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void XFEM::ApplyVoidEnrichmentForElement(
    const DRT::Element*                           xfemele,
    const map<XFEM::PHYSICS::Field, DRT::Element::DiscretizationType>&  element_ansatz,
    const XFEM::InterfaceHandle&                  ih,
    const int&                                    label,
    const std::set<XFEM::PHYSICS::Field>&         fieldset,
    const double                                  volumeRatioLimit,
    const double                                  boundaryRatioLimit,
    std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet,
    std::map<int, std::set<XFEM::FieldEnr> >&     elementalDofs,
    bool&                                         skipped_node_enr,
    bool&                                         skipped_elem_enr
    )
{
  const int element_gid = xfemele->Id();

  const XFEM::Enrichment::EnrType enrtype = XFEM::Enrichment::typeVoid;
  
  if (ih.ElementIntersected(element_gid))
  {
    if (ih.ElementHasLabel(element_gid, label))
    {
      skipped_node_enr = ApplyNodalEnrichments(xfemele, ih, label, enrtype, fieldset, volumeRatioLimit, nodalDofSet);
      //ApplyNodalEnrichmentsNodeWise(xfemele, ih, label, enrtype, fieldset, 2.0e-2, nodalDofSet); 

      skipped_elem_enr = ApplyElementEnrichments(xfemele, element_ansatz, ih, label, enrtype, boundaryRatioLimit, elementalDofs[element_gid]);
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
static void fillNodalDofKeySet(
    const XFEM::InterfaceHandle& ih,
    const std::map<int, std::set<XFEM::FieldEnr> >&  nodalDofSet,
    std::set<XFEM::DofKey<XFEM::onNode> >&      nodaldofkeyset
)
{
  nodaldofkeyset.clear();
  // loop all (non-overlapping = Row)-Nodes and store the DOF information w.t.h. of DofKeys
  for (int i=0; i<ih.xfemdis()->NumMyColNodes(); ++i)
  {
    const DRT::Node* actnode = ih.xfemdis()->lColNode(i);
    const int gid = actnode->Id();
    std::map<int, std::set<XFEM::FieldEnr> >::const_iterator entry = nodalDofSet.find(gid);
    if (entry == nodalDofSet.end())
    {
      // no dofs for this node... must be a hole or somethin'
      continue;
    }
    const std::set<XFEM::FieldEnr> dofset = entry->second;
    
    std::set<XFEM::FieldEnr>::const_iterator fieldenr;
    for(fieldenr = dofset.begin(); fieldenr != dofset.end(); ++fieldenr )
    {
      nodaldofkeyset.insert(XFEM::DofKey<XFEM::onNode>(gid, *fieldenr));
    }
  };
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
static void updateNodalDofMap(
    const XFEM::InterfaceHandle& ih,
    std::map<int, std::set<XFEM::FieldEnr> >&  nodalDofSet,
    const std::set<XFEM::DofKey<XFEM::onNode> >&      nodaldofkeyset
)
{
  // pack data on all processors
  for(std::set<XFEM::DofKey<XFEM::onNode> >::const_iterator dofkey=nodaldofkeyset.begin();
      dofkey != nodaldofkeyset.end();
      ++dofkey)
  {
    const int nodegid = dofkey->getGid();
    const XFEM::FieldEnr fieldenr = dofkey->getFieldEnr();
    // is node a row node?
//    if (ih.xfemdis()->HaveGlobalNode(nodegid))
      nodalDofSet[nodegid].insert(fieldenr);
  }
}


#ifdef PARALLEL
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
static void packDofKeys(
    const set<XFEM::DofKey<XFEM::onNode> >&     dofkeyset,
    vector<char>&                               dataSend )
{
  // pack data on all processors
  for(std::set<XFEM::DofKey<XFEM::onNode> >::const_iterator dofkey=dofkeyset.begin(); dofkey != dofkeyset.end(); dofkey++)
  {
    vector<char> data;
    dofkey->Pack(data);
    DRT::ParObject::AddtoPack(dataSend,data);
  }
}           



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
static void unpackDofKeys(
    const vector<char>&                     dataRecv, 
    set<XFEM::DofKey<XFEM::onNode> >&       dofkeyset )
{       
  int index = 0;
  while (index < (int) dataRecv.size())
  {
    vector<char> data;
    DRT::ParObject::ExtractfromPack(index, dataRecv, data);
    dofkeyset.insert(XFEM::DofKey<XFEM::onNode>(data));
  }
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
static void syncNodalDofs(
    const XFEM::InterfaceHandle& ih,
    std::map<int, std::set<XFEM::FieldEnr> >&  nodalDofSet)
{
  const int myrank = ih.xfemdis()->Comm().MyPID();
  const int numproc = ih.xfemdis()->Comm().NumProc();
  
  int size_one = 1;
  
  DRT::Exporter exporter(ih.xfemdis()->Comm());
  
  int dest = myrank+1;
  if(myrank == (numproc-1))
    dest = 0;
  
  int source = myrank-1;
  if(myrank == 0)
    source = numproc-1;
  
  set<XFEM::DofKey<XFEM::onNode> >  original_dofkeyset;
  fillNodalDofKeySet(ih, nodalDofSet, original_dofkeyset);
  
  set<XFEM::DofKey<XFEM::onNode> >  new_dofkeyset;
  //  new_dofkeyset = original_dofkeyset;
  for (set<XFEM::DofKey<XFEM::onNode> >::const_iterator dofkey = original_dofkeyset.begin(); dofkey != original_dofkeyset.end(); ++dofkey)
  {
    new_dofkeyset.insert(*dofkey);
  }
  
  // pack date for initial send
  vector<char> dataSend;
  packDofKeys(original_dofkeyset, dataSend);
  
  // send data in a circle
  for(int num = 0; num < numproc-1; num++)
  {
    vector<int> lengthSend(1,0);
    lengthSend[0] = dataSend.size();
    
#ifdef DEBUG
    cout << "proc " << myrank << ": sending"<< lengthSend[0] << "bytes to proc " << dest << endl;
    //    cout << "proc " << myrank << ": sending"<< dofkeyset.size() << " dofkeys to proc " << dest << endl;
#endif
    
    // send length of the data to be received ...
    MPI_Request req_length_data;
    int length_tag = 0;
    exporter.ISend(myrank, dest, &(lengthSend[0]) , size_one, length_tag, req_length_data);
    
    // ... and receive length
    vector<int> lengthRecv(1,0);
    exporter.Receive(source, length_tag, lengthRecv, size_one);
    exporter.Wait(req_length_data);   
    
    //    if(lengthRecv[0] > 0) ??
    //    {
    // send actual data ...
    int data_tag = 4;
    MPI_Request req_data;
    exporter.ISend(myrank, dest, &(dataSend[0]), lengthSend[0], data_tag, req_data);
    // ... and receive date
    vector<char> dataRecv(lengthRecv[0]);
    exporter.ReceiveAny(source, data_tag, dataRecv, lengthRecv[0]);
    exporter.Wait(req_data);  
    
    
    // unpack dofkeys from char array
    set<XFEM::DofKey<XFEM::onNode> >       dofkeyset;
    unpackDofKeys(dataRecv, dofkeyset);
#ifdef DEBUG
    cout << "proc " << myrank << ": receiving"<< lengthRecv[0] << "bytes from proc " << source << endl;
    cout << "proc " << myrank << ": receiving"<< dofkeyset.size() << " dofkeys from proc " << source << endl;
#endif
    // get all dofkeys whose nodegid is on this proc in the coloumnmap
    set<XFEM::DofKey<XFEM::onNode> >       relevant_dofkeyset;
    for (set<XFEM::DofKey<XFEM::onNode> >::const_iterator dofkey = dofkeyset.begin(); dofkey != dofkeyset.end(); ++dofkey)
    {
      const int nodegid = dofkey->getGid();
      if (ih.xfemdis()->HaveGlobalNode(nodegid))
      {
        new_dofkeyset.insert(*dofkey);
      }
    }
    // make received data the new 'to be sent' data
    dataSend = dataRecv;
    //    }
    //    else
    //    {
    //      dataSend.clear();
    //    }
    ih.xfemdis()->Comm().Barrier();
  }   // loop over procs 
  
  cout << "sync nodal dofs on proc " << myrank << ": before/after -> " << original_dofkeyset.size()<< "/" << new_dofkeyset.size() << endl;
  
  updateNodalDofMap(ih, nodalDofSet,new_dofkeyset);
  
}

#endif


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void XFEM::createDofMap(
    const XFEM::InterfaceHandle&                        ih,
    std::map<int, const std::set<XFEM::FieldEnr> >&     nodalDofSetFinal,
    std::map<int, const std::set<XFEM::FieldEnr> >&     elementalDofsFinal,
    const std::set<XFEM::PHYSICS::Field>&               fieldset,
    const XFEM::ElementAnsatz&                          elementAnsatz,
    const Teuchos::ParameterList&                       params
    )
{
  // temporary assembly
  std::map<int, std::set<XFEM::FieldEnr> >  nodalDofSet;
  std::map<int, std::set<XFEM::FieldEnr> >  elementalDofs;

  // get elements for each coupling label
  const std::map<int,std::set<int> >& elementsByLabel = ih.elementsByLabel(); 

  const double volumeRatioLimit = params.get<double>("volumeRatioLimit");
  const double boundaryRatioLimit = params.get<double>("boundaryRatioLimit");
  
  int skipped_node_enr_count = 0;
  int skipped_elem_enr_count = 0;
  
  // loop condition labels
  for(std::map<int,std::set<int> >::const_iterator conditer = elementsByLabel.begin(); conditer!=elementsByLabel.end(); ++conditer)
  {
    const int label = conditer->first;

    // for surface with label, loop my col elements and add enrichments to each elements member nodes
    for (int i=0; i<ih.xfemdis()->NumMyColElements(); ++i)
    {
      const DRT::Element* xfemele = ih.xfemdis()->lColElement(i);
      // add discontinuous stress unknowns
      // the number of each of these parameters will be determined later
      // by using a discretization type and appropriate shape functions
      map<XFEM::PHYSICS::Field, DRT::Element::DiscretizationType> element_ansatz; 
      if (not params.get<bool>("DLM_condensation"))
      {
        element_ansatz = elementAnsatz.getElementAnsatz(xfemele->Shape());
      }
      
      bool skipped_node_enr = false;
      bool skipped_elem_enr = false;
      
      XFEM::ApplyVoidEnrichmentForElement(
          xfemele, element_ansatz, ih, label, fieldset,
          volumeRatioLimit, boundaryRatioLimit,
          nodalDofSet, elementalDofs,
          skipped_node_enr, skipped_elem_enr);
      if (skipped_node_enr)
        skipped_node_enr_count++;
      if (skipped_elem_enr)
        skipped_elem_enr_count++;
    };
  };

#ifdef PARALLEL
  syncNodalDofs(ih, nodalDofSet);
#endif

  cout << " skipped node unknowns for "<< skipped_node_enr_count << " elements (volumeratio limit:   " << std::scientific << volumeRatioLimit   << ")" << endl;
  cout << " skipped elem unknowns for "<< skipped_elem_enr_count << " elements (boundaryratio limit: " << std::scientific << boundaryRatioLimit << ")" << endl;
  
  XFEM::applyStandardEnrichmentNodalBasedApproach(ih, fieldset, nodalDofSet);
  
//#ifdef PARALLEL
//  syncNodalDofs(ih, nodalDofSet);
//#endif
  
  // create const sets from standard sets, so the sets cannot be accidently changed
  // could be removed later, if this is a performance bottleneck
  for ( std::map<int, std::set<XFEM::FieldEnr> >::const_iterator oneset = nodalDofSet.begin(); oneset != nodalDofSet.end(); ++oneset )
  {
    nodalDofSetFinal.insert( make_pair(oneset->first, oneset->second));
  };

  for ( std::map<int, std::set<XFEM::FieldEnr> >::const_iterator onevec = elementalDofs.begin(); onevec != elementalDofs.end(); ++onevec )
  {
    elementalDofsFinal.insert( make_pair(onevec->first, onevec->second));
  };
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void XFEM::applyStandardEnrichment(
    const XFEM::InterfaceHandle&                  ih,
    const std::set<XFEM::PHYSICS::Field>&         fieldset,
    std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet,
    std::map<int, std::set<XFEM::FieldEnr> >&     elementalDofs)
{
  const int standard_label = 0;
  const XFEM::Enrichment enr_std(standard_label, XFEM::Enrichment::typeStandard);
  for (int i=0; i<ih.xfemdis()->NumMyColElements(); ++i)
  {
    const DRT::Element* xfemele = ih.xfemdis()->lColElement(i);
    if ( not ih.ElementIntersected(xfemele->Id()))
    {
      const int* nodeidptrs = xfemele->NodeIds();
      const LINALG::Matrix<3,1> nodalpos(xfemele->Nodes()[0]->X());

      const int label = ih.PositionWithinConditionNP(nodalpos);
      const bool in_fluid = (0 == label);

      if (in_fluid)
      {
        for (int inen = 0; inen<xfemele->NumNode(); ++inen)
        {
          const int node_gid = nodeidptrs[inen];
          bool voidenrichment_in_set = false;
          //check for void enrichement in a given set, if such set already exists for this node_gid
          std::map<int, std::set<FieldEnr> >::const_iterator setiter = nodalDofSet.find(node_gid);
          if (setiter != nodalDofSet.end())
          {

            std::set<FieldEnr> fieldenrset = setiter->second;
            for (std::set<FieldEnr>::const_iterator fieldenr = fieldenrset.begin(); fieldenr != fieldenrset.end(); ++fieldenr)
            {
              if (fieldenr->getEnrichment().Type() == Enrichment::typeVoid)
              {
                voidenrichment_in_set = true;
                break;
              }
            }
          }
          if (not voidenrichment_in_set)
          {
            for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
            {
              nodalDofSet[node_gid].insert(XFEM::FieldEnr(*field, enr_std));
            }
          }
        };

        //                // add continuous stress unknowns
        //                const int element_gid = actele->Id();
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauxx, enr_std));
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauyy, enr_std));
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauzz, enr_std));
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauxy, enr_std));
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauxz, enr_std));
        //                elementalDofs[element_gid].insert(XFEM::FieldEnr(PHYSICS::Tauyz, enr_std));
      }
    }
  };
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void XFEM::applyStandardEnrichmentNodalBasedApproach(
    const XFEM::InterfaceHandle&                  ih,
    const std::set<XFEM::PHYSICS::Field>&         fieldset,
    std::map<int, std::set<XFEM::FieldEnr> >&     nodalDofSet)
{
  const int standard_label = 0;
  const XFEM::Enrichment enr_std(standard_label, XFEM::Enrichment::typeStandard);
  for (int i=0; i<ih.xfemdis()->NumMyColNodes(); ++i)
  {
    const DRT::Node* node = ih.xfemdis()->lColNode(i);
    const LINALG::Matrix<3,1> nodalpos(node->X());

    const bool voidenrichment_in_set = EnrichmentInNodalDofSet(node->Id(), XFEM::Enrichment::typeVoid, nodalDofSet);
    
    if (not voidenrichment_in_set)
    {
      const int label = ih.PositionWithinConditionNP(nodalpos);
      const bool in_fluid = (0 == label);

      if (in_fluid)
      {
        for (std::set<XFEM::PHYSICS::Field>::const_iterator field = fieldset.begin();field != fieldset.end();++field)
        {
          nodalDofSet[node->Id()].insert(XFEM::FieldEnr(*field, enr_std));
        }
      }
    }
  };
}




#endif  // #ifdef CCADISCRET
