/*!----------------------------------------------------------------------
\file torsion2_input.cpp
\brief two dimensional torsion spring element

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15264
</pre>

*----------------------------------------------------------------------*/

#include "torsion2.H"
#include "../drt_lib/drt_linedefinition.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::ELEMENTS::Torsion2::ReadElement(const std::string&          eletype,
                                          const std::string&          distype,
                                          DRT::INPUT::LineDefinition* linedef)
{
  // read type of material model
  int material = 0;
  linedef->ExtractInt("MAT",material);
  SetMaterial(material);
  
  // read type of bending potential
  std::string buffer;
  linedef->ExtractString("BENDINGPOTENTIAL",buffer);
  
  // bending potential E_bend = 0.5*SPRING*\theta^2
  if (buffer=="quadratic")
    bendingpotential_ = quadratic;

  // bending potential E_bend = SPRING*(1 - \cos(\theta^2) )
  else if (buffer=="cosine")
    bendingpotential_ = cosine;

  else
    dserror("Reading of Torsion2 element failed because of unknown potential type!");
  
  return true;
}

