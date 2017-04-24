/*!----------------------------------------------------------------------
\file so_hex20_input.cpp
\brief
\level 1
\maintainer Alexander Seitz

*----------------------------------------------------------------------*/

#include "so_hex20.H"
#include "../drt_mat/so3_material.H"
#include "../drt_lib/drt_linedefinition.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::ELEMENTS::So_hex20::ReadElement(const std::string& eletype,
                                          const std::string& distype,
                                          DRT::INPUT::LineDefinition* linedef)
{
  // read number of material model
  int material = 0;
  linedef->ExtractInt("MAT",material);
  SetMaterial(material);

  SolidMaterial()->Setup(NUMGPT_SOH20, linedef);

  std::string buffer;
  linedef->ExtractString("KINEM",buffer);

  if (buffer=="linear")
  {
    kintype_ = INPAR::STR::kinem_linear;
  }
  else if (buffer=="nonlinear")
  {
    kintype_ = INPAR::STR::kinem_nonlinearTotLag;
  }
  else dserror ("Reading SO_HEX20 element failed KINEM unknown");

  // check if material kinematics is compatible to element kinematics
  SolidMaterial()->ValidKinematics(kintype_);

  return true;
}


