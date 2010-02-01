/*!----------------------------------------------------------------------
\file fluid3_input.cpp
\brief

<pre>
Maintainer: Michael Gee
            gee@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15239
</pre>

*----------------------------------------------------------------------*/
#ifdef D_FLUID3
#ifdef CCADISCRET

#include "fluid3.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/standardtypes_cpp.H"
#include "../drt_lib/drt_linedefinition.H"

extern struct _GENPROB     genprob;

using namespace DRT::UTILS;


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::ELEMENTS::Fluid3::ReadElement(const std::string& eletype,
                                        const std::string& distype,
                                        DRT::INPUT::LineDefinition* linedef)
{
  if (genprob.ndim!=3)
    dserror("Problem defined as %dd, but Fluid3 does not support 2D elements yet",genprob.ndim);

  // read number of material model
  int material = 0;
  linedef->ExtractInt("MAT",material);
  SetMaterial(material);

  // Set Discretization Type
  // setOptimalgaussrule is pushed into the element routine
  SetDisType(DRT::StringToDistype(distype));

  std::string na;
  linedef->ExtractString("NA",na);
  if (na=="ale" or na=="ALE" or na=="Ale")
  {
    is_ale_ = true;
  }
  else if (na=="euler" or na=="EULER" or na=="Euler")
    is_ale_ = false;
  else
    dserror("Reading of FLUID3 element failed: Euler/Ale");

  return true;
}


#if 0
/*----------------------------------------------------------------------*
 |  read element input (public)                              g.bau 03/07|
 *----------------------------------------------------------------------*/
bool DRT::ELEMENTS::Fluid3::ReadElement()
{
  if (genprob.ndim!=3)
    dserror("Not a 3d problem. Panic.");

    typedef map<string, DiscretizationType> Gid2DisType;
    Gid2DisType gid2distype;
    gid2distype["HEX8"]     = hex8;
    gid2distype["HEX20"]    = hex20;
    gid2distype["HEX27"]    = hex27;
    gid2distype["TET4"]     = tet4;
    gid2distype["TET10"]    = tet10;
    gid2distype["WEDGE6"]   = wedge6;
    gid2distype["WEDGE15"]  = wedge15;
    gid2distype["PYRAMID5"] = pyramid5;
    gid2distype["NURBS8"]   = nurbs8;
    gid2distype["NURBS27"]  = nurbs27;

    // read element's nodes
    int   ierr = 0;
    int   nnode = 0;
    int   nodes[27];
    DiscretizationType distype = dis_none;

    Gid2DisType::const_iterator iter;
    for( iter = gid2distype.begin(); iter != gid2distype.end(); iter++ )
    {
        const string eletext = iter->first;
        frchk(eletext.c_str(), &ierr);
        if (ierr == 1)
        {
            distype = gid2distype[eletext];
            nnode = DRT::UTILS::getNumberOfElementNodes(distype);
            frint_n(eletext.c_str(), nodes, nnode, &ierr);
            dsassert(ierr==1, "Reading of ELEMENT Topology failed\n");
            break;
        }
    }

    // reduce node numbers by one
    for (int i=0; i<nnode; ++i) nodes[i]--;

    SetNodeIds(nnode,nodes);

    // read number of material model
    int material = 0;
    frint("MAT",&material,&ierr);
    dsassert(ierr==1, "Reading of material for FLUID3 element failed\n");
    dsassert(material!=0, "No material defined for FLUID3 element\n");
    SetMaterial(material);


    // read gaussian points and set gaussrule
    char  buffer[50];
    int ngp[3];
    switch (distype)
    {
    case hex8: case hex20: case hex27: case nurbs8: case nurbs27:
    {
        frint_n("GP",ngp,3,&ierr);
        dsassert(ierr==1, "Reading of FLUID3 element failed: GP\n");
        switch (ngp[0])
        {
        case 1:
            gaussrule_ = intrule_hex_1point;
            break;
        case 2:
            gaussrule_ = intrule_hex_8point;
            break;
        case 3:
            gaussrule_ = intrule_hex_27point;
            break;
        default:
            dserror("Reading of FLUID3 element failed: Gaussrule for hexaeder not supported!\n");
        }
        break;
    }
    case tet4: case tet10:
    {
        frint("GP_TET",&ngp[0],&ierr);
        dsassert(ierr==1, "Reading of FLUID3 element failed: GP_TET\n");

        frchar("GP_ALT",buffer,&ierr);
        dsassert(ierr==1, "Reading of FLUID3 element failed: GP_ALT\n");

        switch(ngp[0])
        {
        case 1:
            if (strncmp(buffer,"standard",8)==0)
                gaussrule_ = intrule_tet_1point;
            else
                dserror("Reading of FLUID3 element failed: GP_ALT: gauss-radau not possible!\n");
            break;
        case 4:
            if (strncmp(buffer,"standard",8)==0)
                gaussrule_ = intrule_tet_4point;
            else if (strncmp(buffer,"gaussrad",8)==0)
                gaussrule_ = intrule_tet_4point_gauss_radau;
            else
                dserror("Reading of FLUID3 element failed: GP_ALT\n");
            break;
        case 10:
            if (strncmp(buffer,"standard",8)==0)
                gaussrule_ = intrule_tet_5point;
            else
                dserror("Reading of FLUID3 element failed: GP_ALT: gauss-radau not possible!\n");
            break;
        default:
            dserror("Reading of FLUID3 element failed: Gaussrule for tetraeder not supported!\n");
        }
        break;
    } // end reading gaussian points for tetrahedral elements
    case wedge6: case wedge15:
    {
      frint("GP_WEDGE",&ngp[0],&ierr);
        dsassert(ierr==1, "Reading of FLUID3 element failed: GP_WEDGE\n");
      switch (ngp[0])
        {
        case 1:
            gaussrule_ = intrule_wedge_1point;
            break;
        case 6:
            gaussrule_ = intrule_wedge_6point;
            break;
        case 9:
            gaussrule_ = intrule_wedge_9point;
            break;
        default:
            dserror("Reading of FLUID3 element failed: Gaussrule for wedge not supported!\n");
        }
        break;
    }

    case pyramid5:
    {
      frint("GP_PYRAMID",&ngp[0],&ierr);
        dsassert(ierr==1, "Reading of FLUID3 element failed: GP_PYRAMID\n");
      switch (ngp[0])
        {
        case 1:
            gaussrule_ = intrule_pyramid_1point;
            break;
        case 8:
            gaussrule_ = intrule_pyramid_8point;
            break;
        default:
            dserror("Reading of FLUID3 element failed: Gaussrule for pyramid  not supported!\n");
        }
        break;
    }
    default:
        dserror("Reading of FLUID3 element failed: integration points\n");
    } // end switch distype

    // read net algo
    frchar("NA",buffer,&ierr);
    if (ierr==1)
    {
        if (strncmp(buffer,"ale",3)==0 ||
            strncmp(buffer,"ALE",3)==0 ||
            strncmp(buffer,"Ale",3)==0 )
        {
            is_ale_ = true;
        }
        else if (strncmp(buffer,"euler",5)==0 ||
                 strncmp(buffer,"EULER",5)==0 ||
                 strncmp(buffer,"Euler",5)==0 )
            is_ale_ = false;
        else
            dserror("Reading of FLUID3 element failed: Euler/Ale\n");
    }
    else
        dserror("Reading of FLUID3 element net algorithm failed: NA\n");


  // input of ale and free surface related stuff is not supported
  // at the moment. TO DO!
  // see src/fluid3/f3_inpele.c for missing details


  return true;

} // Fluid3::ReadElement()
#endif

#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_FLUID3
