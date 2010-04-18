/*!----------------------------------------------------------------------
\file so_sh8_input.cpp
\brief

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15240
</pre>

*----------------------------------------------------------------------*/
#ifdef D_SOLID3
#ifdef CCADISCRET


#include "so_sh8.H"
#include "../drt_mat/artwallremod.H"
#include "../drt_mat/anisotropic_balzani.H"
#include "../drt_mat/viscoanisotropic.H"
#include "../drt_mat/visconeohooke.H"
#include "../drt_mat/elasthyper.H"
#include "../drt_mat/aaaraghavanvorp_damage.H"
#include "../drt_lib/drt_linedefinition.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::ELEMENTS::So_sh8::ReadElement(const std::string& eletype,
                                        const std::string& distype,
                                        DRT::INPUT::LineDefinition* linedef)
{
  // read number of material model
  int material = 0;
  linedef->ExtractInt("MAT",material);
  SetMaterial(material);

  // special element-dependent input of material parameters
  switch (Material()->MaterialType())
  {
  case INPAR::MAT::m_artwallremod:
  {
    MAT::ArtWallRemod* remo = static_cast <MAT::ArtWallRemod*>(Material().get());
    remo->Setup(NUMGPT_SOH8, this->Id(), linedef);
    break;
  }
  case INPAR::MAT::m_anisotropic_balzani:
  {
    MAT::AnisotropicBalzani* balz = static_cast <MAT::AnisotropicBalzani*>(Material().get());
    balz->Setup(linedef);
    break;
  }
  case INPAR::MAT::m_viscoanisotropic:
  {
    MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(Material().get());
    visco->Setup(NUMGPT_SOH8, linedef);
    break;
  }
  case INPAR::MAT::m_visconeohooke:
  {
    MAT::ViscoNeoHooke* visco = static_cast <MAT::ViscoNeoHooke*>(Material().get());
    visco->Setup(NUMGPT_SOH8);
    break;
  }
  case INPAR::MAT::m_elasthyper:
  {
    MAT::ElastHyper* elahy = static_cast <MAT::ElastHyper*>(Material().get());
    elahy->Setup(linedef);
    break;
  }
  case INPAR::MAT::m_aaaraghavanvorp_damage:
  {
    double strength = 0.0; // section for extracting the element strength
    linedef->ExtractDouble("STRENGTH",strength);
    MAT::AAAraghavanvorp_damage* aaadamage = static_cast <MAT::AAAraghavanvorp_damage*>(Material().get());
    aaadamage->Setup(NUMGPT_SOH8,strength);
    //aaadamage->Setup(NUMGPT_SOH8);
  }
  default:
    // Do nothing. Simple material.
    break;
  }


  // temporary variable for read-in
  std::string buffer;

  // we expect kintype to be total lagrangian
  kintype_ = soh8_totlag;

  // read EAS technology flag
  linedef->ExtractString("EAS",buffer);

  // full EAS technology
  if      (buffer=="sosh8")
  {
    eastype_ = soh8_eassosh8;
    neas_ = 7;               // number of eas parameters for EAS_SOSH8
    soh8_easinit();
  }
  // no EAS technology
  else if (buffer=="none")
  {
    eastype_ = soh8_easnone;
    neas_ = 0;               // number of eas parameters for EAS_SOSH8
  }
  else
    dserror("Reading of SO_SH8 EAS technology failed");

  // read ANS technology flag
  linedef->ExtractString("ANS",buffer);
  if      (buffer=="sosh8")
  {
    anstype_ = anssosh8;
  }
  // no ANS technology
  else if (buffer=="none")
  {
    anstype_ = ansnone;
  }
  else
    dserror("Reading of SO_SH8 ANS technology failed");
  
  linedef->ExtractString("THICKDIR",buffer);
  nodes_rearranged_ = false;

  // global X
  if      (buffer=="xdir")    thickdir_ = globx;
  // global Y
  else if (buffer=="ydir")    thickdir_ = globy;
  // global Z
  else if (buffer=="zdir")    thickdir_ = globz;
  // find automatically through Jacobian of Xrefe
  else if (buffer=="auto")    thickdir_ = autoj;
  // local r
  else if (buffer=="rdir")    thickdir_ = enfor;
  // local s
  else if (buffer=="sdir")    thickdir_ = enfos;
  // local t
  else if (buffer=="tdir")    thickdir_ = enfot;
  // no noderearrangement
  else if (buffer=="none")
  {
    thickdir_ = none;
    nodes_rearranged_ = true;
  }
  else dserror("Reading of SO_SH8 thickness direction failed");

  return true;
}


#if 0
/*----------------------------------------------------------------------*
 |  read element input (public)                                maf 04/07|
 *----------------------------------------------------------------------*/
bool DRT::ELEMENTS::So_sh8::ReadElement()
{
  // read element's nodes
  int ierr=0;
  const int nnode=8;
  int nodes[8];
  frchk("SOLIDSH8",&ierr);
  if (ierr==1)
  {
    frint_n("HEX8",nodes,nnode,&ierr);
    if (ierr != 1) dserror("Reading of ELEMENT Topology failed");
  }
  else
  {
    dserror ("Reading of SOLIDSH8 failed");
  }
  // reduce node numbers by one
  for (int i=0; i<nnode; ++i){
    nodes[i]--;
  }

  SetNodeIds(nnode,nodes);


  // read number of material model
  int material = 0;
  frint("MAT",&material,&ierr);
  if (ierr!=1) dserror("Reading of SO_SH8 element material failed");
  SetMaterial(material);

  // special element-dependent input of material parameters
  if (Material()->MaterialType() == INPAR::MAT::m_artwallremod){
    MAT::ArtWallRemod* remo = static_cast <MAT::ArtWallRemod*>(Material().get());
    remo->Setup(NUMGPT_SOH8, this->Id());
  } else if (Material()->MaterialType() == INPAR::MAT::m_anisotropic_balzani){
    MAT::AnisotropicBalzani* balz = static_cast <MAT::AnisotropicBalzani*>(Material().get());
    balz->Setup();
  } else if (Material()->MaterialType() == INPAR::MAT::m_viscoanisotropic){
    MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(Material().get());
    visco->Setup(NUMGPT_SOH8);
  } else if (Material()->MaterialType() == INPAR::MAT::m_visconeohooke){
    MAT::ViscoNeoHooke* visco = static_cast <MAT::ViscoNeoHooke*>(Material().get());
    visco->Setup(NUMGPT_SOH8);
  } else if (Material()->MaterialType() == INPAR::MAT::m_elasthyper){
    MAT::ElastHyper* elahy = static_cast <MAT::ElastHyper*>(Material().get());
    elahy->Setup();
      }
  else if (Material()->MaterialType() == INPAR::MAT::m_aaaraghavanvorp_damage)
      {
       double strength = 0.0; // section for extracting the element strength
       frdouble("STRENGTH",&strength,&ierr);
       if (ierr!=1) dserror("Reading of SO_SH8 element strength failed");

       MAT::AAAraghavanvorp_damage* aaadamage = static_cast <MAT::AAAraghavanvorp_damage*>(Material().get());
       aaadamage->Setup(NUMGPT_SOH8,strength);
       //aaadamage->Setup(NUMGPT_SOH8);
      }
  // read possible gaussian points, obsolete for computation
  int ngp[3];
  frint_n("GP",ngp,3,&ierr);
  if (ierr==1) for (int i=0; i<3; ++i) if (ngp[i]!=2) dserror("Only 2 GP for So_SH8");

  // we expect kintype to be total lagrangian
  kintype_ = soh8_totlag;

  // read kinematic type
  char buffer[50];
  frchar("KINEM",buffer,&ierr);
  if (ierr)
  {
   // geometrically linear
   if      (strncmp(buffer,"Geolin",6)==0)    kintype_ = soh8_geolin;
   // geometrically non-linear with Total Lagrangean approach
   else if (strncmp(buffer,"Totlag",6)==0)    kintype_ = soh8_totlag;
   // geometrically non-linear with Updated Lagrangean approach
   else if (strncmp(buffer,"Updlag",6)==0)
   {
       kintype_ = soh8_updlag;
       dserror("Updated Lagrange for SO_SH8 is not implemented!");
   }
   else dserror("Reading of SO_SH8 element failed");
  }

  // read EAS technology flag
  frchar("EAS",buffer,&ierr);
  if (ierr){
    // full EAS technology
    if      (strncmp(buffer,"sosh8",5)==0){
      eastype_ = soh8_eassosh8;
      neas_ = 7;               // number of eas parameters for EAS_SOSH8
      soh8_easinit();
    }
    // no EAS technology
    else if (strncmp(buffer,"none",4)==0){
//      cout << "Warning: Solid-Shell8 without EAS" << endl;
      eastype_ = soh8_easnone;
    }
    else dserror("Reading of SO_SH8 EAS technology failed");
  }
  else
  {
    eastype_ = soh8_eassosh8;     // default: EAS for Solid-Shell8
    neas_ = 7;                    // number of eas parameters for EAS_SOSH8
    soh8_easinit();
  }

  // read global coordinate of shell-thickness direction
  thickdir_ = autoj;           // default: auto by Jacobian
  nodes_rearranged_ = false;
  frchar("THICKDIR",buffer,&ierr);
  if (ierr)
  {
   // global X
   if      (strncmp(buffer,"xdir",4)==0)    thickdir_ = globx;
   // global Y
   else if (strncmp(buffer,"ydir",4)==0)    thickdir_ = globy;
   // global Z
   else if (strncmp(buffer,"zdir",4)==0)    thickdir_ = globz;
   // find automatically through Jacobian of Xrefe
   else if (strncmp(buffer,"auto",4)==0)    thickdir_ = autoj;
   // local r
   else if (strncmp(buffer,"rdir",4)==0)    thickdir_ = enfor;
   // local s
   else if (strncmp(buffer,"sdir",4)==0)    thickdir_ = enfos;
   // local t
   else if (strncmp(buffer,"tdir",4)==0)    thickdir_ = enfot;
   // no noderearrangement
   else if (strncmp(buffer,"none",4)==0){
     thickdir_ = none;
     nodes_rearranged_ = true;
   }
   else dserror("Reading of SO_SH8 thickness direction failed");
  }

  return true;
} // So_sh8::ReadElement()
#endif

#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_SOLID3
