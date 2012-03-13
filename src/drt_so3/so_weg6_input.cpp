/*!----------------------------------------------------------------------
\file so_weg6_input.cpp
\brief

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15240
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "so_weg6.H"
#include "../drt_mat/artwallremod.H"
#include "../drt_mat/viscoanisotropic.H"
#include "../drt_mat/holzapfelcardiovascular.H"
#include "../drt_mat/humphreycardiovascular.H"
#include "../drt_mat/growth_ip.H"
#include "../drt_mat/constraintmixture.H"
#include "../drt_lib/drt_linedefinition.H"
#include "../drt_mat/elasthyper.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::ELEMENTS::So_weg6::ReadElement(const std::string& eletype,
                                         const std::string& distype,
                                         DRT::INPUT::LineDefinition* linedef)
{
  // read number of material model
  int material = 0;
  linedef->ExtractInt("MAT",material);
  SetMaterial(material);

  // special element-dependent input of material parameters
  if (Material()->MaterialType() == INPAR::MAT::m_artwallremod){
    MAT::ArtWallRemod* remo = static_cast <MAT::ArtWallRemod*>(Material().get());
    remo->Setup(NUMGPT_WEG6, this->Id(), linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_viscoanisotropic){
    MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(Material().get());
    visco->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_holzapfelcardiovascular){
    MAT::HolzapfelCardio* holzcard = static_cast <MAT::HolzapfelCardio*>(Material().get());
    holzcard->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_humphreycardiovascular){
    MAT::HumphreyCardio* humcard = static_cast <MAT::HumphreyCardio*>(Material().get());
    humcard->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_growth){
    MAT::Growth* grow = static_cast <MAT::Growth*>(Material().get());
    grow->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_constraintmixture){
    MAT::ConstraintMixture* comix = static_cast <MAT::ConstraintMixture*>(Material().get());
    comix->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_elasthyper){
    MAT::ElastHyper* elahy = static_cast <MAT::ElastHyper*>(Material().get());
    elahy->Setup(linedef);
  }

  std::string buffer;
  linedef->ExtractString("KINEM",buffer);
  if (buffer=="linear")
  {
    kintype_ = sow6_linear;
    dserror("Reading of SO_WEG6 element failed only nonlinear kinematics implemented");
  }
  else if (buffer=="nonlinear")
  {
    kintype_ = sow6_nonlinear;
  }
  else dserror ("Reading SO_WEG6 element failed KINEM unknwon");

  return true;
}


#if 0
/*----------------------------------------------------------------------*
 |  read element input (public)                                maf 04/07|
 *----------------------------------------------------------------------*/
bool DRT::ELEMENTS::So_weg6::ReadElement()
{
  // read element's nodes
  int ierr=0;
  const int nnode=6;
  int nodes[6];
  frchk("SOLIDW6",&ierr);
  if (ierr==1)
  {
    frint_n("WEDGE6",nodes,nnode,&ierr);
    if (ierr != 1) dserror("Reading of ELEMENT Topology failed");
  }
  else
  {
    dserror ("Reading of SOLIDW6 failed");
  }
  // reduce node numbers by one
  for (int i=0; i<nnode; ++i){
    nodes[i]--;
  }

  SetNodeIds(nnode,nodes);

  // read number of material model
  int material = 0;
  frint("MAT",&material,&ierr);
  if (ierr!=1) dserror("Reading of SO_WEG6 element material failed");
  SetMaterial(material);

  // special element-dependent input of material parameters
  if (Material()->MaterialType() == INPAR::MAT::m_artwallremod){
    MAT::ArtWallRemod* remo = static_cast <MAT::ArtWallRemod*>(Material().get());
    remo->Setup(NUMGPT_WEG6, this->Id());
  } else if (Material()->MaterialType() == INPAR::MAT::m_viscoanisotropic){
    MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(Material().get());
    visco->Setup(NUMGPT_WEG6);
  } else if (Material()->MaterialType() == INPAR::MAT::m_holzapfelcardiovascular){
    MAT::HolzapfelCardio* holzcard = static_cast <MAT::HolzapfelCardio*>(Material().get());
    holzcard->Setup(NUMGPT_WEG6, linedef);
  } else if (Material()->MaterialType() == INPAR::MAT::m_humphreycardiovascular){
    MAT::HumphreyCardio* humcard = static_cast <MAT::HumphreyCardio*>(Material().get());
    humcard->Setup(NUMGPT_WEG6, linedef);
  }


  // we expect kintype to be total lagrangian
  kintype_ = sow6_totlag;

  // read kinematic type
  char buffer[50];
  frchar("KINEM",buffer,&ierr);
  if (ierr)
  {
   // geometrically linear
   if      (strncmp(buffer,"Geolin",6)==0)    kintype_ = sow6_geolin;
   // geometrically non-linear with Total Lagrangean approach
   else if (strncmp(buffer,"Totlag",6)==0)    kintype_ = sow6_totlag;
   // geometrically non-linear with Updated Lagrangean approach
   else if (strncmp(buffer,"Updlag",6)==0)
   {
       kintype_ = sow6_updlag;
       dserror("Updated Lagrange for SO_WEG6 is not implemented!");
   }
   else dserror("Reading of SO_WEG6 element failed");
  }

  return true;
} // So_weg6::ReadElement()
#endif

#endif  // #ifdef CCADISCRET
