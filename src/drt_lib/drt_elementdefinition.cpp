#ifdef CCADISCRET

#include "drt_elementdefinition.H"


/*----------------------------------------------------------------------*/
//! Print function to be called from C
/*----------------------------------------------------------------------*/
extern "C"
void PrintElementDatHeader()
{
  DRT::INPUT::ElementDefinition ed;
  ed.PrintElementDatHeaderToStream(std::cout);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::PrintElementDatHeaderToStream(std::ostream& stream)
{
  SetupValidElementLines();

  PrintSectionHeader(stream,"STRUCTURE ELEMENTS");

  PrintElementLines(stream,"ART");
  PrintElementLines(stream,"BEAM2");
  PrintElementLines(stream,"BEAM2R");
  PrintElementLines(stream,"BEAM3");
  //PrintElementLines(stream,"CONSTRELE2");
  //PrintElementLines(stream,"CONSTRELE3");
  PrintElementLines(stream,"PTET4");
  PrintElementLines(stream,"SHELL8");
  PrintElementLines(stream,"SOLID3");
  PrintElementLines(stream,"SOLIDH20");
  PrintElementLines(stream,"SOLIDH27");
  PrintElementLines(stream,"SOLIDH8");
  PrintElementLines(stream,"SOLIDH8P1J1");
  PrintElementLines(stream,"SOLIDSH8");
  PrintElementLines(stream,"SOLIDSH8P8");
  PrintElementLines(stream,"SOLIDSHW6");
  PrintElementLines(stream,"SOLIDT10");
  PrintElementLines(stream,"SOLIDT4");
  PrintElementLines(stream,"SOLIDW6");
  PrintElementLines(stream,"TORSION2");
  PrintElementLines(stream,"TORSION3");
  PrintElementLines(stream,"TRUSS2");
  PrintElementLines(stream,"TRUSS3");
  PrintElementLines(stream,"WALL");

  PrintSectionHeader(stream,"FLUID ELEMENTS");
  PrintElementLines(stream,"COMBUST3");
  PrintElementLines(stream,"FLUID2");
  PrintElementLines(stream,"FLUID3");
  PrintElementLines(stream,"XDIFF3");
  PrintElementLines(stream,"XFLUID3");

  PrintSectionHeader(stream,"TRANSPORT ELEMENTS");
  //PrintElementLines(stream,"CONDIF2");
  //PrintElementLines(stream,"CONDIF3");
  PrintElementLines(stream,"TRANSP");

  PrintSectionHeader(stream,"ALE ELEMENTS");
  PrintElementLines(stream,"ALE2");
  PrintElementLines(stream,"ALE3");

  //PrintElementLines(stream,"BELE3");
  //PrintElementLines(stream,"VELE3");

  PrintSectionHeader(stream,"THERMO ELEMENTS");
  PrintElementLines(stream,"THERMO");
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::PrintSectionHeader(std::ostream& stream, std::string name, bool color)
{
  std::string blue2light = "";
  std::string bluelight = "";
  std::string redlight = "";
  std::string yellowlight = "";
  std::string greenlight = "";
  std::string magentalight = "";
  std::string endcolor = "";

  if (color)
  {
  }

  unsigned l = name.length();
  stream << redlight << "--";
  for (int i=0; i<std::max<int>(65-l,0); ++i) stream << '-';
  stream << greenlight << name << endcolor << '\n';
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::PrintElementLines(std::ostream& stream, std::string name)
{
  if (definitions_.find(name)!=definitions_.end())
  {
    std::map<std::string,LineDefinition>& defs = definitions_[name];
    for (std::map<std::string,LineDefinition>::iterator i=defs.begin(); i!=defs.end(); ++i)
    {
      stream << "// 0 " << name << " " << i->first << " ";
      i->second.Print(stream);
      stream << '\n';
    }
  }
  else
    stream << "no element type '" << name << "' defined\n";
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupValidElementLines()
{
  SetupArtLines();
  SetupBeam2Lines();
  SetupBeam2rLines();
  SetupBeam3Lines();
//   SetupConstrele2Lines();
//   SetupConstrele3Lines();
  SetupPtet4Lines();
  SetupShell8Lines();
  SetupSolid3Lines();
  SetupSolidh20Lines();
  SetupSolidh27Lines();
  SetupSolidh8Lines();
  SetupSolidh8p1j1Lines();
  SetupSolidsh8Lines();
  SetupSolidsh8p8Lines();
  SetupSolidshw6Lines();
  SetupSolidt10Lines();
  SetupSolidt4Lines();
  SetupSolidw6Lines();
  SetupTorsion2Lines();
  SetupTorsion3Lines();
  SetupTruss2Lines();
  SetupTruss3Lines();
  SetupWallLines();

  SetupCombust3Lines();
  SetupFluid2Lines();
  SetupFluid3Lines();
  SetupTranspLines();
  SetupXdiff3Lines();
  SetupXfluid3Lines();

  SetupAle2Lines();
  SetupAle3Lines();

  SetupThermoLines();

  // backward compatibility
  // still needed?
  //SetupCondif2Lines();
  //SetupCondif3Lines();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
DRT::INPUT::LineDefinition* DRT::INPUT::ElementDefinition::ElementLines(std::string name, std::string distype)
{
  // This is ugly. But we want to access both maps just once.
  std::map<std::string,std::map<std::string,LineDefinition> >::iterator j = definitions_.find(name);
  if (j!=definitions_.end())
  {
    std::map<std::string,LineDefinition>& defs = j->second;
    std::map<std::string,LineDefinition>::iterator i = defs.find(distype);
    if (i!=defs.end())
    {
      return &i->second;
    }
  }
  return NULL;
}




/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupArtLines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["ART"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedInt("GP")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedInt("GP")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupBeam2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["BEAM2"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupBeam2rLines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["BEAM2R"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LINE3"]
    .AddIntVector("LINE3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LIN3"]
    .AddIntVector("LIN3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LINE4"]
    .AddIntVector("LINE4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LIN4"]
    .AddIntVector("LIN4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LINE5"]
    .AddIntVector("LINE5",5)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;

  defs["LIN5"]
    .AddIntVector("LIN5",5)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("INERMOM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupBeam3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["BEAM3"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LINE3"]
    .AddIntVector("LINE3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LIN3"]
    .AddIntVector("LIN3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LINE4"]
    .AddIntVector("LINE4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LIN4"]
    .AddIntVector("LIN4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LINE5"]
    .AddIntVector("LINE5",5)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;

  defs["LIN5"]
    .AddIntVector("LIN5",5)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedDouble("SHEARCORR")
    .AddNamedDouble("MOMIN")
    //.AddNamedDouble("MOMIN")
    .AddNamedDouble("MOMINPOL")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupConstrele2Lines()
{
  // No reading for this element! Will be created on the fly, not from a .dat file.
  //std::map<std::string,LineDefinition>& defs = definitions_["CONSTRELE2"];
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupConstrele3Lines()
{
  // No reading for this element! Will be created on the fly, not from a .dat file.
  //std::map<std::string,LineDefinition>& defs = definitions_["CONSTRELE3"];
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupPtet4Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["PTET4"];

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupShell8Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SHELL8"];

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_TRI")
    .AddNamedString("FORCES")
    .AddNamedString("EAS")
    .AddString("EAS2")
    .AddString("EAS3")
    .AddString("EAS4")
    .AddString("EAS5")
    .AddNamedString("ANS")
    .AddNamedDouble("SDC")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_TRI")
    .AddNamedString("FORCES")
    .AddNamedString("EAS")
    .AddString("EAS2")
    .AddString("EAS3")
    .AddString("EAS4")
    .AddString("EAS5")
    .AddNamedString("ANS")
    .AddNamedDouble("SDC")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_TRI")
    .AddNamedString("FORCES")
    .AddNamedString("EAS")
    .AddString("EAS2")
    .AddString("EAS3")
    .AddString("EAS4")
    .AddString("EAS5")
    .AddNamedString("ANS")
    .AddNamedDouble("SDC")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_TRI")
    .AddNamedString("FORCES")
    .AddNamedString("EAS")
    .AddString("EAS2")
    .AddString("EAS3")
    .AddString("EAS4")
    .AddString("EAS5")
    .AddNamedString("ANS")
    .AddNamedDouble("SDC")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_TRI")
    .AddNamedString("FORCES")
    .AddNamedString("EAS")
    .AddString("EAS2")
    .AddString("EAS3")
    .AddString("EAS4")
    .AddString("EAS5")
    .AddNamedString("ANS")
    .AddNamedDouble("SDC")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolid3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLID3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedInt("GP_PYRAMID")
    .AddNamedInt("GP_TET")
    .AddNamedString("GP_ALT")
    .AddNamedString("KINEM")
    ;

}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidh20Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDH20"];

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedString("KINEM")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    .AddOptionalNamedDouble("STRENGTH")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidh27Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDH27"];

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    .AddNamedIntVector("GP",3)
    .AddNamedString("KINEM")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    .AddOptionalNamedDouble("STRENGTH")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidh8Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDH8"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    .AddNamedString("EAS")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    .AddOptionalNamedDouble("STRENGTH")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidh8p1j1Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDH8P1J1"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidsh8Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDSH8"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    .AddNamedString("EAS")
    .AddNamedString("THICKDIR")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    .AddOptionalNamedDouble("STRENGTH")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidsh8p8Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDSH8P8"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    .AddNamedString("STAB")
    .AddNamedString("ANS")
    .AddNamedString("LIN")
    .AddNamedString("THICKDIR")
    .AddNamedString("EAS")
    .AddNamedString("ISO")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    .AddOptionalNamedDouble("STRENGTH")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidshw6Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDSHW6"];

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    .AddNamedString("KINEM")
    .AddNamedString("EAS")
    .AddOptionalTag("OPTORDER")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidt10Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDT10"];

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidt4Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDT4"];

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupSolidw6Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["SOLIDW6"];

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    .AddNamedString("KINEM")
    .AddOptionalNamedDoubleVector("RAD",3)
    .AddOptionalNamedDoubleVector("AXI",3)
    .AddOptionalNamedDoubleVector("CIR",3)
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupTorsion2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["TORSION2"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupTorsion3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["TORSION3"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupTruss2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["TRUSS2"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupTruss3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["TRUSS3"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    .AddNamedDouble("CROSS")
    .AddNamedString("KINEM")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupWallLines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["WALL"];

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["NURBS4"]
    .AddIntVector("NURBS4",4)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;

  defs["NURBS9"]
    .AddIntVector("NURBS9",9)
    .AddNamedInt("MAT")
    .AddNamedDouble("THICK")
    .AddNamedIntVector("GP",2)
    .AddString("STRESS_STRAIN")
    .AddString("LAGRANGE")
    .AddString("EAS")
    //.AddNamedString("STRESSES")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupCombust3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["COMBUST3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupCondif2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["CONDIF2"];

  // backward compatibility
  defs = definitions_["TRANSP"];
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupCondif3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["CONDIF3"];

  // backward compatibility
  defs = definitions_["TRANSP"];
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupFluid2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["FLUID2"];

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["NURBS4"]
    .AddIntVector("NURBS4",4)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["NURBS9"]
    .AddIntVector("NURBS9",9)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["THQ9"]
    .AddIntVector("THQ9",9)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupFluid3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["FLUID3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["NURBS8"]
    .AddIntVector("NURBS8",8)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;

  defs["NURBS27"]
    .AddIntVector("NURBS27",27)
    .AddNamedInt("MAT")
    .AddNamedString("NA")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupTranspLines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["TRANSP"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    ;

  defs["NURBS4"]
    .AddIntVector("NURBS4",4)
    .AddNamedInt("MAT")
    ;

  defs["NURBS9"]
    .AddIntVector("NURBS9",9)
    .AddNamedInt("MAT")
    ;

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    ;

  defs["LINE3"]
    .AddIntVector("LINE3",3)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupXdiff3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["XDIFF3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;

  defs["NURBS8"]
    .AddIntVector("NURBS8",8)
    .AddNamedInt("MAT")
    ;

  defs["NURBS27"]
    .AddIntVector("NURBS27",27)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupXfluid3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["XFLUID3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;

  defs["NURBS8"]
    .AddIntVector("NURBS8",8)
    .AddNamedInt("MAT")
    ;

  defs["NURBS27"]
    .AddIntVector("NURBS27",27)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupAle2Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["ALE2"];

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupAle3Lines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["ALE3"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::ElementDefinition::SetupThermoLines()
{
  std::map<std::string,LineDefinition>& defs = definitions_["THERMO"];

  defs["HEX8"]
    .AddIntVector("HEX8",8)
    .AddNamedInt("MAT")
    ;

  defs["HEX20"]
    .AddIntVector("HEX20",20)
    .AddNamedInt("MAT")
    ;

  defs["HEX27"]
    .AddIntVector("HEX27",27)
    .AddNamedInt("MAT")
    ;

  defs["TET4"]
    .AddIntVector("TET4",4)
    .AddNamedInt("MAT")
    ;

  defs["TET10"]
    .AddIntVector("TET10",10)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE6"]
    .AddIntVector("WEDGE6",6)
    .AddNamedInt("MAT")
    ;

  defs["WEDGE15"]
    .AddIntVector("WEDGE15",15)
    .AddNamedInt("MAT")
    ;

  defs["PYRAMID5"]
    .AddIntVector("PYRAMID5",5)
    .AddNamedInt("MAT")
    ;

  defs["QUAD4"]
    .AddIntVector("QUAD4",4)
    .AddNamedInt("MAT")
    ;

  defs["QUAD8"]
    .AddIntVector("QUAD8",8)
    .AddNamedInt("MAT")
    ;

  defs["QUAD9"]
    .AddIntVector("QUAD9",9)
    .AddNamedInt("MAT")
    ;

  defs["TRI3"]
    .AddIntVector("TRI3",3)
    .AddNamedInt("MAT")
    ;

  defs["TRI6"]
    .AddIntVector("TRI6",6)
    .AddNamedInt("MAT")
    ;

  defs["NURBS4"]
    .AddIntVector("NURBS4",4)
    .AddNamedInt("MAT")
    ;

  defs["NURBS9"]
    .AddIntVector("NURBS9",9)
    .AddNamedInt("MAT")
    ;

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    ;

  defs["LINE3"]
    .AddIntVector("LINE3",3)
    .AddNamedInt("MAT")
    ;
}

#endif
