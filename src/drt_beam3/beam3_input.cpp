/*!----------------------------------------------------------------------
\file beam3_input.cpp
\brief

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15264
</pre>

*----------------------------------------------------------------------*/
#ifdef D_BEAM3
#ifdef CCADISCRET

#include "beam3.H"
#include "../drt_lib/standardtypes_cpp.H"

/*----------------------------------------------------------------------*
 |  read element input (public)                              cyron 03/08|
 *----------------------------------------------------------------------*/
bool DRT::ELEMENTS::Beam3::ReadElement()
{
  /*
  \The element is capable of using higher order functions from linear to quartic. Please make sure you put the nodes in the right order
  \in the input file.
  \LIN2  1---2
  \LIN3	 1---3---2
  \LIN4  1---4---2---3
  \LIN5	 1---5---2---3---4
  */
	
  // read element's nodes; in case of a beam element always line2 shape
  int ierr=0;

  //typedef for later conversion of string to distype
  //note: GID gives LINX in the .dat file while pre_exodus gives LINEX
  typedef map<string, DiscretizationType> Beam2rDisType;
  Beam2rDisType beam2rdistype;
  beam2rdistype["LIN2"]     = line2;
  beam2rdistype["LINE2"]    = line2;
  beam2rdistype["LIN3"]     = line3;
  beam2rdistype["LINE3"]    = line3;
  beam2rdistype["LIN4"]     = line4;
  beam2rdistype["LINE4"]    = line4;
  beam2rdistype["LIN5"]     = line5;
  beam2rdistype["LINE5"]    = line5;


  DiscretizationType distype = dis_none;

  //Iterator goes through all possibilities
  Beam2rDisType::const_iterator iter;

  for( iter = beam2rdistype.begin(); iter != beam2rdistype.end(); iter++ )
  {
    const string eletext = iter->first;
    frchk(eletext.c_str(), &ierr);
    if (ierr == 1)
    {
      //Get DiscretizationType
 	    distype = beam2rdistype[eletext];
      //Get Number of Nodes of DiscretizationType
      int nnode = DRT::UTILS::getNumberOfElementNodes(distype);

      //Get an array for the global node numbers
      std::vector<int> nodes(nnode,0);
      //Read global node numbers
      frint_n(eletext.c_str(), &nodes[0], nnode, &ierr);

      dsassert(ierr==1, "Reading of ELEMENT Topology failed\n");

      // reduce global node numbers by one because BACI nodes begin with 0 and inputfile nodes begin with 1
      for (int i=0; i<nnode; ++i) nodes[i]--;
      SetNodeIds(nnode,&nodes[0]); // has to be executed in here because of the local scope of nodes
      break;
    }
  }

  // read material parameters using structure _MATERIAL which is defined by inclusion of
  // "../drt_lib/drt_timecurve.H"
  int material = 0;
  frint("MAT",&material,&ierr);
  if (ierr!=1) dserror("Reading of Beam3 element failed");
  SetMaterial(material);

  // read beam cross section
  crosssec_ = 0;
  frdouble("CROSS",&crosssec_,&ierr);
  if (ierr!=1) dserror("Reading of Beam3 element failed");

  // read beam shear correction and compute corrected cross section
  double shear_correction = 0.0;
  frdouble("SHEARCORR",&shear_correction,&ierr);
  if (ierr!=1) dserror("Reading of Beam3 element failed");
  crosssecshear_ = crosssec_ * shear_correction;

  /*read beam moments of inertia of area; currently the beam3 element works only with rotationally symmetric
   * crosssection so that the moment of inertia of area around both principal can be expressed by one input
   * number I_; however, the implementation itself is a general one and works also for other cases; the only
   * point which has to be made sure is that the nodal triad T_ is initialized in the registration process
   * (->beam3.cpp) in such a way that t1 is the unit vector along the beam axis and t2 and t3 are the principal
   * axes with moment of inertia of area Iyy_ and Izz_, respectively; so a modification to more general kinds of
   * cross sections can be done easily by allowing for more complex input right here and by calculating an approxipate
   * initial nodal triad in the frame of the registration; */

  frdouble("MOMIN",&Iyy_,&ierr);
  frdouble("MOMIN",&Izz_,&ierr);
  frdouble("MOMINPOL",&Irr_,&ierr);
  if (ierr!=1) dserror("Reading of Beam3 element failed");


  return true;
} // Beam3::ReadElement()


#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_BEAM3
