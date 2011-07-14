/*----------------------------------------------------------------------*/
/*!
\file drt_validparameters.cpp

\brief Setup of the list of valid input parameters

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/
/*----------------------------------------------------------------------*/

#ifdef CCADISCRET

#include <Teuchos_Array.hpp>
#include <Teuchos_StrUtils.hpp>
#include <Teuchos_any.hpp>

#include "drt_validparameters.H"
#include "../drt_lib/drt_colors.H"
#include "../drt_lib/standardtypes_cpp.H"
#include "../drt_inpar/inpar_solver.H"
#include "../drt_inpar/inpar_fluid.H"
#include "../drt_inpar/inpar_combust.H"
#include "../drt_inpar/inpar_mortar.H"
#include "../drt_inpar/inpar_contact.H"
#include "../drt_inpar/inpar_statmech.H"
#include "../drt_inpar/inpar_fsi.H"
#include "../drt_inpar/inpar_scatra.H"
#include "../drt_inpar/inpar_structure.H"
#include "../drt_inpar/inpar_potential.H"
#include "../drt_inpar/inpar_thermo.H"
#include "../drt_inpar/inpar_tsi.H"
#include "../drt_inpar/inpar_elch.H"
#include "../drt_inpar/inpar_invanalysis.H"
#include "../drt_inpar/inpar_searchtree.H"
#include "../drt_inpar/inpar_xfem.H"
#include "../drt_inpar/inpar_mlmc.H"

#include "../headers/fluid.h"
#include "../headers/dynamic.h"

#include <AztecOO.h>

/*----------------------------------------------------------------------*/
//! Print function to be called from C
/*----------------------------------------------------------------------*/
extern "C"
void PrintValidParameters()
{
  Teuchos::RCP<const Teuchos::ParameterList> list = DRT::INPUT::ValidParameters();
  list->print(std::cout,
              Teuchos::ParameterList::PrintOptions()
              .showDoc(true)
              .showFlags(false)
              .indent(4)
              .showTypes(false));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::PrintDatHeader(std::ostream& stream,
                                const Teuchos::ParameterList& list,
                                std::string parentname,
                                bool color,
                                bool comment)
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
    blue2light = BLUE2_LIGHT;
    bluelight = BLUE_LIGHT;
    redlight = RED_LIGHT;
    yellowlight = YELLOW_LIGHT;
    greenlight = GREEN_LIGHT;
    magentalight = MAGENTA_LIGHT;
    endcolor = END_COLOR;
  }

  // prevent invalid ordering of parameters caused by alphabetical output:
  // in the first run, print out all list elements that are not a sublist
  // in the second run, do the recursive call for all the sublists in the list
  for (int j=0; j<2; ++j)
  {
    for (Teuchos::ParameterList::ConstIterator i = list.begin();
    i!=list.end();
    ++i)
    {
      const Teuchos::ParameterEntry& entry = list.entry(i);
      if (entry.isList() && j==0) continue;
      if ((!entry.isList()) && j==1) continue;
      const std::string &name = list.name(i);
      if (name == PrintEqualSign()) continue;
      Teuchos::RCP<const Teuchos::ParameterEntryValidator> validator = entry.validator();

      if (comment)
      {
        stream << blue2light << "//" << endcolor << '\n';

        std::string doc = entry.docString();
        if (doc!="")
        {
          Teuchos::StrUtils::printLines(stream,blue2light + "// ",doc);
          stream << endcolor;
        }
      }

      if (entry.isList())
      {
        std::string secname = parentname;
        if (secname!="")
          secname += "/";
        secname += name;
        unsigned l = secname.length();
        stream << redlight << "--";
        for (int i=0; i<std::max<int>(65-l,0); ++i) stream << '-';
        stream << greenlight << secname << endcolor << '\n';
        PrintDatHeader(stream,list.sublist(name),secname,color,comment);
      }
      else
      {
        if (comment)
        if (validator!=Teuchos::null)
        {
          Teuchos::RCP<const Teuchos::Array<std::string> > values = validator->validStringValues();
          if (values!=Teuchos::null)
          {
            unsigned len = 0;
            for (int i=0; i<(int)values->size(); ++i)
            {
              len += (*values)[i].length()+1;
            }
            if (len<74)
            {
              stream << blue2light << "//     ";
              for (int i=0; i<static_cast<int>(values->size())-1; ++i)
              {
                stream << magentalight << (*values)[i] << blue2light << ",";
              }
              stream << magentalight << (*values)[values->size()-1] << endcolor << '\n';
            }
            else
            {
              for (int i=0; i<(int)values->size(); ++i)
              {
                stream << blue2light << "//     " << magentalight << (*values)[i] << endcolor << '\n';
              }
            }
          }
        }
        const Teuchos::any& v = entry.getAny(false);
        stream << bluelight << name << endcolor;
        unsigned l = name.length();
        for (int i=0; i<std::max<int>(31-l,0); ++i) stream << ' ';
        if (NeedToPrintEqualSign(list)) stream << " =";
        stream << ' ' << yellowlight << v << endcolor << '\n';
      }
    }
  }
}


/*----------------------------------------------------------------------*/
//! Print function to be called from C
/*----------------------------------------------------------------------*/
extern "C"
void PrintDefaultDatHeader()
{
  Teuchos::RCP<const Teuchos::ParameterList> list = DRT::INPUT::ValidParameters();
  DRT::INPUT::PrintDatHeader(std::cout,*list,"",true);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::PrintDefaultParameters(std::ostream& stream, const Teuchos::ParameterList& list)
{
  bool hasDefault = false;
  for (Teuchos::ParameterList::ConstIterator i = list.begin();
       i!=list.end();
       ++i)
  {
    const Teuchos::ParameterEntry& entry = list.entry(i);
    if (entry.isDefault())
    {
      if (not hasDefault)
      {
        hasDefault = true;
        stream << "default parameters in list '" << list.name() << "':\n";
      }
      const Teuchos::any& v = entry.getAny(false);
      int l = list.name(i).length();
      stream << "    " << list.name(i);
      for (int i=0; i<std::max<int>(31-l,0); ++i) stream << ' ';
      stream << ' ' << v << '\n';
    }
  }
  if (hasDefault)
    stream << "\n";
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::BoolParameter(std::string const& paramName,
                               std::string const& value,
                               std::string const& docString,
                               Teuchos::ParameterList* paramList)
{
  Teuchos::Array<std::string> yesnotuple = Teuchos::tuple<std::string>(
    "Yes",
    "No",
    "yes",
    "no",
    "YES",
    "NO");
  Teuchos::Array<int> yesnovalue = Teuchos::tuple<int>(
    true,
    false,
    true,
    false,
    true,
    false);
  Teuchos::setStringToIntegralParameter<int>(
    paramName,value,docString,
    yesnotuple,yesnovalue,
    paramList);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::IntParameter(std::string const &paramName,
                              int const value,
                              std::string const &docString,
                              Teuchos::ParameterList *paramList)
{
  Teuchos::AnyNumberParameterEntryValidator::AcceptedTypes validator(false);
  validator.allowInt(true);
  Teuchos::setIntParameter(paramName,value,
                           docString,
                           paramList,validator);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::DoubleParameter(std::string const &paramName,
                                 double const &value,
                                 std::string const &docString,
                                 Teuchos::ParameterList *paramList)
{
  Teuchos::AnyNumberParameterEntryValidator::AcceptedTypes validator(false);
  validator.allowDouble(true);
  validator.allowInt(true);
  Teuchos::setDoubleParameter(paramName,value,
                              docString,
                              paramList,validator);
}

#if 0
namespace DRT
{
namespace INPUT
{

  template <class T>
  Teuchos::Array<T> tuple( const T & t1 )
  {
    Teuchos::Array<T> a;
    a.reserve(1);
    a.push_back(t1);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2 )
  {
    Teuchos::Array<T> a;
    a.reserve(2);
    a.push_back(t1);
    a.push_back(t2);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3 )
  {
    Teuchos::Array<T> a;
    a.reserve(3);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4 )
  {
    Teuchos::Array<T> a;
    a.reserve(4);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5 )
  {
    Teuchos::Array<T> a;
    a.reserve(5);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6 )
  {
    Teuchos::Array<T> a;
    a.reserve(6);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7 )
  {
    Teuchos::Array<T> a;
    a.reserve(7);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8 )
  {
    Teuchos::Array<T> a;
    a.reserve(8);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9 )
  {
    Teuchos::Array<T> a;
    a.reserve(9);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10 )
  {
    Teuchos::Array<T> a;
    a.reserve(10);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10, const T & t11 )
  {
    Teuchos::Array<T> a;
    a.reserve(11);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    a.push_back(t11);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10, const T & t11, const T & t12 )
  {
    Teuchos::Array<T> a;
    a.reserve(12);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    a.push_back(t11);
    a.push_back(t12);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10, const T & t11, const T & t12, const T & t13 )
  {
    Teuchos::Array<T> a;
    a.reserve(13);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    a.push_back(t11);
    a.push_back(t12);
    a.push_back(t13);
    return a;
  }

  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10, const T & t11, const T & t12, const T & t13, const T & t14 )
  {
    Teuchos::Array<T> a;
    a.reserve(14);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    a.push_back(t11);
    a.push_back(t12);
    a.push_back(t13);
    a.push_back(t14);
    return a;
  }


  template <class T>
  Teuchos::Array<T> tuple( const T & t1, const T & t2, const T & t3, const T & t4, const T & t5, const T & t6, const T & t7, const T & t8, const T & t9, const T & t10, const T & t11, const T & t12, const T & t13, const T & t14, const T & t15 )
  {
    Teuchos::Array<T> a;
    a.reserve(15);
    a.push_back(t1);
    a.push_back(t2);
    a.push_back(t3);
    a.push_back(t4);
    a.push_back(t5);
    a.push_back(t6);
    a.push_back(t7);
    a.push_back(t8);
    a.push_back(t9);
    a.push_back(t10);
    a.push_back(t11);
    a.push_back(t12);
    a.push_back(t13);
    a.push_back(t14);
    a.push_back(t15);
    return a;
  }

}
}
#endif

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Teuchos::ParameterList> DRT::INPUT::ValidParameters()
{
  using Teuchos::tuple;
  using Teuchos::setStringToIntegralParameter;

  Teuchos::Array<std::string> yesnotuple = tuple<std::string>("Yes","No","yes","no","YES","NO");
  Teuchos::Array<int> yesnovalue = tuple<int>(true,false,true,false,true,false);

  Teuchos::RCP<Teuchos::ParameterList> list = Teuchos::rcp(new Teuchos::ParameterList);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& discret = list->sublist("DISCRETISATION",false,"");

  IntParameter("NUMFLUIDDIS",1,"Number of meshes in fluid field",&discret);
  IntParameter("NUMSTRUCDIS",1,"Number of meshes in structural field",&discret);
  IntParameter("NUMALEDIS",1,"Number of meshes in ale field",&discret);
  IntParameter("NUMARTNETDIS",1,"Number of meshes in arterial network field",&discret);
  IntParameter("NUMTHERMDIS",1,"Number of meshes in thermal field",&discret);
  IntParameter("NUMAIRWAYSDIS",1,"Number of meshes in reduced dimensional airways network field",&discret);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& size = list->sublist("PROBLEM SIZE",false,"");

  IntParameter("ELEMENTS",0,"Total number of elements",&size);
  IntParameter("NODES",0,"Total number of nodes",&size);
  IntParameter("NPATCHES",0,"number of nurbs patches",&size);
  IntParameter("DIM",3,"2d or 3d problem",&size);
  IntParameter("MATERIALS",0,"number of materials",&size);
  IntParameter("NUMDF",3,"maximum number of degrees of freedom",&size);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& type = list->sublist("PROBLEM TYP",false,"");

  {
    Teuchos::Array<std::string> name;
    Teuchos::Array<int> label;
    name.push_back("Structure");                                   label.push_back(prb_structure);
    name.push_back("Structure_Ale");                               label.push_back(prb_struct_ale);
    name.push_back("Fluid");                                       label.push_back(prb_fluid);
    name.push_back("Fluid_XFEM");                                  label.push_back(prb_fluid_xfem);
    name.push_back("Fluid_XFEM2");                                 label.push_back(prb_fluid_xfem2);
    name.push_back("Fluid_Fluid_Ale");                             label.push_back(prb_fluid_fluid_ale);
    name.push_back("Fluid_Fluid");                                 label.push_back(prb_fluid_fluid);
    name.push_back("Fluid_Ale");                                   label.push_back(prb_fluid_ale);
    name.push_back("Fluid_Freesurface");                           label.push_back(prb_freesurf);
    name.push_back("Scalar_Transport");                            label.push_back(prb_scatra);
    name.push_back("Fluid_Structure_Interaction");                 label.push_back(prb_fsi);
    name.push_back("Fluid_Structure_Interaction_XFEM");            label.push_back(prb_fsi_xfem);
    name.push_back("Ale");                                         label.push_back(prb_ale);
    name.push_back("Thermo_Structure_Interaction");                label.push_back(prb_tsi);
    name.push_back("Thermo");                                      label.push_back(prb_thermo);
    name.push_back("Low_Mach_Number_Flow");                        label.push_back(prb_loma);
    name.push_back("Electrochemistry");                            label.push_back(prb_elch);
    name.push_back("Combustion");                                  label.push_back(prb_combust);
    name.push_back("ArterialNetwork");                             label.push_back(prb_art_net);
    name.push_back("Fluid_Structure_Interaction_Lung");            label.push_back(prb_fsi_lung);
    name.push_back("ReducedDimensionalAirWays");                   label.push_back(prb_red_airways);
    name.push_back("Fluid_Structure_Interaction_Lung_Gas");        label.push_back(prb_fsi_lung_gas);
    setStringToIntegralParameter<int>(
      "PROBLEMTYP",
      "Fluid_Structure_Interaction",
      "",
      name,
      label,
      &type);
  }

  IntParameter("NUMFIELD",1,"",&type);
  setStringToIntegralParameter<int>("TIMETYP","Dynamic","",
                               tuple<std::string>("Static","Dynamic"),
                               tuple<int>(time_static,time_dynamic),
                               &type);
  //IntParameter("GRADERW",0,"",&type);
  IntParameter("MULTISC_STRUCT",0,"",&type);
  IntParameter("RESTART",0,"",&type);
  setStringToIntegralParameter<int>("ALGEBRA","Trilinos","outdated",
                               tuple<std::string>("Trilinos","ccarat"),
                               tuple<int>(1,0),
                               &type);
  setStringToIntegralParameter<int>("SHAPEFCT","Polynomial","Defines the function spaces for the spatial approximation",
                               tuple<std::string>("Polynomial","Nurbs"),
                               tuple<int>(1,0),
                               &type);

  setStringToIntegralParameter<int>("ADAPTIVE","No",
                                    "If on switches to spacial adaptive algorithms",
                                    yesnotuple,yesnovalue,&type);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& ps = list->sublist("PATIENT SPECIFIC",false,"");

  setStringToIntegralParameter<int>("PATSPEC","No",
                                    "Triggers application of patient specific tools in discretization construction",
                                    yesnotuple,yesnovalue,&ps);

  setStringToIntegralParameter<int>("PRESTRESS","none","prestressing takes values none mulf id",
                               tuple<std::string>("none","None","NONE",
                                                  "mulf","Mulf","MULF",
                                                  "id","Id","ID"),
                               tuple<int>(INPAR::STR::prestress_none,INPAR::STR::prestress_none,INPAR::STR::prestress_none,
                                                            INPAR::STR::prestress_mulf,INPAR::STR::prestress_mulf,INPAR::STR::prestress_mulf,
                                                            INPAR::STR::prestress_id,INPAR::STR::prestress_id,INPAR::STR::prestress_id),
                               &ps);

  DoubleParameter("PRESTRESSTIME",0.0,"time to switch from pre to post stressing",&ps);

  BoolParameter("REMODEL","No","Turn remodeling on/off",&ps);

  setNumericStringParameter("CENTERLINEFILE","name.txt",
                            "filename of file containing centerline points",
                            &ps);

  setStringToIntegralParameter<int>("CALCSTRENGTH","No","Calculate strength on/off",yesnotuple,yesnovalue,&ps);
  DoubleParameter("AAA_SUBRENDIA",22.01,"subrenal diameter of the AAA",&ps);
  setStringToIntegralParameter<int>("FAMILYHIST","No","Does the patient have AAA family history",yesnotuple,yesnovalue,&ps);
  setStringToIntegralParameter<int>("MALE_PATIENT","Yes","Is the patient a male?",yesnotuple,yesnovalue,&ps);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& io = list->sublist("IO",false,"");

  // are these needed?
  setStringToIntegralParameter<int>("OUTPUT_OUT","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("OUTPUT_GID","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("OUTPUT_BIN","No","",yesnotuple,yesnovalue,&io);

  setStringToIntegralParameter<int>("STRUCT_DISP","Yes","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("STRUCT_SE","No","output of strain energy",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("STRUCT_STRESS","No","",
                               tuple<std::string>("No","no","NO",
                                                  "Yes","yes","YES",
                                                  "Cauchy","cauchy",
                                                  "2PK", "2pk"),
                               tuple<int>(INPAR::STR::stress_none,INPAR::STR::stress_none,INPAR::STR::stress_none,
                                                             INPAR::STR::stress_2pk,INPAR::STR::stress_2pk,INPAR::STR::stress_2pk,
                                                             INPAR::STR::stress_cauchy,INPAR::STR::stress_cauchy,
                                                             INPAR::STR::stress_2pk,INPAR::STR::stress_2pk),
                               &io);
  setStringToIntegralParameter<int>("STRUCT_STRAIN","No","",
                               tuple<std::string>("No","no","NO",
                                                  "Yes","yes","YES",
                                                  "EA","ea",
                                                  "GL", "gl"),
                               tuple<int>(INPAR::STR::strain_none,INPAR::STR::strain_none,INPAR::STR::strain_none,
                                                             INPAR::STR::strain_gl,INPAR::STR::strain_gl,INPAR::STR::strain_gl,
                                                             INPAR::STR::strain_ea,INPAR::STR::strain_ea,
                                                             INPAR::STR::strain_gl,INPAR::STR::strain_gl),
                               &io);
  setStringToIntegralParameter<int>("STRUCT_PLASTIC_STRAIN","No","",
                               tuple<std::string>("No","no","NO",
                                                  "Yes","yes","YES",
                                                  "EA","ea",
                                                  "GL", "gl"),
                               tuple<int>(INPAR::STR::strain_none,INPAR::STR::strain_none,INPAR::STR::strain_none,
                                                             INPAR::STR::strain_gl,INPAR::STR::strain_gl,INPAR::STR::strain_gl,
                                                             INPAR::STR::strain_ea,INPAR::STR::strain_ea,
                                                             INPAR::STR::strain_gl,INPAR::STR::strain_gl),
                               &io);
  setStringToIntegralParameter<int>("STRUCT_SURFACTANT","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("STRUCT_SM_DISP","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("FLUID_SOL","Yes","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("FLUID_STRESS","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("FLUID_WALL_SHEAR_STRESS","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("FLUID_VIS","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("ALE_DISP","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("THERM_TEMPERATURE","No","",yesnotuple,yesnovalue,&io);
  setStringToIntegralParameter<int>("THERM_HEATFLUX","None","",
                               tuple<std::string>("None",
                                                  "No",
                                                  "NO",
                                                  "no",
                                                  "Current",
                                                  "Initial"),
                               tuple<int>(INPAR::THR::heatflux_none,
                                                               INPAR::THR::heatflux_none,
                                                               INPAR::THR::heatflux_none,
                                                               INPAR::THR::heatflux_none,
                                                               INPAR::THR::heatflux_current,
                                                               INPAR::THR::heatflux_initial),
                               &io);
  setStringToIntegralParameter<int>("THERM_TEMPGRAD","None","",
                               tuple<std::string>("None",
                                                  "No",
                                                  "NO",
                                                  "no",
                                                  "Current",
                                                  "Initial"),
                               tuple<int>(INPAR::THR::tempgrad_none,
                                                               INPAR::THR::tempgrad_none,
                                                               INPAR::THR::tempgrad_none,
                                                               INPAR::THR::tempgrad_none,
                                                               INPAR::THR::tempgrad_current,
                                                               INPAR::THR::tempgrad_initial),
                               &io);

  IntParameter("FILESTEPS",1000,"",&io);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& design = list->sublist("DESIGN DESCRIPTION",false,"number of nodal clouds");

  IntParameter("NDPOINT",0,"number of points",&design);
  IntParameter("NDLINE",0,"number of line clouds",&design);
  IntParameter("NDSURF",0,"number of surface clouds",&design);
  IntParameter("NDVOL",0,"number of volume clouds",&design);

  /*----------------------------------------------------------------------*/
  // An empty list. The actual list is arbitrary and not validated.
  Teuchos::ParameterList& condition =
    list->sublist("CONDITION NAMES",false,
                "Names of conditions from exodus file.\n"
                "This section is not validated, any variable is allowed here.\n"
                "The names defined in this section can be used by all conditions instead of\n"
                "a design object number. This section assigns the respective numbers to\n"
                "the names.");

  condition.disableRecursiveValidation();

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& stat = list->sublist("STATIC",false,"");

  //IntParameter("LINEAR",0,"",&stat);
  //IntParameter("NONLINEAR",0,"",&stat);

  setStringToIntegralParameter<int>("NEWTONRAPHSO","Load_Control",
                                    "",
                                    tuple<std::string>("Load_Control"),
                                    tuple<int>(control_load),
                                    &stat);

  IntParameter("MAXITER",0,"",&stat);
  IntParameter("NUMSTEP",0,"",&stat);
  IntParameter("RESULTSEVRY",1,"",&stat);
  IntParameter("RESTARTEVRY",20,"",&stat);

  DoubleParameter("TOLDISP",0.0,"",&stat);
  DoubleParameter("STEPSIZE",0.0,"",&stat);

  /*--------------------------------------------------------------------*/
  /* parameters for NOX - non-linear solution */
  Teuchos::ParameterList& snox = list->sublist("STRUCT NOX",false,"");
  SetValidNoxParameters(snox);


  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& sdyn = list->sublist("STRUCTURAL DYNAMIC",false,"");

  setStringToIntegralParameter<int>("DYNAMICTYP","GenAlpha",
                               "type of time integration control",
                               tuple<std::string>(
                                 "Centr_Diff",
                                 "Gen_EMM",
                                 "Gen_Alfa",
                                 "Static",
                                 "Statics",
                                 "GenAlpha",
                                 "OneStepTheta",
                                 "GEMM",
                                 "AdamsBashforth2",
                                 "EulerMaruyama",
                                 "EulerImpStoch"),
                               tuple<int>(
                                 INPAR::STR::dyna_centr_diff,
                                 INPAR::STR::dyna_Gen_EMM,
                                 INPAR::STR::dyna_gen_alfa,
                                 INPAR::STR::dyna_gen_alfa_statics,
                                 INPAR::STR::dyna_statics,
                                 INPAR::STR::dyna_genalpha,
                                 INPAR::STR::dyna_onesteptheta,
                                 INPAR::STR::dyna_gemm,
                                 INPAR::STR::dyna_ab2,
                                 INPAR::STR::dyna_euma,
                                 INPAR::STR::dyna_euimsto),
                               &sdyn);
  // a temporary flag
  setStringToIntegralParameter<int>("ADAPTERDRIVE","No",
                                    "TEMPORARY FLAG: Switch on time integration driver based on ADAPTER::Structure rather than independent implementation",
                                    yesnotuple,yesnovalue,&sdyn);

  // Output type
  IntParameter("EIGEN",0,"EIGEN make eigenanalysis of the initial dynamic system",&sdyn);
  IntParameter("RESULTSEVRY",1,"save displacements and contact forces every RESULTSEVRY steps",&sdyn);
  IntParameter("RESEVRYERGY",0,"write system energies every requested step",&sdyn);
  IntParameter("RESTARTEVRY",1,"write restart possibility every RESTARTEVRY steps",&sdyn);
  // Time loop control
  DoubleParameter("TIMESTEP",0.05,"time step size",&sdyn);
  IntParameter("NUMSTEP",200,"maximum number of steps",&sdyn);
  DoubleParameter("MAXTIME",5.0,"maximum time",&sdyn);
  // Generalised-alpha parameters
  DoubleParameter("BETA",0.25,"generalized alpha factors, also used by explicit time integration",&sdyn);
  DoubleParameter("DELTA",0.25,"generalized alpha factors",&sdyn);
  DoubleParameter("GAMMA",0.5,"generalized alpha factors, also used by explicit time integration",&sdyn);
  DoubleParameter("ALPHA_M",0.5,"generalized alpha factors",&sdyn);
  DoubleParameter("ALPHA_F",0.5,"generalized alpha factors",&sdyn);
  // Damping
  setStringToIntegralParameter<int>("DAMPING","No",
                               "type of damping: (1) Rayleigh damping matrix and use it from M_DAMP x M + K_DAMP x K, (2) Material based and calculated in elements",
                               tuple<std::string>(
                                 "no",
                                 "No",
                                 "NO",
                                 "yes",
                                 "Yes",
                                 "YES",
                                 "Rayleigh",
                                 "Material",
                                 "BrownianMotion"),
                               tuple<int>(
                                 INPAR::STR::damp_none,
                                 INPAR::STR::damp_none,
                                 INPAR::STR::damp_none,
                                 INPAR::STR::damp_rayleigh,
                                 INPAR::STR::damp_rayleigh,
                                 INPAR::STR::damp_rayleigh,
                                 INPAR::STR::damp_rayleigh,
                                 INPAR::STR::damp_material,
                                 INPAR::STR::damp_brownianmotion),
                               &sdyn);
  DoubleParameter("M_DAMP",0.5,"",&sdyn);
  DoubleParameter("K_DAMP",0.5,"",&sdyn);
  // Iteration
  setStringToIntegralParameter<int>("ITERATION","full","unused",
                               tuple<std::string>("full","Full","FULL"),
                               tuple<int>(1,1,1),
                               &sdyn);
  setStringToIntegralParameter<int>("CONV_CHECK","AbsRes_Or_AbsDis","type of convergence check",
                               tuple<std::string>(
                                 "AbsRes_Or_AbsDis",
                                 "AbsRes_And_AbsDis",
                                 "RelRes_Or_AbsDis",
                                 "RelRes_And_AbsDis",
                                 "RelRes_Or_RelDis",
                                 "RelRes_And_RelDis",
                                 "MixRes_Or_MixDis",
                                 "MixRes_And_MixDis",
                                 "None"),
                               tuple<int>(
                                 INPAR::STR::convcheck_absres_or_absdis,
                                 INPAR::STR::convcheck_absres_and_absdis,
                                 INPAR::STR::convcheck_relres_or_absdis,
                                 INPAR::STR::convcheck_relres_and_absdis,
                                 INPAR::STR::convcheck_relres_or_reldis,
                                 INPAR::STR::convcheck_relres_and_reldis,
                                 INPAR::STR::convcheck_mixres_or_mixdis,
                                 INPAR::STR::convcheck_mixres_and_mixdis,
                                 INPAR::STR::convcheck_vague),
                               &sdyn);

  DoubleParameter("TOLDISP",1.0E-10,
                  "tolerance in the displacement norm for the newton iteration",
                  &sdyn);
  setStringToIntegralParameter<int>("NORM_DISP","Abs","type of norm for displacement convergence check",
                               tuple<std::string>(
                                 "Abs",
                                 "Rel",
                                 "Mix"),
                               tuple<int>(
                                 INPAR::STR::convnorm_abs,
                                 INPAR::STR::convnorm_rel,
                                 INPAR::STR::convnorm_mix),
                               &sdyn);

  DoubleParameter("TOLRES",1.0E-08,
                  "tolerance in the residual norm for the newton iteration",
                  &sdyn);
  setStringToIntegralParameter<int>("NORM_RESF","Abs","type of norm for residual convergence check",
                               tuple<std::string>(
                                 "Abs",
                                 "Rel",
                                 "Mix"),
                               tuple<int>(
                                 INPAR::STR::convnorm_abs,
                                 INPAR::STR::convnorm_rel,
                                 INPAR::STR::convnorm_mix),
                               &sdyn);

  DoubleParameter("TOLPRE",1.0E-08,
                  "tolerance in pressure norm for the newton iteration",
                  &sdyn);
  setStringToIntegralParameter<int>("NORM_PRES","Abs","type of norm for pressure convergence check",
                               tuple<std::string>(
                                 "Abs"),
                               tuple<int>(
                                 INPAR::STR::convnorm_abs),
                               &sdyn);

  DoubleParameter("TOLINCO",1.0E-08,
                  "tolerance in the incompressible residual norm for the newton iteration",
                  &sdyn);
  setStringToIntegralParameter<int>("NORM_INCO","Abs","type of norm for incompressible residual convergence check",
                               tuple<std::string>(
                                 "Abs"),
                               tuple<int>(
                                 INPAR::STR::convnorm_abs),
                               &sdyn);

  setStringToIntegralParameter<int>("NORMCOMBI_DISPPRES","And","binary operator to combine pressure and displacement values",
                               tuple<std::string>(
                                 "And",
                                 "Or"),
                               tuple<int>(
                                 INPAR::STR::bop_and,
                                 INPAR::STR::bop_or),
                               &sdyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFINCO","And","binary operator to combine force and incompressible residual",
                               tuple<std::string>(
                                 "And",
                                 "Or"),
                               tuple<int>(
                                 INPAR::STR::bop_and,
                                 INPAR::STR::bop_or),
                               &sdyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFDISP","And","binary operator to combine displacement and residual force values",
                               tuple<std::string>(
                                 "And",
                                 "Or"),
                               tuple<int>(
                                 INPAR::STR::bop_and,
                                 INPAR::STR::bop_or),
                               &sdyn);

  setStringToIntegralParameter<int>("STC_SCALING","no",
      "Scaled director conditioning for thin shell structures",
      tuple<std::string>(
        "no",
        "No",
        "NO",
        "Symmetric",
        "Right"),
      tuple<int>(
        INPAR::STR::stc_none,
        INPAR::STR::stc_none,
        INPAR::STR::stc_none,
        INPAR::STR::stc_currsym,
        INPAR::STR::stc_curr),
      &sdyn);


  IntParameter("STC_LAYER",1,
               "number of STC layers for multilayer case",
               &sdyn);

  DoubleParameter("TOLCONSTR",1.0E-08,
                  "tolerance in the constr error norm for the newton iteration",
                  &sdyn);
  IntParameter("MAXITER",50,
               "maximum number of iterations allowed for Newton-Raphson iteration before failure",
               &sdyn);
  IntParameter("MINITER",0,
               "minimum number of iterations to be done within Newton-Raphson loop",
               &sdyn);
  setStringToIntegralParameter<int>("ITERNORM","L2","type of norm to be applied to residuals",
                               tuple<std::string>(
                                 "L1",
                                 "L2",
                                 "Rms",
                                 "Inf"),
                               tuple<int>(
                                 INPAR::STR::norm_l1,
                                 INPAR::STR::norm_l2,
                                 INPAR::STR::norm_rms,
                                 INPAR::STR::norm_inf),
                               &sdyn);
  setStringToIntegralParameter<int>("DIVERCONT","No",
                                    "Go on with time integration even if Newton-Raphson iteration failed",
                                    yesnotuple,yesnovalue,&sdyn);

  setStringToIntegralParameter<int>("NLNSOL","fullnewton","Nonlinear solution technique",
                               tuple<std::string>(
                                 "vague",
                                 "fullnewton",
                                 "lsnewton",
                                 "oppnewton",
                                 "modnewton",
                                 "nlncg",
                                 "ptc",
                                 "newtonlinuzawa",
                                 "augmentedlagrange",
                                 "NoxNewtonLineSearch",
                                 "noxgeneral"),
                               tuple<int>(
                                 INPAR::STR::soltech_vague,
                                 INPAR::STR::soltech_newtonfull,
                                 INPAR::STR::soltech_newtonls,
                                 INPAR::STR::soltech_newtonopp,
                                 INPAR::STR::soltech_newtonmod,
                                 INPAR::STR::soltech_nlncg,
                                 INPAR::STR::soltech_ptc,
                                 INPAR::STR::soltech_newtonuzawalin,
                                 INPAR::STR::soltech_newtonuzawanonlin,
                                 INPAR::STR::soltech_noxnewtonlinesearch,
                                 INPAR::STR::soltech_noxgeneral),
                               &sdyn);

  setStringToIntegralParameter<int>("CONTROLTYPE","load","load, disp, arc1, arc2 control",
                               tuple<std::string>(
                                 "load",
                                 "Load",
                                 "disp",
                                 "Disp",
                                 "Displacement",
                                 "arc1",
                                 "Arc1",
                                 "arc2",
                                 "Arc2"),
                               tuple<int>(
                                 INPAR::STR::control_load,
                                 INPAR::STR::control_load,
                                 INPAR::STR::control_disp,
                                 INPAR::STR::control_disp,
                                 INPAR::STR::control_disp,
                                 INPAR::STR::control_arc1,
                                 INPAR::STR::control_arc1,
                                 INPAR::STR::control_arc2,
                                 INPAR::STR::control_arc2),
                               &sdyn);

  setNumericStringParameter("CONTROLNODE","-1 -1 -1",
                            "for methods other than load control: [node(fortran numbering)] [dof(c-numbering)] [curve(fortran numbering)]",
                            &sdyn);

  setStringToIntegralParameter<int>("LOADLIN","no",
                                    "Use linearization of external follower load in Newton",
                                    yesnotuple,yesnovalue,&sdyn);

  setStringToIntegralParameter<int>("PREDICT","ConstDis","",
                               tuple<std::string>(
                                 "Vague",
                                 "ConstDis",
                                 "ConstDisVelAcc",
                                 "TangDis",
                                 "ConstDisPres",
                                 "ConstDisVelAccPres"),
                               tuple<int>(
                                 INPAR::STR::pred_vague,
                                 INPAR::STR::pred_constdis,
                                 INPAR::STR::pred_constdisvelacc,
                                 INPAR::STR::pred_tangdis,
                                 INPAR::STR::pred_constdispres,
                                 INPAR::STR::pred_constdisvelaccpres),
                               &sdyn);

  // time adaptivity (old style)
  IntParameter("TIMEADAPT",0,"",&sdyn);
  IntParameter("ITWANT",0,"",&sdyn);
  DoubleParameter("MAXDT",0.0,"",&sdyn);
  DoubleParameter("RESULTDT",0.0,"",&sdyn);
  // Uzawa iteration for constraint systems
  DoubleParameter("UZAWAPARAM",1.0,"Parameter for Uzawa algorithm dealing with lagrange multipliers",&sdyn);
  DoubleParameter("UZAWATOL",1.0E-8,"Tolerance for iterative solve with Uzawa algorithm",&sdyn);
  IntParameter("UZAWAMAXITER",50,"maximum number of iterations allowed for uzawa algorithm before failure going to next newton step",&sdyn);
  setStringToIntegralParameter<int>("UZAWAALGO","direct","",
                                 tuple<std::string>(
                                   "uzawa",
                                   "simple",
                                   "direct"),
                                 tuple<int>(
                                   INPAR::STR::consolve_uzawa,
                                   INPAR::STR::consolve_simple,
                                   INPAR::STR::consolve_direct),
                                 &sdyn);

  // convergence criteria adaptivity
  setStringToIntegralParameter<int>("ADAPTCONV","No",
                               "Switch on adaptive control of linear solver tolerance for nonlinear solution",
                               yesnotuple,yesnovalue,&sdyn);
  DoubleParameter("ADAPTCONV_BETTER",0.1,"The linear solver shall be this much better than the current nonlinear residual in the nonlinear convergence limit",&sdyn);

  /*--------------------------------------------------------------------*/
  /* parameters for time step size adaptivity in structural dynamics */
  Teuchos::ParameterList& tap = sdyn.sublist("TIMEADAPTIVITY",false,"");
  SetValidTimeAdaptivityParameters(tap);

  /*----------------------------------------------------------------------*/
  /* parameters for generalised-alpha structural integrator */
  Teuchos::ParameterList& genalpha = sdyn.sublist("GENALPHA",false,"");

  setStringToIntegralParameter<int>("GENAVG","ImrLike",
                               "mid-average type of internal forces",
                               tuple<std::string>(
                                 "Vague",
                                 "ImrLike",
                                 "TrLike"),
                               tuple<int>(
                                 INPAR::STR::midavg_vague,
                                 INPAR::STR::midavg_imrlike,
                                 INPAR::STR::midavg_trlike),
                               &genalpha);
  DoubleParameter("BETA",0.25,"Generalised-alpha factor in (0,1/2]",&genalpha);
  DoubleParameter("GAMMA",0.5,"Generalised-alpha factor in (0,1]",&genalpha);
  DoubleParameter("ALPHA_M",0.5,"Generalised-alpha factor in [0,1)",&genalpha);
  DoubleParameter("ALPHA_F",0.5,"Generalised-alpha factor in [0,1)",&genalpha);

  /*----------------------------------------------------------------------*/
  /* parameters for one-step-theta structural integrator */
  Teuchos::ParameterList& onesteptheta = sdyn.sublist("ONESTEPTHETA",false,"");

  DoubleParameter("THETA",0.5,"One-step-theta factor in (0,1]",&onesteptheta);


  /*----------------------------------------------------------------------*/
  /* parameters for generalised-energy-momentum structural integrator */
  Teuchos::ParameterList& gemm = sdyn.sublist("GEMM",false,"");

  DoubleParameter("BETA",0.25,"Generalised-alpha factor in (0,0.5]",&gemm);
  DoubleParameter("GAMMA",0.5,"Generalised-alpha factor in (0,1]",&gemm);
  DoubleParameter("ALPHA_M",0.5,"Generalised-alpha factor in [0,1)",&gemm);
  DoubleParameter("ALPHA_F",0.5,"Generalised-alpha factor in [0,1)",&gemm);
  DoubleParameter("XI",0.0,"generalisation factor in [0,1)",&gemm);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& iap = list->sublist("INVERSE ANALYSIS",false,"");

  // Inverse Analysis
  setStringToIntegralParameter<int>("INV_ANALYSIS","none",
                               "types of inverse analysis and on/off switch",
                               tuple<std::string>(
                                 "none",
                                 "lung",
                                 "gen"),
                               tuple<int>(
                                 INPAR::STR::inv_none,
                                 INPAR::STR::inv_lung,
                                 INPAR::STR::inv_generalized),
                               &iap);


  DoubleParameter("MC_X_0",0.0,"measured displacment of the tension testing in x dir",&iap);
  DoubleParameter("MC_X_1",0.0,"measured displacment of the tension testing in x dir",&iap);
  DoubleParameter("MC_X_2",0.0,"measured displacment of the tension testing in x dir",&iap);
  DoubleParameter("MC_Y_0",0.0,"measured displacment of the tension testing in y dir",&iap);
  DoubleParameter("MC_Y_1",0.0,"measured displacment of the tension testing in y dir",&iap);
  DoubleParameter("MC_Y_2",0.0,"measured displacment of the tension testing in y dir",&iap);



  // tolerance for inv_analysis
  DoubleParameter("INV_ANA_TOL",1.0,"tolerance for inverse analysis",&iap);
  IntParameter("INV_ANA_MAX_RUN",100,"max iterations for inverse analysis",&iap);

  // perturbation parameters
  DoubleParameter("INV_ALPHA",1.0e-3,"perturbation parameters",&iap);
  DoubleParameter("INV_BETA",1.0e-3,"perturbation parameters",&iap);

  // initial regularization parameter
  DoubleParameter("INV_INITREG",1.0,"initial regularization parameter",&iap);


  setNumericStringParameter("MONITORFILE","none.monitor",
                            "filename of file containing measured displacements",
                            &iap);

  setNumericStringParameter("INV_LIST","-1",
                            "IDs of materials that have to be fitted",
                            &iap);

  setNumericStringParameter("INV_EH_LIST","-1",
                            "IDs of materials that have to be fitted",
                            &iap);

  setStringToIntegralParameter<int>("INV_MULTI_OUT","no",
                                    "output for optimization on micro-scale",
                                    yesnotuple,yesnovalue,&iap);

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
   Teuchos::ParameterList& mlmcp = list->sublist("MULTI LEVEL MONTE CARLO",false,"");

   setStringToIntegralParameter<int>("MLMC","no",
                                     "perform multi level monte carlo analysis",
                                     yesnotuple,yesnovalue,&mlmcp);
   IntParameter("NUMRUNS",200,"Number of Monte Carlo runs",&mlmcp);

   //IntParameter("NUMLEVELS",2,"Number of levels",&mlmcp);
   setStringToIntegralParameter<int>("DIFF_TO_LOWER_LEVEL","no","calculate difference to next lower level",yesnotuple,yesnovalue,&mlmcp);
   IntParameter("START_RUN",0,"Run to start calculating the difference to lower level", &mlmcp);
   //IntParameter("END_RUN",0,"Run to stop calculating the difference to lower level", &mlmcp);
   // NUMLEVEL additional inputfiles are read name must be standard_inputfilename+_level_i.dat
   setNumericStringParameter("DISCRETIZATION_FOR_PROLONGATION","filename.dat",
                          "filename of.dat file which contains discretization to which the results are prolongated",
                          &mlmcp);
   setNumericStringParameter("OUTPUT_FILE_OF_LOWER_LEVEL","level0",
                             "filename of controlfiles of next lower level",
                             &mlmcp);
   setStringToIntegralParameter<int>("PROLONGATERES","Yes",
                                        "Prolongate Displacements to finest Discretization",
                                        yesnotuple,yesnovalue,&mlmcp);
   //Parameter for Newton loop to find background element
   IntParameter("ITENODEINELE",20,"Number iteration in Newton loop to determine background element",&mlmcp);
   DoubleParameter("CONVTOL",10e-5,"Convergence tolerance for Newton loop",&mlmcp);
   IntParameter("INITRANDOMSEED",1000,"Random seed for first Monte Carlo run",&mlmcp);
   IntParameter("LEVELNUMBER",0,"Level number for Multi Level Monte Carlo", &mlmcp);
   // Parameters to simulate random field
   IntParameter("RANDOM_FIELD_DIMENSION",3,"Dimension of Random Field 2 or 3",&mlmcp);
   DoubleParameter("PERIODICITY",3000,"Period length of Random Field",&mlmcp);
   //DoubleParameter("CorrLength",3000,"Correlation length of Random Field",&mlmcp);
   IntParameter("NUM_COS_TERMS",200,"Number of terms in geometric row ",&mlmcp);
   IntParameter("WRITESTATS",1000,"Write statistics to file every WRITESTATS ",&mlmcp);

  Teuchos::ParameterList& scontact = list->sublist("MESHTYING AND CONTACT",false,"");

  setStringToIntegralParameter<int>("APPLICATION","None","Type of contact or meshtying app",
       tuple<std::string>("None","none",
                          "MortarContact","mortarcontact",
                          "MortarMeshtying","mortarmeshtying",
                          "BeamContact","beamcontact"),
       tuple<int>(
                  INPAR::CONTACT::app_none,INPAR::CONTACT::app_none,
                  INPAR::CONTACT::app_mortarcontact,INPAR::CONTACT::app_mortarcontact,
                  INPAR::CONTACT::app_mortarmeshtying,INPAR::CONTACT::app_mortarmeshtying,
                  INPAR::CONTACT::app_beamcontact, INPAR::CONTACT::app_beamcontact),
       &scontact);

  setStringToIntegralParameter<int>("FRICTION","None","Type of friction law",
      tuple<std::string>("None","none",
                         "Stick","stick",
                         "Tresca","tresca",
                         "Coulomb","coulomb"),
      tuple<int>(
                 INPAR::CONTACT::friction_none,INPAR::CONTACT::friction_none,
                 INPAR::CONTACT::friction_stick,INPAR::CONTACT::friction_stick,
                 INPAR::CONTACT::friction_tresca,INPAR::CONTACT::friction_tresca,
                 INPAR::CONTACT::friction_coulomb,INPAR::CONTACT::friction_coulomb),
      &scontact);

  DoubleParameter("FRBOUND",0.0,"Friction bound for Tresca friction",&scontact);
  DoubleParameter("FRCOEFF",0.0,"Friction coefficient for Coulomb friction",&scontact);

  setStringToIntegralParameter<int>("WEAR","None","Type of wear law",
      tuple<std::string>("None","none",
                         "Archard","archard"),
      tuple<int>(
                 INPAR::CONTACT::wear_none,INPAR::CONTACT::wear_none,
                 INPAR::CONTACT::wear_archard,INPAR::CONTACT::wear_archard),
      &scontact);

  DoubleParameter("WEARCOEFF",0.0,"Wear coefficient",&scontact);

  setStringToIntegralParameter<int>("STRATEGY","LagrangianMultipliers","Type of employed solving strategy",
        tuple<std::string>("LagrangianMultipliers","lagrange", "Lagrange",
                           "PenaltyMethod","penalty", "Penalty",
                           "AugmentedLagrange","augmented", "Augmented"),
        tuple<int>(
                INPAR::CONTACT::solution_lagmult, INPAR::CONTACT::solution_lagmult, INPAR::CONTACT::solution_lagmult,
                INPAR::CONTACT::solution_penalty, INPAR::CONTACT::solution_penalty, INPAR::CONTACT::solution_penalty,
                INPAR::CONTACT::solution_auglag, INPAR::CONTACT::solution_auglag, INPAR::CONTACT::solution_auglag),
        &scontact);

  setStringToIntegralParameter<int>("SHAPEFCN","Dual","Type of employed set of shape functions",
        tuple<std::string>("Dual", "dual",
                           "Standard", "standard", "std"),
        tuple<int>(
                INPAR::MORTAR::shape_dual, INPAR::MORTAR::shape_dual,
                INPAR::MORTAR::shape_standard, INPAR::MORTAR::shape_standard, INPAR::MORTAR::shape_standard),
        &scontact);

  setStringToIntegralParameter<int>("SYSTEM","Condensed","Type of linear system setup / solution",
        tuple<std::string>("Condensed","condensed", "cond",
                           "SaddlePointCoupled","saddlepointcoupled", "spcoupled",
                           "SaddlePointSimpler","saddlepointsimpler", "spsimpler"),
        tuple<int>(
                INPAR::CONTACT::system_condensed, INPAR::CONTACT::system_condensed, INPAR::CONTACT::system_condensed,
                INPAR::CONTACT::system_spcoupled, INPAR::CONTACT::system_spcoupled, INPAR::CONTACT::system_spcoupled,
                INPAR::CONTACT::system_spsimpler, INPAR::CONTACT::system_spsimpler, INPAR::CONTACT::system_spsimpler),
        &scontact);

  DoubleParameter("PENALTYPARAM",0.0,"Penalty parameter for penalty / augmented solution strategy",&scontact);
  DoubleParameter("PENALTYPARAMTAN",0.0,"Tangential penalty parameter for penalty / augmented solution strategy",&scontact);
  IntParameter("UZAWAMAXSTEPS",10,"Maximum no. of Uzawa steps for augmented / Uzawa solution strategy",&scontact);
  DoubleParameter("UZAWACONSTRTOL",1.0e-8,"Tolerance of constraint norm for augmented / Uzawa solution strategy",&scontact);

  setStringToIntegralParameter<int>("FULL_LINEARIZATION","Yes","If chosen full linearization of contact is applied",
                               yesnotuple,yesnovalue,&scontact);

  setStringToIntegralParameter<int>("SEMI_SMOOTH_NEWTON","Yes","If chosen semi-smooth Newton concept is applied",
                               yesnotuple,yesnovalue,&scontact);

  DoubleParameter("SEMI_SMOOTH_CN",1.0,"Weighting factor cn for semi-smooth PDASS",&scontact);
  DoubleParameter("SEMI_SMOOTH_CT",1.0,"Weighting factor ct for semi-smooth PDASS",&scontact);

  setStringToIntegralParameter<int>("SEARCH_ALGORITHM","Binarytree","Type of contact search",
       tuple<std::string>("BruteForce","bruteforce",
      		                "BruteForceEleBased","bruteforceelebased",
                          "BinaryTree","Binarytree","binarytree"),
       tuple<int>(INPAR::MORTAR::search_bfele,INPAR::MORTAR::search_bfele,
                  INPAR::MORTAR::search_bfele,INPAR::MORTAR::search_bfele,
                  INPAR::MORTAR::search_binarytree,INPAR::MORTAR::search_binarytree,
                  INPAR::MORTAR::search_binarytree),
       &scontact);

  DoubleParameter("SEARCH_PARAM",0.3,"Radius / Bounding volume inflation for contact search",&scontact);

  setStringToIntegralParameter<int>("COUPLING_AUXPLANE","Yes","If chosen auxiliary planes are used for 3D coupling",
                               yesnotuple,yesnovalue,&scontact);

  setStringToIntegralParameter<int>("LAGMULT_QUAD3D","undefined","Type of LM ansatz/testing fct.",
       tuple<std::string>("undefined",
                          "quad_quad", "quadratic_quadratic",
                          "quad_pwlin", "quadratic_piecewiselinear",
                          "quad_lin", "quadratic_linear",
                          "pwlin_pwlin", "piecewiselinear_piecewiselinear",
                          "lin_lin","linear_linear"),
       tuple<int>(
                  INPAR::MORTAR::lagmult_undefined,
                  INPAR::MORTAR::lagmult_quad_quad, INPAR::MORTAR::lagmult_quad_quad,
                  INPAR::MORTAR::lagmult_quad_pwlin, INPAR::MORTAR::lagmult_quad_pwlin,
                  INPAR::MORTAR::lagmult_quad_lin, INPAR::MORTAR::lagmult_quad_lin,
                  INPAR::MORTAR::lagmult_pwlin_pwlin, INPAR::MORTAR::lagmult_pwlin_pwlin,
                  INPAR::MORTAR::lagmult_lin_lin, INPAR::MORTAR::lagmult_lin_lin),
       &scontact);

  setStringToIntegralParameter<int>("CROSSPOINTS","No","If chosen, multipliers are removed from crosspoints / edge nodes",
                               yesnotuple,yesnovalue,&scontact);

  setStringToIntegralParameter<int>("EMOUTPUT","None","Type of energy and momentum output",
      tuple<std::string>("None","none", "No", "no",
                         "Screen", "screen",
                         "File", "file",
                         "Both", "both"),
      tuple<int>(
              INPAR::CONTACT::output_none, INPAR::CONTACT::output_none,
              INPAR::CONTACT::output_none, INPAR::CONTACT::output_none,
              INPAR::CONTACT::output_screen, INPAR::CONTACT::output_screen,
              INPAR::CONTACT::output_file, INPAR::CONTACT::output_file,
              INPAR::CONTACT::output_both, INPAR::CONTACT::output_both),
      &scontact);

  setStringToIntegralParameter<int>("PARALLEL_REDIST","Static","Type of redistribution algorithm",
      tuple<std::string>("None","none", "No", "no",
                         "Static", "static",
                         "Dynamic", "dynamic"),
      tuple<int>(
              INPAR::MORTAR::parredist_none, INPAR::MORTAR::parredist_none,
              INPAR::MORTAR::parredist_none, INPAR::MORTAR::parredist_none,
              INPAR::MORTAR::parredist_static, INPAR::MORTAR::parredist_static,
              INPAR::MORTAR::parredist_dynamic, INPAR::MORTAR::parredist_dynamic),
      &scontact);

  DoubleParameter("MAX_BALANCE",2.0,"Maximum value of load balance measure before parallel redistribution",&scontact);
  IntParameter("MIN_ELEPROC",0,"Minimum no. of elements per processor for parallel redistribution",&scontact);

  DoubleParameter("HEATTRANSSLAVE",0.0,"Heat transfer parameter for slave side in thermal contact",&scontact);
  DoubleParameter("HEATTRANSMASTER",0.0,"Heat transfer parameter for master side in thermal contact",&scontact);

  setStringToIntegralParameter<int>("THERMOLAGMULT","Yes","Lagrange Multipliers are applied for thermo-contact",
                               yesnotuple,yesnovalue,&scontact);

  setStringToIntegralParameter<int>("BEAMS_NEWGAP","No","choose between original or enhanced gapfunction",
                                yesnotuple,yesnovalue,&scontact);

  setStringToIntegralParameter<int>("BEAMS_SMOOTHING","None","Application of smoothed tangent field",
       tuple<std::string>("None","none",
                          "Smoothed","smoothed",
                          "Partially","partially",
                          "Cpp", "cpp"),
       tuple<int>(
                  INPAR::CONTACT::bsm_none,INPAR::CONTACT::bsm_none,
                  INPAR::CONTACT::bsm_smoothed,INPAR::CONTACT::bsm_smoothed,
                  INPAR::CONTACT::bsm_partially,INPAR::CONTACT::bsm_partially,
                  INPAR::CONTACT::bsm_cpp,INPAR::CONTACT::bsm_cpp),
       &scontact);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& interaction_potential = list->sublist("INTERACTION POTENTIAL",false,"");

  // read if surfaces , volumes or both including fluid should be considered
  setStringToIntegralParameter<int>("POTENTIAL_TYPE","Surface","Type of interaction potential",
                                tuple<std::string>("Surface",
                                                   "Volume",
                                                   "Surfacevolume",
                                                   "Surface_fsi",
                                                   "Volume_fsi",
                                                   "Surfacevolume_fsi"),
                                tuple<int>(
                                   INPAR::POTENTIAL::potential_surface,
                                   INPAR::POTENTIAL::potential_volume,
                                   INPAR::POTENTIAL::potential_surfacevolume,
                                   INPAR::POTENTIAL::potential_surface_fsi,
                                   INPAR::POTENTIAL::potential_volume_fsi,
                                   INPAR::POTENTIAL::potential_surfacevolume_fsi),
                                &interaction_potential);

  // approximation method
  setStringToIntegralParameter<int>("APPROXIMATION_TYPE","None","Type of approximation",
                                tuple<std::string>("None",
                                                   "Surface_approx",
                                                   "Point_approx"),
                                tuple<int>(
                                           INPAR::POTENTIAL::approximation_none,
                                           INPAR::POTENTIAL::approximation_surface,
                                           INPAR::POTENTIAL::approximation_point),
                                &interaction_potential);

  // switches on the analytical solution computation for two van der waals spheres or membranes
  setStringToIntegralParameter<int>("ANALYTICALSOLUTION","None", "Type of analytical solution"
                                 "computes analytical solutions for two Van der Waals spheres or membranes",
                                 tuple<std::string>("None",
                                                    "Sphere",
                                                    "Membrane"),
                                 tuple<int>(
                                            INPAR::POTENTIAL::solution_none,
                                            INPAR::POTENTIAL::solution_sphere,
                                            INPAR::POTENTIAL::solution_membrane),
                                            &interaction_potential);
  // use 2D integration for pseudo 3D
  setStringToIntegralParameter<int>("PSEUDO3D","no",
                                     "use 2D integration for pseudo 3D",
                                     yesnotuple,yesnovalue,&interaction_potential);

  // radius of can der Waals spheres for analytical testing
  DoubleParameter(  "VDW_RADIUS",0.0,
                    "radius of van der Waals spheres",
                    &interaction_potential);

  // thickness of sphericael Waals membranes for analytical testing
  DoubleParameter(  "THICKNESS",0.0,
                    "membrane thickness",
                    &interaction_potential);

  // number of atoms or molecules offset
  DoubleParameter(  "N_OFFSET",0.0,
                    "number of atoms or molecules offset",
                    &interaction_potential);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& statmech = list->sublist("STATISTICAL MECHANICS",false,"");

  //Reading kind of background fluid stream in the thermal bath
  setStringToIntegralParameter<int>("THERMALBATH","None","Type of thermal bath applied to elements",
                               //listing possible strings in input file in category THERMALBATH
                               tuple<std::string>("None","none",
                                                  "Uniform","uniform",
                                                  "ShearFlow","shearflow","Shearflow"),
                               //translating input strings into BACI input parameters
                               tuple<int>(INPAR::STATMECH::thermalbath_none,INPAR::STATMECH::thermalbath_none,
                                          INPAR::STATMECH::thermalbath_uniform,INPAR::STATMECH::thermalbath_uniform,
                                          INPAR::STATMECH::thermalbath_shearflow,INPAR::STATMECH::thermalbath_shearflow,INPAR::STATMECH::thermalbath_shearflow),
                               &statmech);
  //Reading which kind of special output should be written to files
  setStringToIntegralParameter<int>("SPECIAL_OUTPUT","None","kind of special statistical output data written into files",
                                 //listing possible strings in input file in category SPECIAL_OUTPUT
                                 tuple<std::string>("None","none",
                                                    "endtoend_log",
                                                    "anisotropic",
                                                    "orientationcorrelation",
                                                    "endtoend_const",
                                                    "viscoelasticity",
                                                    "densitydensitycorr"),
                                 //translating input strings into BACI input parameters
                                 tuple<int>(INPAR::STATMECH::statout_none,INPAR::STATMECH::statout_none,
                                            INPAR::STATMECH::statout_endtoendlog,
                                            INPAR::STATMECH::statout_anisotropic,
                                            INPAR::STATMECH::statout_orientationcorrelation,
                                            INPAR::STATMECH::statout_endtoendconst,
                                            INPAR::STATMECH::statout_viscoelasticity,
                                            INPAR::STATMECH::statout_densitydensitycorr),
                                 &statmech);
  //Reading which kind of friction model should be applied
  setStringToIntegralParameter<int>("FRICTION_MODEL","none","friction model for polymer dynamics",
                                 //listing possible strings in input file in category FRICTION_MODEL
                                 tuple<std::string>("none",
                                                    "isotropiclumped",
                                                    "isotropicconsistent",
                                                    "anisotropicconsistent"),
                                 //translating input strings into BACI input parameters
                                 tuple<int>(INPAR::STATMECH::frictionmodel_none,
                                                                    INPAR::STATMECH::frictionmodel_isotropiclumped,
                                                                    INPAR::STATMECH::frictionmodel_isotropicconsistent,
                                                                    INPAR::STATMECH::frictionmodel_anisotropicconsistent),
                                                                    &statmech);
  //time after which writing of statistical output is started
  DoubleParameter("STARTTIMEOUT",0.0,"Time after which writing of statistical output is started",&statmech);
  //time after which certain action in simulation (e.g. DBCs in viscoelastic simulations) are started
  DoubleParameter("STARTTIMEACT",0.0,"Time after which certain action in simulation is started",&statmech);
  //time after which certain action in simulation (e.g. DBCs in viscoelastic simulations) are started
  DoubleParameter("EQUILIBTIME",0.0,"Time until which no crosslinkers are set",&statmech);
  //time after which certain action in simulation (e.g. DBCs in viscoelastic simulations) are started
  DoubleParameter("KTSWITCHTIME",0.0,"Time when KT value is changed to KTACT",&statmech);
  //alternative post-STARTTIME time step size
  DoubleParameter("DELTA_T_NEW",0.0,"A new time step size that comes into play once DBCs are have been activated",&statmech);
  //Reading whether dynamics remodelling of cross linker distribution takes place
  setStringToIntegralParameter<int>("DYN_CROSSLINKERS","No","If chosen cross linker proteins are added and removed in each time step",
                               yesnotuple,yesnovalue,&statmech);
  //Reading double parameter for shear flow field
  DoubleParameter("SHEARAMPLITUDE",0.0,"Shear amplitude of flow in z-direction; note: not amplitude of displacement, but of shear strain!",&statmech);
  //Reading double parameter for viscosity of background fluid
  DoubleParameter("ETA",0.0,"viscosity",&statmech);
  //Reading double parameter for thermal energy in background fluid (temperature * Boltzmann constant)
  DoubleParameter("KT",0.0,"thermal energy",&statmech);
  //Reading double parameter for thermal energy in background fluid (temperature * Boltzmann constant)
  DoubleParameter("KTACT",0.0,"thermal energy for t>=STARTTIMEACT",&statmech);
  //Reading double parameter for crosslinker on-rate at the beginning
  DoubleParameter("K_ON_start",0.0,"crosslinker on-rate at the end",&statmech);
  //Reading double parameter for crosslinker on-rate at the end
  DoubleParameter("K_ON_end",0.0,"crosslinker on-rate at the end",&statmech);
  //Reading double parameter for crosslinker off-rate at the beginning
  DoubleParameter("K_OFF_start",0.0,"crosslinker off-rate at the beginning",&statmech);
  //Reading double parameter for crosslinker off-rate at the end
  DoubleParameter("K_OFF_end",0.0,"crosslinker off-rate at the end",&statmech);
  //Reading double parameter for crosslinker off-rate at the end
  DoubleParameter("K_ON_SELF",0.0,"crosslinker on-rate for crosslinkers with both bonds on same filament",&statmech);
  //number of overall crosslink molecules in the boundary volume
  IntParameter("N_crosslink",0,"number of crosslinkers for switching on- and off-rates; if molecule diffusion model is used: number of crosslink molecules",&statmech);
  //number by which the number of crosslinkers is reduced.
  IntParameter("REDUCECROSSLINKSBY",0,"number of crosslinker elements by which the overall number of crosslinker is reduced.",&statmech);
  //Maximal number of crosslinkers a node can establish to other nodes
  IntParameter("N_CROSSMAX",1,"Maximal number of crosslinkers a node can establish to other nodes",&statmech);
  //Reading double parameter for crosslinker protein mean length
  DoubleParameter("R_LINK",0.0,"Mean distance between two nodes connected by a crosslinker",&statmech);
  //Absolute value of difference between maximal/minimal and mean cross linker length
  DoubleParameter("DeltaR_LINK",0.0,"Absolute value of difference between maximal/minimal and mean cross linker length",&statmech);
  //Edge length of cube for periodic boundary conditions problem
  DoubleParameter("PeriodLength",0.0,"Edge length of cube for periodic boundary conditions problem",&statmech);
  // upper bound of the interval within which uniformly distributed random numbers are generated
  DoubleParameter("MaxRandValue",0.0,"Upper bound of the interval within which uniformly distributed random numbers are generated (usually equal to PeriodLength)",&statmech);
  //angle between filament axes at crosslinked points with zero potential energy
  DoubleParameter("PHI0",0.0,"equilibrium angle between crosslinker axis and filament at each binding site",&statmech);
  //only angles in the range PHI0 +/- PHIODEV are admitted at all for the angle PHI between filament axes at crosslinked points; the default value for this parameter is 2*pi so that by default any value is admitted
  DoubleParameter("PHI0DEV",6.28,"only angles in the range PHI0 +/- PHIODEV",&statmech);
  //stiffness of orientation potential of crosslinkers
  DoubleParameter("CORIENT",0.0,"stiffness of orientation potential of crosslinkers",&statmech);
 //Young's modulus of crosslinkers
  DoubleParameter("ELINK",0.0,"Moment of inertia of area of crosslinkers",&statmech);
  //Moment of inertia of area of crosslinkers
  DoubleParameter("ILINK",0.0,"Moment of inertia of area of crosslinkers",&statmech);
  //Polar moment of inertia of area of crosslinkers
  DoubleParameter("IPLINK",0.0,"Polar moment of inertia of area of crosslinkers",&statmech);
  //Cross section of crosslinkers
  DoubleParameter("ALINK",0.0,"Cross section of crosslinkers",&statmech);
  //Parameter for PTC according to Cyron,Wall (2011):Numerical method for the simulation of the Brownian dynamics of rod-like microstructures with three dimensional nonlinear beam elements
  DoubleParameter("CTRANSPTC0",0.0,"PTC factor for translational DOF in first iteration step",&statmech);
  //Parameter for PTC according to Cyron,Wall (2011):Numerical method for the simulation of the Brownian dynamics of rod-like microstructures with three dimensional nonlinear beam elements
  DoubleParameter("CROTPTC0",0.145,"PTC factor for rotational DOF in first iteration step",&statmech);
  //Parameter for PTC according to Cyron,Wall (2011):Numerical method for the simulation of the Brownian dynamics of rod-like microstructures with three dimensional nonlinear beam elements
  DoubleParameter("ALPHAPTC",6.0,"exponent of power law for reduction of PTC factor",&statmech);
  //Makes filaments and crosslinkers be plotted by that factor thicker than they are acutally
  DoubleParameter("PlotFactorThick",1.0,"Makes filaments and crosslinkers be plotted by that factor thicker than they are acutally",&statmech);
  //Reading whether fixed seed for random numbers should be applied
  setStringToIntegralParameter<int>("CHECKORIENT","No","If chosen crosslinkers are set only after check of orientation of linked filaments",
                               yesnotuple,yesnovalue,&statmech);
  //Reading whether fixed seed for random numbers should be applied
  setStringToIntegralParameter<int>("GMSHOUTPUT","No","If chosen gmsh output is generated.",
                               yesnotuple,yesnovalue,&statmech);
  //Number of time steps between two special outputs written
  IntParameter("OUTPUTINTERVALS",1,"Number of time steps between two special outputs written",&statmech);
  //Number of time steps between two gmsh outputs written
	IntParameter("GMSHOUTINTERVALS",100,"Number of time steps between two gmsh outputs written",&statmech);
  //Reading direction of oscillatory motion that DBC nodes are subjected to (we need this when using periodic BCs)
  IntParameter("OSCILLDIR",-1,"Global spatial direction of oscillatory motion by Dirichlet BCs",&statmech);
  //Reading time curve number for oscillatory motion
  IntParameter("CURVENUMBER",0,"Specifies Time Curve number of oscillatory motion",&statmech);
  //Reading number of elements that are taken into account when applying Dirichlet Conditions (useful to avoid redundant evaluation)
  // when Crosslink elements are added or the bead-spring-model is used
  IntParameter("NUM_EVAL_ELEMENTS",-1,"number of elements that are taken into account when applying Dirichlet Conditions",&statmech);
  // number of partitions along the edge length of the volume determining the resolution of the search grid
  IntParameter("SEARCHRES",1,"leads to the indexing of SEARCHRES^3 cubic volume partitions",&statmech);
  //Reading direction of oscillatory motion that DBC nodes are subjected to (we need this when using periodic BCs)
  IntParameter("INITIALSEED",0,"Integer value which guarantuees reproducable random number, default 0",&statmech);
  // number of histogram bins for post-analysis
  IntParameter("HISTOGRAMBINS",1,"number of bins for histograms showing the density-density-correlation-function",&statmech);
  // number of raster point along for ddcorr output boundary box shift -> n³ points in volume
  IntParameter("NUMRASTERPOINTS",3,"number of bins for histograms showing the density-density-correlation-function",&statmech);
  //Reading whether DBCs shall be applied to broken elements
  setStringToIntegralParameter<int>("PERIODICDBC","No","If chosen, Point DBCs are applied to the nodes of discontinuous elements",
                               yesnotuple,yesnovalue,&statmech);
  //Reading whether initial DBC declarations from the input file are kept valid during simulation
  setStringToIntegralParameter<int>("CONVENTIONALDBC","Yes","If chosen, Point DBCs conventionally defined by input file are taken into account",
                               yesnotuple,yesnovalue,&statmech);
  //Reading whether fixed seed for random numbers should be applied
  setStringToIntegralParameter<int>("FIXEDSEED","No","If chosen fixed seed for random numbers in each time step is applied",
                               yesnotuple,yesnovalue,&statmech);
  //Reading whether beam contact is switched on or not
  setStringToIntegralParameter<int>("BEAMCONTACT","No","If chosen beam contact is calculated",yesnotuple,yesnovalue,&statmech);
  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& tdyn = list->sublist("THERMAL DYNAMIC",false,"");

  setStringToIntegralParameter<int>("DYNAMICTYP","OneStepTheta",
                               "type of time integration control",
                               tuple<std::string>(
                                 "Statics",
                                 "OneStepTheta",
                                 "GEMM",
                                 "GenAlpha"
                                 ),
                               tuple<int>(
                                  INPAR::THR::dyna_statics,
                                  INPAR::THR::dyna_onesteptheta,
                                  INPAR::THR::dyna_gemm,
                                  INPAR::THR::dyna_genalpha),
                               &tdyn);

  // Output type
  IntParameter("RESEVRYGLOB",1,"save temperature and other global quantities every RESEVRYGLOB steps",&tdyn);
  IntParameter("RESEVRYELEM",1,"save heat fluxes and other element quantities every RESEVRYELEM steps",&tdyn);
  IntParameter("RESEVRYERGY",0,"write system energies every requested step",&tdyn);
  IntParameter("RESTARTEVRY",1,"write restart possibility every RESTARTEVRY steps",&tdyn);

  setStringToIntegralParameter<int>("INITIALFIELD","zero_field",
                               "Initial Field for thermal problem",
                               tuple<std::string>(
                                "zero_field",
                                 "field_by_function",
                                 "field_by_condition"
                                 ),
                               tuple<int>(
                                   INPAR::THR::initfield_zero_field,
                                   INPAR::THR::initfield_field_by_function,
                                   INPAR::THR::initfield_field_by_condition
                                   ),
                               &tdyn);
  IntParameter("INITFUNCNO",-1,"function number for thermal initial field",&tdyn);

  // Time loop control
  DoubleParameter("TIMESTEP",0.05,"time step size",&tdyn);
  IntParameter("NUMSTEP",200,"maximum number of steps",&tdyn);
  DoubleParameter("MAXTIME",5.0,"maximum time",&tdyn);

  // Iterationparameters
  DoubleParameter("TOLTEMP",1.0E-10,
                  "tolerance in the temperature norm of the Newton iteration",
                  &tdyn);
  setStringToIntegralParameter<int>("NORM_TEMP","Abs","type of norm for temperature convergence check",
                               tuple<std::string>(
                                 "Abs",
                                 "Rel",
                                 "Mix"),
                               tuple<int>(
                                 INPAR::THR::convnorm_abs,
                                 INPAR::THR::convnorm_rel,
                                 INPAR::THR::convnorm_mix),
                               &tdyn);

  DoubleParameter("TOLRES",1.0E-08,
                  "tolerance in the residual norm for the Newton iteration",
                  &tdyn);
  setStringToIntegralParameter<int>("NORM_RESF","Abs","type of norm for residual convergence check",
                               tuple<std::string>(
                                 "Abs",
                                 "Rel",
                                 "Mix"),
                               tuple<int>(
                                 INPAR::THR::convnorm_abs,
                                 INPAR::THR::convnorm_rel,
                                 INPAR::THR::convnorm_mix),
                               &tdyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFTEMP","And","binary operator to combine temperature and residual force values",
                               tuple<std::string>(
                                 "And",
                                 "Or"),
                               tuple<int>(
                                 INPAR::THR::bop_and,
                                 INPAR::THR::bop_or),
                               &tdyn);

  IntParameter("MAXITER",50,
               "maximum number of iterations allowed for Newton-Raphson iteration before failure",
               &tdyn);
  IntParameter("MINITER",0,
               "minimum number of iterations to be done within Newton-Raphson loop",
               &tdyn);
  setStringToIntegralParameter<int>("ITERNORM","L2","type of norm to be applied to residuals",
                               tuple<std::string>(
                                 "L1",
                                 "L2",
                                 "Rms",
                                 "Inf"),
                               tuple<int>(
                                 INPAR::THR::norm_l1,
                                 INPAR::THR::norm_l2,
                                 INPAR::THR::norm_rms,
                                 INPAR::THR::norm_inf),
                               &tdyn);
  setStringToIntegralParameter<int>("DIVERCONT","No",
                                    "Go on with time integration even if Newton-Raphson iteration failed",
                                     yesnotuple,yesnovalue,&tdyn);

  setStringToIntegralParameter<int>("NLNSOL","fullnewton","Nonlinear solution technique",
                               tuple<std::string>(
                                 "vague",
                                 "fullnewton"),
                                 tuple<int>(
                                     INPAR::THR::soltech_vague,
                                     INPAR::THR::soltech_newtonfull),
                                     &tdyn);

  setStringToIntegralParameter<int>("PREDICT","ConstTemp","Predictor of iterative solution techniques",
                               tuple<std::string>(
                                 "Vague",
                                 "ConstTemp",
                                 "ConstTempRate",
                                 "TangTemp"),
                               tuple<int>(
                                 INPAR::THR::pred_vague,
                                 INPAR::THR::pred_consttemp,
                                 INPAR::THR::pred_consttemprate,
                                 INPAR::THR::pred_tangtemp),
                               &tdyn);

  // convergence criteria solver adaptivity
  setStringToIntegralParameter<int>("ADAPTCONV","No",
                              "Switch on adaptive control of linear solver tolerance for nonlinear solution",
                              yesnotuple,yesnovalue,&tdyn);
  DoubleParameter("ADAPTCONV_BETTER",0.1,"The linear solver shall be this much better than the current nonlinear residual in the nonlinear convergence limit",&tdyn);

  /*----------------------------------------------------------------------*/
  /* parameters for generalised-alpha thermal integrator */
  Teuchos::ParameterList& tgenalpha = tdyn.sublist("GENALPHA",false,"");

  setStringToIntegralParameter<int>("GENAVG","ImrLike",
                              "mid-average type of internal forces",
                              tuple<std::string>(
                                "Vague",
                                "ImrLike",
                                "TrLike"),
                              tuple<int>(
                                INPAR::THR::midavg_vague,
                                INPAR::THR::midavg_imrlike,
                                INPAR::THR::midavg_trlike),
                              &tgenalpha);
  DoubleParameter("BETA",0.25,"Generalised-alpha factor in (0,1/2]",&tgenalpha);
  DoubleParameter("GAMMA",0.5,"Generalised-alpha factor in (0,1]",&tgenalpha);
  DoubleParameter("ALPHA_M",0.5,"Generalised-alpha factor in [0,1)",&tgenalpha);
  DoubleParameter("ALPHA_F",0.5,"Generalised-alpha factor in [0,1)",&tgenalpha);

  /*----------------------------------------------------------------------*/
  /* parameters for one-step-theta thermal integrator */
  Teuchos::ParameterList& tonesteptheta = tdyn.sublist("ONESTEPTHETA",false,"");

  DoubleParameter("THETA",0.5,"One-step-theta factor in (0,1]",&tonesteptheta);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& tsidyn = list->sublist(
   "TSI DYNAMIC",false,
   "Thermo Structure Interaction\n"
   "Partitioned TSI solver with various coupling methods"
   );

  // Coupling strategy for (partitioned and monolithic) TSI solvers
  setStringToIntegralParameter<int>(
                              "COUPALGO","tsi_iterstagg",
                              "Coupling strategies for TSI solvers",
                              tuple<std::string>(
                                "tsi_oneway",
                                "tsi_sequstagg",
                                "tsi_iterstagg",
                                "tsi_monolithic"
                                ),
                              tuple<int>(
                                INPAR::TSI::OneWay,
                                INPAR::TSI::SequStagg,
                                INPAR::TSI::IterStagg,
                                INPAR::TSI::Monolithic
                                ),
                              &tsidyn);

  // decide in partitioned TSI which one-way coupling should be used
  setStringToIntegralParameter<int>("COUPVARIABLE","Displacement",
                              "Coupling variable",
                              tuple<std::string>(
                                "Displacement",
                                "Temperature"
                                ),
                              tuple<int>(0,1),
                              &tsidyn);

  // Output type
  IntParameter("RESTARTEVRY",1,"write restart possibility every RESTARTEVRY steps",&tsidyn);
  // Time loop control
  IntParameter("NUMSTEP",200,"maximum number of Timesteps",&tsidyn);
  DoubleParameter("MAXTIME",1000.0,"total simulation time",&tsidyn);
  DoubleParameter("TIMESTEP",0.05,"time step size dt",&tsidyn);
  DoubleParameter("CONVTOL",1e-6,"tolerance for convergence check",&tsidyn);
  IntParameter("ITEMAX",10,"maximum number of iterations over fields",&tsidyn);
  IntParameter("ITEMIN",1,"minimal number of iterations over fields",&tsidyn);
  IntParameter("UPRES",1,"increment for writing solution",&tsidyn);

  // Iterationparameters
  setStringToIntegralParameter<int>("NORM_INC","Abs","type of norm for primary variables convergence check",
                               tuple<std::string>(
                                 "Abs"
                                 ),
                               tuple<int>(
                                 INPAR::TSI::convnorm_abs
                                 ),
                               &tsidyn);

  setStringToIntegralParameter<int>("NORM_RESF","Abs","type of norm for residual convergence check",
                                 tuple<std::string>(
                                   "Abs"
                                   ),
                                 tuple<int>(
                                   INPAR::TSI::convnorm_abs
                                   ),
                                 &tsidyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFINC","And","binary operator to combine primary variables and residual force values",
                               tuple<std::string>("And"),
                               tuple<int>(INPAR::TSI::bop_and),
                               &tsidyn);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& flucthydro = list->sublist("FLUCTUATING HYDRODYNAMICS",false,"");
  DoubleParameter("TEMPERATURE",300,"Temperature in K",&flucthydro);
  DoubleParameter("BOLTZMANNCONST",1.380650424e-23,"Boltzmann constant",&flucthydro);
  setStringToIntegralParameter<int>("SEEDCONTROL","No",
                                      "control seeding with given unsigned integer",
                                      yesnotuple,yesnovalue,&flucthydro);
  IntParameter("SEEDVARIABLE",0,"seed variable",&flucthydro);
  IntParameter("SAMPLEPERIOD",1,"sample period",&flucthydro);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fdyn = list->sublist("FLUID DYNAMIC",false,"");

  // physical type of fluid flow (incompressible, varying density, loma, Boussinesq approximation)
  setStringToIntegralParameter<int>("PHYSICAL_TYPE","Incompressible",
                               "Physical Type",
                               tuple<string>(
                                 "Incompressible",
                                 "Varying_density",
                                 "Loma",
                                 "Boussinesq"
                                 ),
                               tuple<int>(
                                     INPAR::FLUID::incompressible,
                                     INPAR::FLUID::varying_density,
                                     INPAR::FLUID::loma,
                                     INPAR::FLUID::boussinesq),
                               &fdyn);

  setStringToIntegralParameter<int>("FLUID_SOLVER", "Implicit",
                                    "Solving strategy for fluid",
                                    tuple<std::string>("Implicit",
                                                       "Pressure Correction",
                                                       "Pressure Correction SemiImplicit",
                                                       "FluidXFluid"),
                                    tuple<int>(fluid_solver_implicit,
                                               fluid_solver_pressurecorrection,
                                               fluid_solver_pressurecorrection_semiimplicit,
                                               fluid_solver_fluid_xfluid),
                                    &fdyn);

  setStringToIntegralParameter<int>("TIMEINTEGR","One_Step_Theta",
      "Time Integration Scheme",
      tuple<std::string>(
          "Stationary",
          "Np_Gen_Alpha",
          "Gen_Alpha",
          "Af_Gen_Alpha",
          "One_Step_Theta",
          "BDF2"),
      tuple<int>(
          INPAR::FLUID::timeint_stationary,
          INPAR::FLUID::timeint_npgenalpha,        // fluid3's implementation (solving for velocity increment)
          INPAR::FLUID::timeint_gen_alpha,         // Peter's implementation (solving for acceleration increment)
          INPAR::FLUID::timeint_afgenalpha,
          INPAR::FLUID::timeint_one_step_theta,
          INPAR::FLUID::timeint_bdf2),
          &fdyn);

  setStringToIntegralParameter<int>("STARTINGALGO","One_Step_Theta","",
                               tuple<std::string>(
                                 "One_Step_Theta"
                                 ),
                               tuple<int>(
                                 INPAR::FLUID::timeint_one_step_theta
                                 ),
                               &fdyn);
  setStringToIntegralParameter<int>("NONLINITER","fixed_point_like",
                               "Nonlinear iteration scheme",
                               tuple<std::string>(
                                 "fixed_point_like",
                                 "Newton",
                                 "minimal"
                                 ),
                               tuple<int>(
                                     INPAR::FLUID::fixed_point_like,
                                     INPAR::FLUID::Newton,
                                     INPAR::FLUID::minimal),
                               &fdyn);

  setStringToIntegralParameter<int>("PREDICTOR","default",
                                    "Predictor for first guess in nonlinear iteration",
                                    tuple<std::string>(
                                      "disabled",
                                      "default",
                                      "steady_state_predictor",
                                      "zero_acceleration_predictor",
                                      "constant_acceleration_predictor",
                                      "constant_increment_predictor",
                                      "explicit_second_order_midpoint"
                                      ),
                                    tuple<int>(1,2,3,4,5,6,7),
                                    &fdyn);

  setStringToIntegralParameter<int>("CONVCHECK","L_2_norm",
                               "norm for convergence check",
                               tuple<std::string>(
                                 "No",
                                 "L_infinity_norm",
                                 "L_1_norm",
                                 "L_2_norm",
                                 "L_2_norm_without_residual_at_itemax"
                                 ),
                               tuple<std::string>(
                                 "do not check for convergence (ccarat)",
                                 "use max norm (ccarat)",
                                 "use abs. norm (ccarat)",
                                 "compute L2 errors of increments (relative) and residuals (absolute)",
                                 "same as L_2_norm, only no residual norm is computed if itemax is reached (speedup for turbulence calculations, startup phase)"
                                 ),
                               tuple<int>(
                                 FLUID_DYNAMIC::fncc_no,
                                 FLUID_DYNAMIC::fncc_Linf,
                                 FLUID_DYNAMIC::fncc_L1,
                                 FLUID_DYNAMIC::fncc_L2,
                                 FLUID_DYNAMIC::fncc_L2_wo_res
                                 ),
                               &fdyn);
  setStringToIntegralParameter<int>("STEADYCHECK","L_2_norm",
                               "Norm of steady state check",
                               tuple<std::string>(
                                 "No",
                                 "L_infinity_norm",
                                 "L_1_norm",
                                 "L_2_norm"
                                 ),
                               tuple<int>(
                                 FLUID_DYNAMIC::fncc_no,
                                 FLUID_DYNAMIC::fncc_Linf,
                                 FLUID_DYNAMIC::fncc_L1,
                                 FLUID_DYNAMIC::fncc_L2
                                 ),
                               &fdyn);

  setStringToIntegralParameter<int>("INITIALFIELD","zero_field",
                               "Initial field for fluid problem",
                               tuple<std::string>(
                                 "zero_field",
                                 "field_by_function",
                                 "disturbed_field_from_function",
                                 "FLAME_VORTEX_INTERACTION",
                                 "BELTRAMI-FLOW",
                                 "KIM-MOIN-FLOW",
                                 "BOCHEV-TEST"),
                               tuple<int>(
                                     INPAR::FLUID::initfield_zero_field,
                                     INPAR::FLUID::initfield_field_by_function,
                                     INPAR::FLUID::initfield_disturbed_field_from_function,
                                     INPAR::FLUID::initfield_flame_vortex_interaction,
                                     INPAR::FLUID::initfield_beltrami_flow,
                                     INPAR::FLUID::initfield_kim_moin_flow,
                                     INPAR::FLUID::initfield_bochev_test),
                               &fdyn);

  setStringToIntegralParameter<int>("LIFTDRAG","No",
                               "Calculate lift and drag forces along specified boundary",
                               tuple<std::string>(
                                 "No",
                                 "no",
                                 "Yes",
                                 "yes",
                                 "Nodeforce",
                                 "NODEFORCE",
                                 "nodeforce"
                                 ),
                               tuple<int>(
                                 FLUID_DYNAMIC::ld_none,
                                 FLUID_DYNAMIC::ld_none,
                                 FLUID_DYNAMIC::ld_nodeforce,
                                 FLUID_DYNAMIC::ld_nodeforce,
                                 FLUID_DYNAMIC::ld_nodeforce,
                                 FLUID_DYNAMIC::ld_nodeforce,
                                 FLUID_DYNAMIC::ld_nodeforce
                                 ),
                               &fdyn);

  setStringToIntegralParameter<int>("CONVFORM","convective","form of convective term",
                               tuple<std::string>(
                                 "convective",
                                 "conservative"
                                 ),
                               tuple<int>(0,1),
                               &fdyn);

  setStringToIntegralParameter<int>("NEUMANNINFLOW",
                               "no",
                               "Flag to (de)activate potential Neumann inflow term(s)",
                               tuple<std::string>(
                                 "no",
                                 "yes"),
                               tuple<std::string>(
                                 "No Neumann inflow term(s)",
                                 "Neumann inflow term(s) might occur"),
                               tuple<int>(0,1),
                               &fdyn);

  setStringToIntegralParameter<int>("2DFLOW",
                               "no",
                               "Flag needed for pseudo 2D-simulations for fluid-fluid-Coupling",
                               tuple<std::string>(
                                 "no",
                                 "yes"),
                               tuple<std::string>(
                                 "No 2D-Simulation",
                                 "2D-Simulation"),
                               tuple<int>(0,1),
                               &fdyn);

  setStringToIntegralParameter<int>("MESHTYING", "no", "Flag to (de)activate mesh tying algorithm",
                                  tuple<std::string>(
                                    "no",
                                    "Condensed_Smat",
                                    "Condensed_Bmat",
                                    "SaddlePointSystem_coupled",
                                    "SaddlePointSystem_pc"),
                                  tuple<int>(
                                      INPAR::FLUID::no_meshtying,
                                      INPAR::FLUID::condensed_smat,
                                      INPAR::FLUID::condensed_bmat,
                                      INPAR::FLUID::sps_coupled,
                                      INPAR::FLUID::sps_pc),
                                  &fdyn);

  setStringToIntegralParameter<int>("CALCERROR",
                               "no",
                               "Flag to (de)activate error calculation",
                               tuple<std::string>(
                                 "no",
                                 "beltrami_flow",
                                 "channel2D",
                                 "gravitation",
                                 "shear_flow"),
                               tuple<int>(
                                   INPAR::FLUID::no_error_calculation,
                                   INPAR::FLUID::beltrami_flow,
                                   INPAR::FLUID::channel2D,
                                   INPAR::FLUID::gravitation,
                                   INPAR::FLUID::shear_flow),
                               &fdyn);

  setStringToIntegralParameter<int>("FSSUGRVISC","No","fine-scale subgrid viscosity",
                               tuple<std::string>(
                                 "No",
                                 "Smagorinsky_all",
                                 "Smagorinsky_small"
                                 ),
                               tuple<int>(
                                   INPAR::FLUID::no_fssgv,
                                   INPAR::FLUID::smagorinsky_all,
                                   INPAR::FLUID::smagorinsky_small
                                   ),
                               &fdyn);

  setStringToIntegralParameter<int>("SIMPLER","no",
                               "Switch on SIMPLE family of solvers, needs additional FLUID PRESSURE SOLVER block!",
                               yesnotuple,yesnovalue,&fdyn);

/*  setStringToIntegralParameter<int>("SPLITFLUID","no",
                               "If yes, the fluid matrix is splitted into a block sparse matrix for velocity and pressure degrees of freedom (similar to SIMPLER flag)",
                               yesnotuple,yesnovalue,&fdyn);*/

  setStringToIntegralParameter<int>("ADAPTCONV","yes",
                               "Switch on adaptive control of linear solver tolerance for nonlinear solution",
                               yesnotuple,yesnovalue,&fdyn);
  DoubleParameter("ADAPTCONV_BETTER",0.1,"The linear solver shall be this much better than the current nonlinear residual in the nonlinear convergence limit",&fdyn);

  IntParameter("UPPSS",1,"Increment for visualisation (unused)",&fdyn);
  IntParameter("UPOUT",1,"Increment for writing solution to output file (unused)",&fdyn);
  IntParameter("UPRES",1,"Increment for writing solution",&fdyn);
  IntParameter("RESSTEP",0,"Restart Step (unused)",&fdyn);
  IntParameter("RESTARTEVRY",20,"Increment for writing restart",&fdyn);
  IntParameter("NUMSTEP",1,"Total number of Timesteps",&fdyn);
  IntParameter("STEADYSTEP",-1,"steady state check every step",&fdyn);
  IntParameter("NUMSTASTEPS",0,"Number of Steps for Starting Scheme",&fdyn);
  IntParameter("STARTFUNCNO",-1,"Function for Initial Starting Field",&fdyn);
  IntParameter("ITEMAX",10,"max. number of nonlin. iterations",&fdyn);
  IntParameter("INITSTATITEMAX",5,"max number of nonlinear iterations for initial stationary solution",&fdyn);
  IntParameter("GRIDVEL",1,"order of accuracy of mesh velocity determination",&fdyn);
  DoubleParameter("TIMESTEP",0.01,"Time increment dt",&fdyn);
  DoubleParameter("MAXTIME",1000.0,"Total simulation time",&fdyn);
  DoubleParameter("ALPHA_M",1.0,"Time integration factor",&fdyn);
  DoubleParameter("ALPHA_F",1.0,"Time integration factor",&fdyn);
  DoubleParameter("GAMMA",1.0,"Time integration factor",&fdyn);
  DoubleParameter("THETA",0.66,"Time integration factor",&fdyn);

  DoubleParameter("CONVTOL",1e-6,"Tolerance for convergence check",&fdyn);
  DoubleParameter("STEADYTOL",1e-6,"Tolerance for steady state check",&fdyn);
  DoubleParameter("START_THETA",1.0,"Time integration factor for starting scheme",&fdyn);

 /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& andyn = list->sublist("ARTERIAL DYNAMIC",false,"");

  setStringToIntegralParameter<int>("DYNAMICTYP","ExpTaylorGalerkin",
                               "Explicit Taylor Galerkin Scheme",
                               tuple<std::string>(
                                 "ExpTaylorGalerkin"
                                 ),
                               tuple<int>(
                                typ_tay_gal
                                ),
                               &andyn);

  DoubleParameter("TIMESTEP",0.01,"Time increment dt",&andyn);
  IntParameter("NUMSTEP",0,"Number of Time Steps",&andyn);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&andyn);
  IntParameter("UPRES",1,"Increment for writing solution",&andyn);
 /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& redawdyn = list->sublist("REDUCED DIMENSIONAL AIRWAYS DYNAMIC",false,"");

  setStringToIntegralParameter<int>("DYNAMICTYP","CrankNicolson",
                               "CrankNicolson Scheme",
                               tuple<std::string>(
                                 "CrankNicolson"
                                 ),
                               tuple<int>(
                                typ_crank_nicolson
                                ),
                               &redawdyn);


  setStringToIntegralParameter<int>("SOLVERTYPE","Linear",
                               "Solver type",
                               tuple<std::string>(
                                 "Linear",
                                 "Nonlinear"
                                 ),
                               tuple<int>(
                                 linear,
                                 nonlinear
                                ),
                               &redawdyn);

  DoubleParameter("TIMESTEP",0.01,"Time increment dt",&redawdyn);
  IntParameter("NUMSTEP",0,"Number of Time Steps",&redawdyn);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&redawdyn);
  IntParameter("UPRES",1,"Increment for writing solution",&redawdyn);

  IntParameter("MAXITERATIONS",1,"maximum iteration steps",&redawdyn);
  DoubleParameter("TOLERANCE",1.0E-6,"tolerance",&redawdyn);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fdyn_stab = fdyn.sublist("STABILIZATION",false,"");

  // this parameter seperates stabilized from unstabilized methods
  setStringToIntegralParameter<int>("STABTYPE",
                               "residual_based",
                               "Apply (un)stabilized fluid formulation",
                               tuple<std::string>(
                                 "no_stabilization",
                                 "inconsistent",
                                 "residual_based"),
                               tuple<std::string>(
                                 "Do not use any stabilization -> inf-sup stable elements required!",
                                 "Similar to residual based without second derivatives (i.e. only consistent for tau->0, but faster)",
                                 "Use a residual-based stabilization or, more generally, a stabilization \nbased on the concept of the residual-based variational multiscale method...\nExpecting additional input")  ,
                               tuple<int>(0,1,2),
                               &fdyn_stab);

  // the following parameters are necessary only if a residual based stabilized method is applied
  setStringToIntegralParameter<int>("TDS",
                               "quasistatic",
                               "Flag to allow time dependency of subscales for residual-based stabilization.",
                               tuple<std::string>(
                                 "quasistatic",
                                 "time_dependent"),
                               tuple<std::string>(
                                 "Use a quasi-static residual-based stabilization (standard case)",
                                 "Residual-based stabilization including time evolution equations for subscales"),
                                 tuple<int>(
                                   INPAR::FLUID::subscales_quasistatic,
                                   INPAR::FLUID::subscales_time_dependent),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("TRANSIENT",
                               "no_transient",
                               "Specify how to treat the transient term.",
                               tuple<std::string>(
                                 "no_transient",
                                 "yes_transient",
                                 "transient_complete"),
                               tuple<std::string>(
                                 "Do not use transient term (currently only opportunity for quasistatic stabilization)",
                                 "Use transient term (recommended for time dependent subscales)",
                                 "Use transient term including a linearisation of 1/tau"),
                               tuple<int>(
                                   INPAR::FLUID::inertia_stab_drop,
                                   INPAR::FLUID::inertia_stab_keep,
                                   INPAR::FLUID::inertia_stab_keep_complete),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("PSPG",
                               "yes_pspg",
                               "Flag to (de)activate PSPG.",
                               tuple<std::string>(
                                 "no_pspg",
                                 "yes_pspg"),
                               tuple<std::string>(
                                 "No PSPG -> inf-sup-stable elements mandatory",
                                 "Use PSPG -> allowing for equal-order interpolation"),
                               tuple<int>(
                                 INPAR::FLUID::pstab_assume_inf_sup_stable,   //No PSPG -> inf-sup-stable elements mandatory
                                 INPAR::FLUID::pstab_use_pspg),               // Use PSPG -> allowing for equal-order interpolation
                               &fdyn_stab);



  setStringToIntegralParameter<int>("SUPG",
                               "yes_supg",
                               "Flag to (de)activate SUPG.",
                               tuple<std::string>(
                                 "no_supg",
                                 "yes_supg"),
                               tuple<std::string>(
                                 "No SUPG",
                                 "Use SUPG."),
                               tuple<int>(
                                 INPAR::FLUID::convective_stab_none,  // no SUPG stabilization
                                 INPAR::FLUID::convective_stab_supg), // use SUPG stabilization
                               &fdyn_stab);

  setStringToIntegralParameter<int>("VSTAB",
                               "no_vstab",
                               "Flag to (de)activate viscous term in residual-based stabilization.",
                               tuple<std::string>(
                                 "no_vstab",
                                 "vstab_gls",
                                 "vstab_gls_rhs",
                                 "vstab_usfem",
                                 "vstab_usfem_rhs"
                                 ),
                               tuple<std::string>(
                                 "No viscous term in stabilization",
                                 "Viscous stabilization of GLS type",
                                 "Viscous stabilization of GLS type, included only on the right hand side",
                                 "Viscous stabilization of USFEM type",
                                 "Viscous stabilization of USFEM type, included only on the right hand side"
                                 ),
                                 tuple<int>(
                                     INPAR::FLUID::viscous_stab_none,
                                     INPAR::FLUID::viscous_stab_gls,
                                     INPAR::FLUID::viscous_stab_gls_only_rhs,
                                     INPAR::FLUID::viscous_stab_usfem,
                                     INPAR::FLUID::viscous_stab_usfem_only_rhs
                                   ),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("RSTAB",
                               "no_rstab",
                               "Flag to (de)activate reactive term in residual-based stabilization.",
                               tuple<std::string>(
                                 "no_rstab",
                                 "rstab_gls",
                                 "rstab_usfem"
                                 ),
                               tuple<std::string>(
                                 "no reactive term in stabilization",
                                 "reactive stabilization of GLS type",
                                 "reactive stabilization of USFEM type"
                                 ),
                                 tuple<int>(
                                     INPAR::FLUID::reactive_stab_none,
                                     INPAR::FLUID::reactive_stab_gls,
                                     INPAR::FLUID::reactive_stab_usfem
                                   ),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("CSTAB",
                               "cstab_qs",
                               "Flag to (de)activate least-squares stabilization of continuity equation.",
                               tuple<std::string>(
                                 "no_cstab",
                                 "cstab_qs"),
                               tuple<std::string>(
                                 "No continuity stabilization",
                                 "Quasistatic continuity stabilization"),
                               tuple<int>(
                                   INPAR::FLUID::continuity_stab_none,
                                   INPAR::FLUID::continuity_stab_yes
                                 ),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("CROSS-STRESS",
                               "no_cross",
                               "Flag to (de)activate cross-stress term -> residual-based VMM.",
                               tuple<std::string>(
                                 "no_cross",
                                 "yes_cross",
                                 "cross_rhs"
                                 //"cross_complete"
                                 ),
                               tuple<std::string>(
                                 "No cross-stress term",
                                 "Include the cross-stress term with a linearization of the convective part",
                                 "Include cross-stress term, but only explicitly on right hand side"
                                 //""
                                 ),
                               tuple<int>(
                                   INPAR::FLUID::cross_stress_stab_none,
                                   INPAR::FLUID::cross_stress_stab,
                                   INPAR::FLUID::cross_stress_stab_only_rhs
                                ),
                               &fdyn_stab);

  setStringToIntegralParameter<int>("REYNOLDS-STRESS",
                               "no_reynolds",
                               "Flag to (de)activate Reynolds-stress term -> residual-based VMM.",
                               tuple<std::string>(
                                 "no_reynolds",
                                 "yes_reynolds",
                                 "reynolds_rhs"
                                 //"reynolds_complete"
                                 ),
                               tuple<std::string>(
                                 "No Reynolds-stress term",
                                 "Include Reynolds-stress term with linearisation",
                                 "Include Reynolds-stress term explicitly on right hand side"
                                 //""
                                 ),
                               tuple<int>(
                                   INPAR::FLUID::reynolds_stress_stab_none,
                                   INPAR::FLUID::reynolds_stress_stab,
                                   INPAR::FLUID::reynolds_stress_stab_only_rhs
                               ),
                               &fdyn_stab);

  // this parameter selects the tau definition applied
  // the options "tau_not_defined" are only available in Peter's Genalpha code
  // (there, which_tau is set via the string, not via INPAR::FLUID::TauType)
  setStringToIntegralParameter<int>("DEFINITION_TAU",
                               "Franca_Barrenechea_Valentin_Frey_Wall",
                               "Definition of tau_M and Tau_C",
                               tuple<std::string>(
                                 "Taylor_Hughes_Zarins",
                                 "Taylor_Hughes_Zarins_wo_dt",
                                 "Taylor_Hughes_Zarins_Whiting_Jansen",
                                 "Taylor_Hughes_Zarins_Whiting_Jansen_wo_dt",
                                 "Taylor_Hughes_Zarins_scaled",
                                 "Taylor_Hughes_Zarins_scaled_wo_dt",
                                 "Franca_Barrenechea_Valentin_Frey_Wall",
                                 "Franca_Barrenechea_Valentin_Frey_Wall_wo_dt",
                                 "Shakib_Hughes_Codina",
                                 "Shakib_Hughes_Codina_wo_dt",
                                 "Codina",
                                 "Codina_wo_dt",
                                 "Franca_Madureira_Valentin_Badia_Codina",
                                 "Franca_Madureira_Valentin_Badia_Codina_wo_dt",
                                 //"BFVW_gradient_based_hk",
                                 "Smoothed_FBVW"),
                               tuple<int>(
                                   INPAR::FLUID::tau_taylor_hughes_zarins,
                                   INPAR::FLUID::tau_taylor_hughes_zarins_wo_dt,
                                   INPAR::FLUID::tau_taylor_hughes_zarins_whiting_jansen,
                                   INPAR::FLUID::tau_taylor_hughes_zarins_whiting_jansen_wo_dt,
                                   INPAR::FLUID::tau_taylor_hughes_zarins_scaled,
                                   INPAR::FLUID::tau_taylor_hughes_zarins_scaled_wo_dt,
                                   INPAR::FLUID::tau_franca_barrenechea_valentin_frey_wall,
                                   INPAR::FLUID::tau_franca_barrenechea_valentin_frey_wall_wo_dt,
                                   INPAR::FLUID::tau_shakib_hughes_codina,
                                   INPAR::FLUID::tau_shakib_hughes_codina_wo_dt,
                                   INPAR::FLUID::tau_codina,
                                   INPAR::FLUID::tau_codina_wo_dt,
                                   INPAR::FLUID::tau_franca_madureira_valentin_badia_codina,
                                   INPAR::FLUID::tau_franca_madureira_valentin_badia_codina_wo_dt,
                                   //INPAR::FLUID::tau_not_defined,
                                   INPAR::FLUID::tau_not_defined),
                               &fdyn_stab);

  // this parameter selects the location where tau is evaluated
  setStringToIntegralParameter<int>("EVALUATION_TAU",
                               "element_center",
                               "Location where tau is evaluated",
                               tuple<std::string>(
                                 "element_center",
                                 "integration_point"),
                               tuple<std::string>(
                                 "evaluate tau at element center",
                                 "evaluate tau at integration point")  ,
                                tuple<int>(0,1),
                               &fdyn_stab);

  // this parameter selects the location where the material law is evaluated
  // (does not fit here very well, but parameter transfer is easier)
  setStringToIntegralParameter<int>("EVALUATION_MAT",
                               "element_center",
                               "Location where material law is evaluated",
                               tuple<std::string>(
                                 "element_center",
                                 "integration_point"),
                               tuple<std::string>(
                                 "evaluate material law at element center",
                                 "evaluate material law at integration point")  ,
                                tuple<int>(0,1),
                               &fdyn_stab);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fdyn_turbu = fdyn.sublist("TURBULENCE MODEL",false,"");

  setStringToIntegralParameter<int>(
    "TURBULENCE_APPROACH",
    "DNS_OR_RESVMM_LES",
    "There are several options to deal with turbulent flows.",
    tuple<std::string>(
      "DNS_OR_RESVMM_LES",
      "CLASSICAL_LES",
      "RANS"),
    tuple<std::string>(
      "Try to solve flow as an underresolved DNS.\nMind that your stabilisation already acts as a kind of turbulence model!",
      "Perform a classical Large Eddy Simulation adding \naddititional turbulent viscosity. This may be based on various physical models.",
      "Solve Reynolds averaged Navier Stokes using an \nalgebraic, one- or two equation closure.\nNot implemented yet."),
    tuple<int>(0,1,2),
    &fdyn_turbu);

  setStringToIntegralParameter<int>(
    "PHYSICAL_MODEL",
    "no_model",
    "Classical LES approaches require an additional model for\nthe turbulent viscosity.",
    tuple<std::string>(
      "no_model",
      "Smagorinsky",
      "Smagorinsky_with_van_Driest_damping",
      "Dynamic_Smagorinsky",
      "Scale_Similarity",
      "Multifractal_Subgrid_Scales"),
    tuple<std::string>(
      "If classical LES is our turbulence approach, this is a contradiction and should cause a dserror.",
      "Classical constant coefficient Smagorinsky model. Be careful if you \nhave a wall bounded flow domain!",
      "Use an exponential damping function for the turbulent viscosity \nclose to the wall. This is only implemented for a channel geometry of \nheight 2 in y direction. The viscous lengthscale l_tau is \nrequired as additional input.",
      "The solution is filtered and by comparison of the filtered \nvelocity field with the real solution, the Smagorinsky constant is \nestimated in each step --- mind that this procedure includes \nan averaging in the xz plane, hence this implementation will only work \nfor a channel flow.",
      "",
      ""),
    tuple<int>(0,1,2,3,4,5),
    &fdyn_turbu);

  DoubleParameter("C_SMAGORINSKY",0.0,"Constant for the Smagorinsky model. Something between 0.1 to 0.24",&fdyn_turbu);

  DoubleParameter("C_TURBPRANDTL",1.0,"(Constant) turbulent Prandtl number for the Smagorinsky model in scalar transport.",&fdyn_turbu);

  DoubleParameter("C_SCALE_SIMILARITY",1.0,"Constant for the scale similarity model. Something between 0.45 +- 0.15 or 1.0.", &fdyn_turbu);

  setStringToIntegralParameter<int>(
    "FILTERTYPE",
    "box_filter",
    "Specify the filter type for scale separation in LES",
    tuple<std::string>(
      "box_filter",
      "algebraic_multigrid_operator"),
    tuple<std::string>(
      "classical box filter",
      "scale separation by algebraic multigrid operator"),
    tuple<int>(0,1),
    &fdyn_turbu);

  {
    // a standard Teuchos::tuple can have at maximum 10 entries! We have to circumvent this here.
    // Otherwise BACI DEBUG version will crash during runtime!
    Teuchos::Tuple<std::string,12> name;
    Teuchos::Tuple<int,12> label;
    name[ 0] = "no";                                      label[ 0] = 0;
    name[ 1] = "time_averaging";                          label[ 1] = 1;
    name[ 2] = "channel_flow_of_height_2";                label[ 2] = 2;
    name[ 3] = "lid_driven_cavity";                       label[ 3] = 3;
    name[ 4] = "backward_facing_step";                    label[ 4] = 4;
    name[ 5] = "square_cylinder";                         label[ 5] = 5;
    name[ 6] = "square_cylinder_nurbs";                   label[ 6] = 6;
    name[ 7] = "rotating_circular_cylinder_nurbs";        label[ 7] = 7;
    name[ 8] = "rotating_circular_cylinder_nurbs_scatra"; label[ 8] = 8;
    name[ 9] = "loma_channel_flow_of_height_2";           label[ 9] = 9;
    name[10] = "loma_lid_driven_cavity";                  label[10] = 10;
    name[11] = "loma_backward_facing_step";               label[11] = 11;

    Teuchos::Tuple<std::string,12> description;
    description[0]="The flow is not further specified, so spatial averaging \nand hence the standard sampling procedure is not possible";
    description[1]="The flow is not further specified, but time averaging of velocity and pressure field is performed";
    description[2]="For this flow, all statistical data could be averaged in \nthe homogenous planes --- it is essentially a statistically one dimensional flow.";
    description[3]="For this flow, all statistical data are evaluated on the center lines of the xy-midplane, averaged only over time.";
    description[4]="For this flow, statistical data are evaluated on various lines, averaged over time and z.";
    description[5]="For this flow, statistical data are evaluated on various lines of the xy-midplane, averaged only over time.";
    description[6]="For this flow, statistical data are evaluated on various lines of the xy-midplane, averaged over time and eventually in one hom.direction.";
    description[7]="For this flow, statistical data is computed in concentric surfaces and averaged. in time and in one hom. direction";
    description[8]="For this flow with mass transport, statistical data is computed in concentric surfaces and averaged. in time and in one hom. direction";
    description[9]="For this low-Mach-number flow, all statistical data could be averaged in \nthe homogenous planes --- it is essentially a statistically one dimensional flow.";
    description[10]="For this low-Mach-number flow, all statistical data are evaluated on the center lines of the xy-midplane, averaged only over time.";
    description[11]="For this low-Mach-number flow, statistical data are evaluated on various lines, averaged over time and z.";

    setStringToIntegralParameter<int>(
        "CANONICAL_FLOW",
        "no",
        "Sampling is different for different canonical flows \n--- so specify what kind of flow you've got",
        name,
        description,
        label,
        &fdyn_turbu);
  }

  setStringToIntegralParameter<int>(
    "HOMDIR",
    "not_specified",
    "Specify the homogenous direction(s) of a flow",
    tuple<std::string>(
      "not_specified",
      "x"            ,
      "y"            ,
      "z"            ,
      "xy"           ,
      "xz"           ,
      "yz"           ),
    tuple<std::string>(
      "no homogeneous directions available, averaging is restricted to time averaging",
      "average along x-direction"                                                     ,
      "average along y-direction"                                                     ,
      "average along z-direction"                                                     ,
      "Wall normal direction is z, average in x and y direction"                      ,
      "Wall normal direction is y, average in x and z direction (standard case)"      ,
      "Wall normal direction is x, average in y and z direction"                      ),
    tuple<int>(0,1,2,3,4,5,6),
    &fdyn_turbu);

  DoubleParameter(
    "CHANNEL_L_TAU",
    0.0,
    "Used for normalisation of the wall normal distance in the Van \nDriest Damping function. May be taken from the output of \nthe apply_mesh_stretching.pl preprocessing script.",
    &fdyn_turbu);

  DoubleParameter(
    "CHAN_AMPL_INIT_DIST",
    0.1,
    "Max. amplitude of the random disturbance in percent of the initial value in mean flow direction.",
    &fdyn_turbu);

  IntParameter("SAMPLING_START",10000000,"Time step after when sampling shall be started",&fdyn_turbu);
  IntParameter("SAMPLING_STOP",1,"Time step when sampling shall be stopped",&fdyn_turbu);
  IntParameter("DUMPING_PERIOD",1,"Period of time steps after which statistical data shall be dumped",&fdyn_turbu);

  /*----------------------------------------------------------------------*/
   Teuchos::ParameterList& fdyn_turbinf = fdyn.sublist("TURBULENT INFLOW",false,"");

   setStringToIntegralParameter<int>("TURBULENTINFLOW",
                                "no",
                                "Flag to (de)activate potential separate turbulent inflow section",
                                tuple<std::string>(
                                  "no",
                                  "yes"),
                                tuple<std::string>(
                                  "no turbulent inflow section",
                                  "turbulent inflow section"),
                                tuple<int>(0,1),
                                &fdyn_turbinf);

   setStringToIntegralParameter<int>("INITIALINFLOWFIELD","zero_field",
                                "Initial field for inflow section",
                                tuple<std::string>(
                                  "zero_field",
                                  "field_by_function",
                                  "disturbed_field_from_function"),
                                tuple<int>(
                                      INPAR::FLUID::initfield_zero_field,
                                      INPAR::FLUID::initfield_field_by_function,
                                      INPAR::FLUID::initfield_disturbed_field_from_function),
                                &fdyn_turbinf);

   IntParameter("INFLOWFUNC",-1,"Function number for initial flow field in inflow section",&fdyn_turbinf);

   DoubleParameter(
     "INFLOW_INIT_DIST",
     0.1,
     "Max. amplitude of the random disturbance in percent of the initial value in mean flow direction.",
     &fdyn_turbinf);

   IntParameter("NUMINFLOWSTEP",1,"Total number of time steps for development of turbulent flow",&fdyn_turbinf);

   setStringToIntegralParameter<int>(
       "CANONICAL_INFLOW",
       "no",
       "Sampling is different for different canonical flows \n--- so specify what kind of flow you've got",
       tuple<std::string>(
       "no",
       "time_averaging",
       "channel_flow_of_height_2"),
       tuple<std::string>(
       "The flow is not further specified, so spatial averaging \nand hence the standard sampling procedure is not possible",
       "The flow is not further specified, but time averaging of velocity and pressure field is performed",
       "For this flow, all statistical data could be averaged in \nthe homogenous planes --- it is essentially a statistically one dimensional flow."),
       tuple<int>(0,1,2),
       &fdyn_turbinf);

   setStringToIntegralParameter<int>(
     "INFLOW_HOMDIR",
     "not_specified",
     "Specify the homogenous direction(s) of a flow",
     tuple<std::string>(
       "not_specified",
       "x"            ,
       "y"            ,
       "z"            ,
       "xy"           ,
       "xz"           ,
       "yz"           ),
     tuple<std::string>(
       "no homogeneous directions available, averaging is restricted to time averaging",
       "average along x-direction"                                                     ,
       "average along y-direction"                                                     ,
       "average along z-direction"                                                     ,
       "Wall normal direction is z, average in x and y direction"                      ,
       "Wall normal direction is y, average in x and z direction (standard case)"      ,
       "Wall normal direction is x, average in y and z direction"                      ),
     tuple<int>(0,1,2,3,4,5,6),
     &fdyn_turbinf);

   IntParameter("INFLOW_SAMPLING_START",10000000,"Time step after when sampling shall be started",&fdyn_turbinf);
   IntParameter("INFLOW_SAMPLING_STOP",1,"Time step when sampling shall be stopped",&fdyn_turbinf);
   IntParameter("INFLOW_DUMPING_PERIOD",1,"Period of time steps after which statistical data shall be dumped",&fdyn_turbinf);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& adyn = list->sublist("ALE DYNAMIC",false,"");

  DoubleParameter("TIMESTEP",0.1,"",&adyn);
  IntParameter("NUMSTEP",41,"",&adyn);
  DoubleParameter("MAXTIME",4.0,"",&adyn);
  setStringToIntegralParameter<int>("ALE_TYPE","classic_lin","ale mesh movement algorithm",
                               tuple<std::string>("classic_lin","incr_lin","laplace","springs","springs_fixed_ref"),
                               tuple<int>(ALE_DYNAMIC::classic_lin,
                                          ALE_DYNAMIC::incr_lin,
                                          ALE_DYNAMIC::laplace,
                                          ALE_DYNAMIC::springs,
                                          ALE_DYNAMIC::springs_fixed_ref),
                               &adyn);
  IntParameter("NUM_INITSTEP",0,"",&adyn);
  IntParameter("RESULTSEVRY",1,"",&adyn);

  setStringToIntegralParameter<int>("QUALITY","none","unused",
                               tuple<std::string>("none","NONE"),
                               tuple<int>(
                                 ALE_DYNAMIC::no_quality,
                                 ALE_DYNAMIC::no_quality),
                               &adyn);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& scatradyn = list->sublist(
      "SCALAR TRANSPORT DYNAMIC",
      false,
      "control parameters for scalar transport problems\n");

  setStringToIntegralParameter<int>("SOLVERTYPE","linear_full",
                               "type of scalar transport solver",
                               tuple<std::string>(
                                 "linear_full",
                                 "linear_incremental",
                                 "nonlinear"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::solvertype_linear_full,
                                   INPAR::SCATRA::solvertype_linear_incremental,
                                   INPAR::SCATRA::solvertype_nonlinear),
                               &scatradyn);

  setStringToIntegralParameter<int>("TIMEINTEGR","One_Step_Theta",
                               "Time Integration Scheme",
                               tuple<std::string>(
                                 "Stationary",
                                 "One_Step_Theta",
                                 "BDF2",
                                 "Gen_Alpha"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::timeint_stationary,
                                   INPAR::SCATRA::timeint_one_step_theta,
                                   INPAR::SCATRA::timeint_bdf2,
                                   INPAR::SCATRA::timeint_gen_alpha
                                 ),
                               &scatradyn);

  DoubleParameter("MAXTIME",1000.0,"Total simulation time",&scatradyn);
  IntParameter("NUMSTEP",20,"Total number of time steps",&scatradyn);
  DoubleParameter("TIMESTEP",0.1,"Time increment dt",&scatradyn);
  DoubleParameter("THETA",0.5,"One-step-theta time integration factor",&scatradyn);
  DoubleParameter("ALPHA_M",0.5,"Generalized-alpha time integration factor",&scatradyn);
  DoubleParameter("ALPHA_F",0.5,"Generalized-alpha time integration factor",&scatradyn);
  DoubleParameter("GAMMA",0.5,"Generalized-alpha time integration factor",&scatradyn);
  //IntParameter("WRITESOLEVRY",1,"Increment for writing solution",&scatradyn);
  IntParameter("UPRES",1,"Increment for writing solution",&scatradyn);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&scatradyn);
  setNumericStringParameter("MATID","-1",
               "Material IDs for automatic mesh generation",
               &scatradyn);

  setStringToIntegralParameter<int>("VELOCITYFIELD","zero",
                               "type of velocity field used for scalar transport problems",
                               tuple<std::string>(
                                 "zero",
                                 "function",
                                 "function_and_curve",
                                 "Navier_Stokes"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::velocity_zero,
                                   INPAR::SCATRA::velocity_function,
                                   INPAR::SCATRA::velocity_function_and_curve,
                                   INPAR::SCATRA::velocity_Navier_Stokes),
                               &scatradyn);

  IntParameter("VELFUNCNO",-1,"function number for scalar transport velocity field",&scatradyn);

  IntParameter("VELCURVENO",-1,"curve number for time-dependent scalar transport velocity field",&scatradyn);

  setStringToIntegralParameter<int>("INITIALFIELD","zero_field",
                               "Initial Field for scalar transport problem",
                               tuple<std::string>(
                                 "zero_field",
                                 "field_by_function",
                                 "field_by_condition",
                                 "disturbed_field_by_function",
                                 "1D_DISCONTPV",
                                 "FLAME_VORTEX_INTERACTION",
                                 "RAYTAYMIXFRAC",
                                 "L_shaped_domain",
                                 "facing_flame_fronts"),
                               tuple<int>(
                                   INPAR::SCATRA::initfield_zero_field,
                                   INPAR::SCATRA::initfield_field_by_function,
                                   INPAR::SCATRA::initfield_field_by_condition,
                                   INPAR::SCATRA::initfield_disturbed_field_by_function,
                                   INPAR::SCATRA::initfield_discontprogvar_1D,
                                   INPAR::SCATRA::initfield_flame_vortex_interaction,
                                   INPAR::SCATRA::initfield_raytaymixfrac,
                                   INPAR::SCATRA::initfield_Lshapeddomain,
                                   INPAR::SCATRA::initfield_facing_flame_fronts),
                               &scatradyn);

  IntParameter("INITFUNCNO",-1,"function number for scalar transport initial field",&scatradyn);

  setStringToIntegralParameter<int>("CALCERROR","No",
                               "compute error compared to analytical solution",
                               tuple<std::string>(
                                 "No",
                                 "Kwok_Wu",
                                 "ConcentricCylinders"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::calcerror_no,
                                   INPAR::SCATRA::calcerror_Kwok_Wu,
                                   INPAR::SCATRA::calcerror_cylinder),
                               &scatradyn);

  setStringToIntegralParameter<int>("WRITEFLUX","No","output of diffusive/total flux vectors",
                               tuple<std::string>(
                                 "No",
                                 "totalflux_domain",
                                 "diffusiveflux_domain",
                                 "totalflux_boundary",
                                 "diffusiveflux_boundary"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::flux_no,
                                   INPAR::SCATRA::flux_total_domain,
                                   INPAR::SCATRA::flux_diffusive_domain,
                                   INPAR::SCATRA::flux_total_boundary,
                                   INPAR::SCATRA::flux_diffusive_boundary),
                               &scatradyn);

  setNumericStringParameter("WRITEFLUX_IDS","-1",
      "Write diffusive/total flux vector fields for these scalar fields only (starting with 1)",
      &scatradyn);

  BoolParameter("OUTMEAN","No","Output of mean values for scalars and density",&scatradyn);
  BoolParameter("OUTPUT_GMSH","No","Do you want to write Gmsh postprocessing files?",&scatradyn);

  setStringToIntegralParameter<int>("CONVFORM","convective","form of convective term",
                               tuple<std::string>(
                                 "convective",
                                 "conservative"
                                 ),
                               tuple<int>(
                                 INPAR::SCATRA::convform_convective,
                                 INPAR::SCATRA::convform_conservative),
                               &scatradyn);

  BoolParameter("NEUMANNINFLOW",
      "no","Flag to (de)activate potential Neumann inflow term(s)",&scatradyn);

  BoolParameter("SKIPINITDER",
      "no","Flag to skip computation of initial time derivative",&scatradyn);

  BoolParameter("INITPOTCALC","no",
      "Automatically calculate initial field for electric potential",&scatradyn);

  setStringToIntegralParameter<int>("FSSUGRDIFF",
                               "No",
                               "fine-scale subgrid diffusivity",
                               tuple<std::string>(
                                 "No",
                                 "artificial",
                                 "Smagorinsky_all",
                                 "Smagorinsky_small"
                                 ),
                               tuple<int>(
                                   INPAR::SCATRA::fssugrdiff_no,
                                   INPAR::SCATRA::fssugrdiff_artificial,
                                   INPAR::SCATRA::fssugrdiff_smagorinsky_all,
                                   INPAR::SCATRA::fssugrdiff_smagorinsky_small),
                               &scatradyn);

  BoolParameter("BLOCKPRECOND","NO",
      "Switch to block-preconditioned family of solvers, needs additional SCALAR TRANSPORT ELECTRIC POTENTIAL SOLVER block!",&scatradyn);

  setStringToIntegralParameter<int>("SCATRATYPE","Undefined",
                               "Type of scalar transport problem",
                               tuple<std::string>(
                                 "Undefined",
                                 "ConvectionDiffusion",
                                 "LowMachNumberFlow",
                                 "Elch_ENC",
                                 "Elch_ENC_PDE",
                                 "Elch_Possion",
                                 "LevelSet"),
                               tuple<int>(
                                 INPAR::SCATRA::scatratype_undefined,
                                 INPAR::SCATRA::scatratype_condif,
                                 INPAR::SCATRA::scatratype_loma,
                                 INPAR::SCATRA::scatratype_elch_enc,
                                 INPAR::SCATRA::scatratype_elch_enc_pde,
                                 INPAR::SCATRA::scatratype_elch_poisson,
                                 INPAR::SCATRA::scatratype_levelset),
                                 &scatradyn);

  setStringToIntegralParameter<int>("MESHTYING", "no", "Flag to (de)activate mesh tying algorithm",
                                  tuple<std::string>(
                                      "no",
                                      "Condensed_Smat",
                                      "Condensed_Bmat",
                                      "SaddlePointSystem_coupled",
                                      "SaddlePointSystem_pc"),
                                    tuple<int>(
                                        INPAR::SCATRA::no_meshtying,
                                        INPAR::SCATRA::condensed_smat,
                                        INPAR::SCATRA::condensed_bmat,
                                        INPAR::SCATRA::sps_coupled,
                                        INPAR::SCATRA::sps_pc),
                                    &scatradyn);
  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& scatra_nonlin = scatradyn.sublist(
      "NONLINEAR",
      false,
      "control parameters for solving nonlinear SCATRA problems\n");

  IntParameter("ITEMAX",10,"max. number of nonlin. iterations",&scatra_nonlin);
  DoubleParameter("CONVTOL",1e-6,"Tolerance for convergence check",&scatra_nonlin);
  BoolParameter("EXPLPREDICT","no","do an explicit predictor step before starting nonlinear iteration",&scatra_nonlin);
  DoubleParameter("ABSTOLRES",1e-14,"Absolute tolerance for deciding if residual of nonlinear problem is already zero",&scatra_nonlin);

  // convergence criteria adaptivity
  BoolParameter("ADAPTCONV","yes","Switch on adaptive control of linear solver tolerance for nonlinear solution",&scatra_nonlin);
  DoubleParameter("ADAPTCONV_BETTER",0.1,"The linear solver shall be this much better than the current nonlinear residual in the nonlinear convergence limit",&scatra_nonlin);

  /*----------------------------------------------------------------------*/
//  Teuchos::ParameterList& scatradyn_levelset = scatradyn.sublist("LEVELSET",false,
//      "control parameters for a level set function");
//
//  setStringToIntegralParameter<int>("REINITIALIZATION","None",
//                               "Type of reinitialization strategy for level set function",
//                               tuple<std::string>(
//                                 "None",
//                                 "DirectDistance",
//                                 "Sussman",
//                                 "InterfaceProjection",
//                                 "Function",
//                                 "Signed_Distance_Function"),
//                               tuple<int>(
//                                 INPAR::SCATRA::reinitaction_none,
//                                 INPAR::SCATRA::reinitaction_directdistance,
//                                 INPAR::SCATRA::reinitaction_sussman,
//                                 INPAR::SCATRA::reinitaction_interfaceprojection,
//                                 INPAR::SCATRA::reinitaction_function,
//                                 INPAR::SCATRA::reinitaction_signeddistancefunction),
//                               &scatradyn_levelset);
//
//  setStringToIntegralParameter<int>("MASSCALCULATION","No",
//                               "Type of mass calculation",
//                               tuple<std::string>(
//                                 "No",
//                                 "Squares",
//                                 "Interpolated"),
//                               tuple<int>(
//                                 INPAR::SCATRA::masscalc_none,
//                                 INPAR::SCATRA::masscalc_squares,
//                                 INPAR::SCATRA::masscalc_interpolated),
//                             &scatradyn_levelset);

/*----------------------------------------------------------------------*/
  Teuchos::ParameterList& scatradyn_stab = scatradyn.sublist("STABILIZATION",false,"");

  // this parameter governs type of stabilization
  setStringToIntegralParameter<int>("STABTYPE",
                                    "SUPG",
                                    "type of stabilization (if any)",
                               tuple<std::string>(
                                 "no_stabilization",
                                 "SUPG",
                                 "GLS",
                                 "USFEM"),
                               tuple<std::string>(
                                 "Do not use any stabilization -> only reasonable for low-Peclet-number flows",
                                 "Use SUPG",
                                 "Use GLS",
                                 "Use USFEM")  ,
                               tuple<int>(
                                   INPAR::SCATRA::stabtype_no_stabilization,
                                   INPAR::SCATRA::stabtype_SUPG,
                                   INPAR::SCATRA::stabtype_GLS,
                                   INPAR::SCATRA::stabtype_USFEM),
                               &scatradyn_stab);

  // this parameter governs whether subgrid-scale velocity is included
  BoolParameter("SUGRVEL","no","potential incorporation of subgrid-scale velocity",&scatradyn_stab);

  // this parameter governs whether all-scale subgrid diffusivity is included
  BoolParameter("ASSUGRDIFF","no",
      "potential incorporation of all-scale subgrid diffusivity (a.k.a. discontinuity-capturing) term",&scatradyn_stab);

  // this parameter selects the tau definition applied
  setStringToIntegralParameter<int>("DEFINITION_TAU",
                               "Franca_Valentin",
                               "Definition of tau",
                               tuple<std::string>(
                                 "Taylor_Hughes_Zarins",
                                 "Taylor_Hughes_Zarins_wo_dt",
                                 "Franca_Valentin",
                                 "Franca_Valentin_wo_dt",
                                 "Shakib_Hughes_Codina",
                                 "Shakib_Hughes_Codina_wo_dt",
                                 "Codina",
                                 "Codina_wo_dt",
                                 "Exact_1D",
                                 "Zero"),
                                tuple<int>(
                                    INPAR::SCATRA::tau_taylor_hughes_zarins,
                                    INPAR::SCATRA::tau_taylor_hughes_zarins_wo_dt,
                                    INPAR::SCATRA::tau_franca_valentin,
                                    INPAR::SCATRA::tau_franca_valentin_wo_dt,
                                    INPAR::SCATRA::tau_shakib_hughes_codina,
                                    INPAR::SCATRA::tau_shakib_hughes_codina_wo_dt,
                                    INPAR::SCATRA::tau_codina,
                                    INPAR::SCATRA::tau_codina_wo_dt,
                                    INPAR::SCATRA::tau_exact_1d,
                                    INPAR::SCATRA::tau_zero),
                               &scatradyn_stab);

  // this parameter selects the all-scale subgrid-diffusivity definition applied
  setStringToIntegralParameter<int>("DEFINITION_ASSGD",
                               "artificial_linear",
                               "Definition of (all-scale) subgrid diffusivity",
                               tuple<std::string>(
                                 "artificial_linear",
                                 "Hughes_etal_86_nonlinear",
                                 "Tezduyar_Park_86_nonlinear",
                                 "doCarmo_Galeao_91_nonlinear",
                                 "Almeida_Silva_97_nonlinear"),
                               tuple<std::string>(
                                 "classical linear artificial subgrid-diffusivity",
                                 "nonlinear isotropic according to Hughes et al. (1986)",
                                 "nonlinear isotropic according to Tezduyar and Park (1986)",
                                 "nonlinear isotropic according to doCarmo and Galeao (1991)",
                                 "nonlinear isotropic according to Almeida and Silva (1997)")  ,
                                tuple<int>(
                                    INPAR::SCATRA::assgd_artificial,
                                    INPAR::SCATRA::assgd_hughes,
                                    INPAR::SCATRA::assgd_tezduyar,
                                    INPAR::SCATRA::assgd_docarmo,
                                    INPAR::SCATRA::assgd_almeida),
                               &scatradyn_stab);

  // this parameter selects the location where tau is evaluated
  setStringToIntegralParameter<int>("EVALUATION_TAU",
                               "element_center",
                               "Location where tau is evaluated",
                               tuple<std::string>(
                                 "element_center",
                                 "integration_point"),
                               tuple<std::string>(
                                 "evaluate tau at element center",
                                 "evaluate tau at integration point")  ,
                                tuple<int>(
                                  INPAR::SCATRA::evaltau_element_center,
                                  INPAR::SCATRA::evaltau_integration_point),
                               &scatradyn_stab);

  // this parameter selects the location where the material law is evaluated
  // (does not fit here very well, but parameter transfer is easier)
  setStringToIntegralParameter<int>("EVALUATION_MAT",
                               "element_center",
                               "Location where material law is evaluated",
                               tuple<std::string>(
                                 "element_center",
                                 "integration_point"),
                               tuple<std::string>(
                                 "evaluate material law at element center",
                                 "evaluate material law at integration point"),
                               tuple<int>(
                                 INPAR::SCATRA::evalmat_element_center,
                                 INPAR::SCATRA::evalmat_integration_point),
                               &scatradyn_stab);

  // this parameter selects methods for improving consistency of stabilization terms
  setStringToIntegralParameter<int>("CONSISTENCY",
                               "no",
                               "improvement of consistency for stabilization",
                               tuple<std::string>(
                                 "no",
                                 "L2_projection_lumped"),
                               tuple<std::string>(
                                 "inconsistent",
                                 "L2 projection with lumped mass matrix")  ,
                                tuple<int>(
                                  INPAR::SCATRA::consistency_no,
                                  INPAR::SCATRA::consistency_l2_projection_lumped),
                               &scatradyn_stab);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& lomacontrol = list->sublist(
      "LOMA CONTROL",
      false,
      "control parameters for low-Mach-number flow problems\n");

  IntParameter("NUMSTEP",24,"Total number of time steps",&lomacontrol);
  DoubleParameter("TIMESTEP",0.1,"Time increment dt",&lomacontrol);
  DoubleParameter("MAXTIME",1000.0,"Total simulation time",&lomacontrol);
  IntParameter("ITEMAX",10,"Maximum number of outer iterations",&lomacontrol);
  DoubleParameter("CONVTOL",1e-6,"Tolerance for convergence check",&lomacontrol);
  IntParameter("UPRES",1,"Increment for writing solution",&lomacontrol);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&lomacontrol);
  setStringToIntegralParameter<int>("CONSTHERMPRESS","Yes",
                               "treatment of thermodynamic pressure in time",
                               tuple<std::string>(
                                 "No_energy",
                                 "No_mass",
                                 "Yes"
                                 ),
                               tuple<int>(0,1,2),
                               &lomacontrol);
  setStringToIntegralParameter<int>(
    "CANONICAL_FLOW",
    "no",
    "Information on special flows",
    tuple<std::string>(
      "no",
      "loma_channel_flow_of_height_2",
      "loma_lid_driven_cavity",
      "loma_backward_facing_step"),
    tuple<std::string>(
      "The flow is not further specified.",
      "low-Mach-number in channel",
      "low-Mach-number flow in lid-driven cavity",
      "low-Mach-number flow over a backward-facing step"),
    tuple<int>(0,1,2,3),
    &lomacontrol);
  IntParameter("SAMPLING_START",1,"Time step after when sampling shall be started",&lomacontrol);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& elchcontrol = list->sublist(
      "ELCH CONTROL",
      false,
      "control parameters for electrochemistry problems\n");

  DoubleParameter("MAXTIME",1000.0,"Total simulation time",&elchcontrol);
  IntParameter("NUMSTEP",24,"Total number of time steps",&elchcontrol);
  DoubleParameter("TIMESTEP",0.1,"Time increment dt",&elchcontrol);
  IntParameter("ITEMAX",10,"Maximum number of outer iterations",&elchcontrol);
  IntParameter("UPRES",1,"Increment for writing solution",&elchcontrol);
  DoubleParameter("CONVTOL",1e-6,"Convergence check tolerance for outer loop",&elchcontrol);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&elchcontrol);
  DoubleParameter("TEMPERATURE",298.0,"Constant temperature (Kelvin)",&elchcontrol);
  BoolParameter("MOVINGBOUNDARY","No","ELCH algorithm for deforming meshes",&elchcontrol);
  DoubleParameter("MOLARVOLUME",0.0,"Molar volume for electrode shape change computations",&elchcontrol);
  BoolParameter("NATURAL_CONVECTION","No","Include natural convection effects",&elchcontrol);
  BoolParameter("GALVANOSTATIC","No","flag for galvanostatic mode",&elchcontrol);
  IntParameter("GSTATCONDID_CATHODE",0,"condition id of electrode kinetics for cathode",&elchcontrol);
  IntParameter("GSTATCONDID_ANODE",1,"condition id of electrode kinetics for anode",&elchcontrol);
  DoubleParameter("GSTATCONVTOL",1.e-5,"Convergence check tolerance for galvanostatic mode",&elchcontrol);
  DoubleParameter("GSTATCURTOL",1.e-15,"Current Tolerance",&elchcontrol);
  IntParameter("GSTATCURVENO",-1,"function number defining the imposed current curve",&elchcontrol);
  IntParameter("GSTATITEMAX",10,"maximum number of iterations for galvanostatic mode",&elchcontrol);
  DoubleParameter("GSTAT_LENGTH_CURRENTPATH",0.0,"average length of the current path",&elchcontrol);
  IntParameter("MAGNETICFIELD_FUNCNO",-1,"function number defining an externally imposed magnetic field",&elchcontrol);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& combustcontrol = list->sublist("COMBUSTION CONTROL",false,
      "control parameters for a combustion problem");

  DoubleParameter("MAXTIME",10.0,"Total simulation time",&combustcontrol);
  IntParameter("NUMSTEP",100,"Total number of timesteps",&combustcontrol);
  DoubleParameter("TIMESTEP",0.1,"Time increment dt",&combustcontrol);
  IntParameter("ITEMAX",1,"Total number of FG iterations",&combustcontrol);
  DoubleParameter("CONVTOL",1e-6,"Tolerance for iteration over fields",&combustcontrol);
  IntParameter("RESTARTEVRY",20,"Increment for writing restart",&combustcontrol);
  IntParameter("UPRES",1,"Increment for writing solution",&combustcontrol);
  setStringToIntegralParameter<int>("TIMEINT","One_Step_Theta","Time Integration Scheme",
      tuple<std::string>(
          "Stationary",
          "One_Step_Theta",
          "Generalized_Alpha"),
          tuple<int>(
              INPAR::FLUID::timeint_stationary,
              INPAR::FLUID::timeint_one_step_theta,
              INPAR::FLUID::timeint_gen_alpha),
              &combustcontrol);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& combustcontrolfluid = combustcontrol.sublist("COMBUSTION FLUID",false,
      "control parameters for the fluid field of a combustion problem");

  setStringToIntegralParameter<int>("COMBUSTTYPE","Premixed_Combustion",
      "Type of combustion problem",
      tuple<std::string>(
          "Premixed_Combustion",
          "Two_Phase_Flow",
          "Two_Phase_Flow_Surf",
          "Two_Phase_Flow_Jumps"),
          tuple<int>(
              INPAR::COMBUST::combusttype_premixedcombustion,
              INPAR::COMBUST::combusttype_twophaseflow,
              INPAR::COMBUST::combusttype_twophaseflow_surf,
              INPAR::COMBUST::combusttype_twophaseflowjump),
              &combustcontrolfluid);

  setStringToIntegralParameter<int>("XFEMINTEGRATION","Cut",
      "Type of integration strategy for intersected elements",
      tuple<std::string>(
          "Cut",
          "Tetrahedra",
          "Hexahedra"),
          tuple<int>(
              INPAR::COMBUST::xfemintegration_cut,
              INPAR::COMBUST::xfemintegration_tetrahedra,
              INPAR::COMBUST::xfemintegration_hexahedra),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("INITIALFIELD","zero_field","Initial field for fluid problem",
      tuple<std::string>(
          "zero_field",
          "field_by_function",
          "disturbed_function_by_function",
          "flame_vortex_interaction",
          "beltrami_flow"),
          tuple<int>(
              INPAR::COMBUST::initfield_zero_field,
              INPAR::COMBUST::initfield_field_by_function,
              INPAR::COMBUST::initfield_disturbed_field_by_function,
              INPAR::COMBUST::initfield_flame_vortex_interaction,
              INPAR::COMBUST::initfield_beltrami_flow),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("NITSCHE_ERROR","nitsche_error_none","To which analyt. solution do we compare?",
      tuple<std::string>(
          "nitsche_error_none",
          "nitsche_error_static_bubble_nxnx1",
          "nitsche_error_static_bubble_nxnxn",
          "nitsche_error_shear",
          "nitsche_error_couette_20x20x1",
          "nitsche_error_straight_bodyforce",
          "nitsche_error_ellipsoid_bubble_2D",
          "nitsche_error_ellipsoid_bubble_3D"),
          tuple<int>(
              INPAR::COMBUST::nitsche_error_none,
              INPAR::COMBUST::nitsche_error_static_bubble_nxnx1,
              INPAR::COMBUST::nitsche_error_static_bubble_nxnxn,
              INPAR::COMBUST::nitsche_error_shear,
              INPAR::COMBUST::nitsche_error_couette_20x20x1,
              INPAR::COMBUST::nitsche_error_straight_bodyforce,
              INPAR::COMBUST::nitsche_error_ellipsoid_bubble_2D,
              INPAR::COMBUST::nitsche_error_ellipsoid_bubble_3D),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("SURFTENSAPPROX","surface_tension_approx_none","Type of surface tension approximation",
      tuple<std::string>(
          "surface_tension_approx_none",
          "surface_tension_approx_fixed_curvature",
          "surface_tension_approx_divgrad",
          "surface_tension_approx_laplacebeltrami",
          "surface_tension_approx_laplacebeltrami_smoothed"),
          tuple<int>(
              INPAR::COMBUST::surface_tension_approx_none,
              INPAR::COMBUST::surface_tension_approx_fixed_curvature,
              INPAR::COMBUST::surface_tension_approx_divgrad,
              INPAR::COMBUST::surface_tension_approx_laplacebeltrami,
              INPAR::COMBUST::surface_tension_approx_laplacebeltrami_smoothed),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("SMOOTHGRADPHI","smooth_grad_phi_meanvalue","Type of smoothing for grad(phi)",
      tuple<std::string>(
          "smooth_grad_phi_none",
          "smooth_grad_phi_meanvalue",
          "smooth_grad_phi_leastsquares_3D",
          "smooth_grad_phi_leastsquares_2Dx",
          "smooth_grad_phi_leastsquares_2Dy",
          "smooth_grad_phi_leastsquares_2Dz"),
          tuple<int>(
              INPAR::COMBUST::smooth_grad_phi_none,
              INPAR::COMBUST::smooth_grad_phi_meanvalue,
              INPAR::COMBUST::smooth_grad_phi_leastsquares_3D,
              INPAR::COMBUST::smooth_grad_phi_leastsquares_2Dx,
              INPAR::COMBUST::smooth_grad_phi_leastsquares_2Dy,
              INPAR::COMBUST::smooth_grad_phi_leastsquares_2Dz),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("VELOCITY_JUMP_TYPE","vel_jump_none","Type of velocity jump",
      tuple<std::string>(
          "vel_jump_none",
          "vel_jump_const",
          "vel_jump_premixed_combustion"),
          tuple<int>(
              INPAR::COMBUST::vel_jump_none,
              INPAR::COMBUST::vel_jump_const,
              INPAR::COMBUST::vel_jump_premixed_combustion),
              &combustcontrolfluid);
  setStringToIntegralParameter<int>("FLUX_JUMP_TYPE","flux_jump_none","Type of flux jump",
      tuple<std::string>(
          "flux_jump_none",
          "flux_jump_const",
          "flux_jump_premixed_combustion",
          "flux_jump_surface_tension"),
          tuple<int>(
              INPAR::COMBUST::flux_jump_none,
              INPAR::COMBUST::flux_jump_const,
              INPAR::COMBUST::flux_jump_premixed_combustion,
              INPAR::COMBUST::flux_jump_surface_tension),
              &combustcontrolfluid);
  IntParameter("INITFUNCNO",-1,"Function for initial field",&combustcontrolfluid);
  DoubleParameter("PHI_MODIFY_TOL",1.0E-10,"We modify GfuncValues near zero",&combustcontrolfluid);
  DoubleParameter("LAMINAR_FLAMESPEED",1.0,"The laminar flamespeed incorporates all chemical kinetics into the problem for now",&combustcontrolfluid);
  DoubleParameter("MARKSTEIN_LENGTH",0.0,"The Markstein length takes flame curvature into account",&combustcontrolfluid);
  DoubleParameter("NITSCHE_VELOCITY",100.0,"Nitsche parameter to stabilize/penalize the velocity jump",&combustcontrolfluid);
  DoubleParameter("NITSCHE_PRESSURE",0.0,"Nitsche parameter to stabilize/penalize the pressure jump",&combustcontrolfluid);
  setStringToIntegralParameter<int>("CONNECTED_INTERFACE","No","Turn refinement strategy for level set function on/off",
                                     yesnotuple,yesnovalue,&combustcontrolfluid);
  setStringToIntegralParameter<int>("SMOOTHED_BOUNDARY_INTEGRATION","No","Turn on/off type of boundary integration",
                                     yesnotuple,yesnovalue,&combustcontrolfluid);
  setStringToIntegralParameter<int>("INITSTATSOL","No","Compute stationary solution as initial solution",
                                     yesnotuple,yesnovalue,&combustcontrolfluid);
  setStringToIntegralParameter<int>("START_VAL_SEMILAGRANGE","No","Turn XFEM-time-integration strategy for nodal start values on/off",
                                     yesnotuple,yesnovalue,&combustcontrolfluid);
  setStringToIntegralParameter<int>("START_VAL_ENRICHMENT","No","Turn XFEM-time-integration strategy for enrichment values on/off",
                                     yesnotuple,yesnovalue,&combustcontrolfluid);


  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& combustcontrolgfunc = combustcontrol.sublist("COMBUSTION GFUNCTION",false,
      "control parameters for the G-function (level set) field of a combustion problem");

  setStringToIntegralParameter<int>("REINITIALIZATION","Signed_Distance_Function",
                               "Type of reinitialization strategy for level set function",
                               tuple<std::string>(
                                 "None",
                                 "Function",
                                 "Signed_Distance_Function"),
                               tuple<int>(
                                 INPAR::COMBUST::reinitaction_none,
                                 INPAR::COMBUST::reinitaction_byfunction,
                                 INPAR::COMBUST::reinitaction_signeddistancefunction),
                               &combustcontrolgfunc);
  IntParameter("REINITFUNCNO",-1,"function number for reinitialization of level set (G-function) field",&combustcontrolgfunc);
  IntParameter("REINITINTERVAL",1,"reinitialization interval",&combustcontrolgfunc);
  BoolParameter("REINITBAND","No","reinitialization only within a band around the interface, or entire domain?",&combustcontrolgfunc);
  DoubleParameter("REINITBANDWIDTH",1.0,"G-function value defining band width for reinitialization",&combustcontrolgfunc);
  setStringToIntegralParameter<int>("REFINEMENT","No","Turn refinement strategy for level set function on/off",
                                     yesnotuple,yesnovalue,&combustcontrolgfunc);
  IntParameter("REFINEMENTLEVEL",-1,"number of refinement level for refinement strategy",&combustcontrolgfunc);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fsidyn = list->sublist(
    "FSI DYNAMIC",false,
    "Fluid Structure Interaction\n"
    "Partitioned FSI solver with various coupling methods"
    );

  Teuchos::Tuple<std::string,20> name;
  Teuchos::Tuple<int,20> label;

  name[ 0] = "basic_sequ_stagg";                   label[ 0] = fsi_basic_sequ_stagg;
  name[ 1] = "iter_stagg_fixed_rel_param";         label[ 1] = fsi_iter_stagg_fixed_rel_param;
  name[ 2] = "iter_stagg_AITKEN_rel_param";        label[ 2] = fsi_iter_stagg_AITKEN_rel_param;
  name[ 3] = "iter_stagg_steep_desc";              label[ 3] = fsi_iter_stagg_steep_desc;
  name[ 4] = "iter_stagg_NLCG";                    label[ 4] = fsi_iter_stagg_NLCG;
  name[ 5] = "iter_stagg_MFNK_FD";                 label[ 5] = fsi_iter_stagg_MFNK_FD;
  name[ 6] = "iter_stagg_MFNK_FSI";                label[ 6] = fsi_iter_stagg_MFNK_FSI;
  name[ 7] = "iter_stagg_MPE";                     label[ 7] = fsi_iter_stagg_MPE;
  name[ 8] = "iter_stagg_RRE";                     label[ 8] = fsi_iter_stagg_RRE;
  name[ 9] = "iter_monolithicfluidsplit";          label[ 9] = fsi_iter_monolithicfluidsplit;
  name[10] = "iter_monolithiclagrange";            label[10] = fsi_iter_monolithiclagrange;
  name[11] = "iter_monolithicstructuresplit";      label[11] = fsi_iter_monolithicstructuresplit;
  name[12] = "iter_lung_monolithicstructuresplit"; label[12] = fsi_iter_lung_monolithicstructuresplit;
  name[13] = "iter_lung_monolithicfluidsplit";     label[13] = fsi_iter_lung_monolithicfluidsplit;
  name[14] = "iter_monolithicxfem";                label[14] = fsi_iter_monolithicxfem;
  name[15] = "pseudo_structure";                   label[15] = fsi_pseudo_structureale;
  name[16] = "iter_constr_monolithicfluidsplit";     label[16] = fsi_iter_constr_monolithicfluidsplit;
  name[17] = "iter_constr_monolithicstructuresplit";     label[17] = fsi_iter_constr_monolithicstructuresplit;
  name[18] = "iter_mortar_monolithicstructuresplit";     label[18] = fsi_iter_mortar_monolithicstructuresplit;
  name[19] = "iter_mortar_monolithicfluidsplit";     label[19] = fsi_iter_mortar_monolithicfluidsplit;

  setStringToIntegralParameter<int>("COUPALGO","iter_stagg_AITKEN_rel_param",
                                    "Iteration Scheme over the fields",
                                    name,
                                    label,
                                    &fsidyn);

  setStringToIntegralParameter<int>(
                               "PARTITIONED","DirichletNeumann",
                               "Coupling strategies for partitioned FSI solvers. Most of the time Dirichlet-Neumann is just right.",
                               tuple<std::string>(
                                 "DirichletNeumann",
                                 "RobinNeumann",
                                 "DirichletRobin",
                                 "RobinRobin",
                                 "DirichletNeumannSlideALE"
                                 ),
                               tuple<int>(
                                 INPAR::FSI::DirichletNeumann,
                                 INPAR::FSI::RobinNeumann,
                                 INPAR::FSI::DirichletRobin,
                                 INPAR::FSI::RobinRobin,
                                 INPAR::FSI::DirichletNeumannSlideale
                                 ),
                               &fsidyn);

  DoubleParameter("ALPHA_F",-1.0,"Robin parameter fluid",&fsidyn);
  DoubleParameter("ALPHA_S",-1.0,"Robin parameter structure",&fsidyn);

  setStringToIntegralParameter<int>("DEBUGOUTPUT","No",
                                    "Output of unconverged interface values during partitioned FSI iteration.\n"
                                    "There will be a new control file for each time step.\n"
                                    "This might be helpful to understand the coupling iteration.",
                                    tuple<std::string>(
                                      "No",
                                      "Yes",
                                      "Interface",
                                      "Preconditioner",
                                      "All"
                                      ),
                                    tuple<int>(
                                      0,
                                      1,
                                      1,
                                      2,
                                      256
                                      ),
                                    &fsidyn);

  setStringToIntegralParameter<int>("PREDICTOR","d(n)+dt*v(n)+0.5*dt^2*a(n)",
                               "Predictor for interface displacements",
                               tuple<std::string>(
                                 "d(n)",
                                 "d(n)+dt*(1.5*v(n)-0.5*v(n-1))",
                                 "d(n)+dt*v(n)",
                                 "d(n)+dt*v(n)+0.5*dt^2*a(n)"
                                 ),
                               tuple<int>(1,2,3,4),
                               &fsidyn);

  setStringToIntegralParameter<int>("CONVCRIT","||g(i)||:sqrt(neq)",
                               "Convergence criterium for iteration over fields (unused)",
                               tuple<std::string>(
                                 "||g(i)||:sqrt(neq)",
                                 "||g(i)||:||g(0)||"
                                 ),
                               tuple<int>(1,2),
                               &fsidyn);

  setStringToIntegralParameter<int>("COUPVARIABLE","Displacement",
                               "Coupling variable at the interface",
                               tuple<std::string>("Displacement","Force"),
                               tuple<int>(0,1),
                               &fsidyn);

  setStringToIntegralParameter<int>("ENERGYCHECK","No",
                               "Energy check for iteration over fields",
                               yesnotuple,yesnovalue,&fsidyn);

  setStringToIntegralParameter<int>("IALE","Pseudo_Structure",
                               "Treatment of ALE-field (outdated)",
                               tuple<std::string>(
                                 "Pseudo_Structure"
                                 ),
                               tuple<int>(1),
                               &fsidyn);

  setStringToIntegralParameter<int>("COUPMETHOD","conforming",
                               "Coupling Method Mortar (mtr) or conforming nodes at interface",
                               tuple<std::string>(
                                 "MTR",
                                 "Mtr",
                                 "mtr",
                                 "conforming"
                                 ),
                               tuple<int>(0,0,0,1),
                               &fsidyn);

  setStringToIntegralParameter<int>("COUPFORCE","nodeforce",
                               "Coupling force. Unused. We always couple with nodal forces.",
                               tuple<std::string>(
                                 "none",
                                 "stress",
                                 "nodeforce"
                                 ),
                               tuple<int>(
                                 FSI_DYNAMIC::cf_none,
                                 FSI_DYNAMIC::cf_stress,
                                 FSI_DYNAMIC::cf_nodeforce),
                               &fsidyn);

  setStringToIntegralParameter<int>("SECONDORDER","No",
                               "Second order coupling at the interface.",
                               yesnotuple,yesnovalue,&fsidyn);

  setStringToIntegralParameter<int>("SHAPEDERIVATIVES","No",
                               "Include linearization with respect to mesh movement in Navier Stokes equation.\n"
                               "Supported in monolithic FSI for now.",
                               yesnotuple,yesnovalue,&fsidyn);

  IntParameter("PRECONDREUSE",
               10,
               "Number of preconditioner reused in monolithic FSI",
               &fsidyn);

  setStringToIntegralParameter<int>(
                               "LINEARBLOCKSOLVER","PreconditionedKrylov",
                               "Linear solver algorithm for monolithic block system in monolithic FSI.\n"
                               "Most of the time preconditioned Krylov is the right thing to choose. But there are\n"
                               "block Gauss-Seidel methods as well.",
                               tuple<std::string>(
                                 "PreconditionedKrylov",
                                 "FSIAMG",
                                 "PartitionedAitken",
                                 "PartitionedVectorExtrapolation",
                                 "PartitionedJacobianFreeNewtonKrylov",
                                 "BGSAitken",
                                 "BGSVectorExtrapolation",
                                 "BGSJacobianFreeNewtonKrylov"
                                 ),
                               tuple<int>(
                                 INPAR::FSI::PreconditionedKrylov,
                                 INPAR::FSI::FSIAMG,
                                 INPAR::FSI::PartitionedAitken,
                                 INPAR::FSI::PartitionedVectorExtrapolation,
                                 INPAR::FSI::PartitionedJacobianFreeNewtonKrylov,
                                 INPAR::FSI::BGSAitken,
                                 INPAR::FSI::BGSVectorExtrapolation,
                                 INPAR::FSI::BGSJacobianFreeNewtonKrylov
                                 ),
                               &fsidyn);

  setStringToIntegralParameter<int>("FSIAMGANALYZE","No",
                               "run analysis on fsiamg multigrid scheme\n"
                               "Supported in monolithic FSI for now.",
                               yesnotuple,yesnovalue,&fsidyn);

  IntParameter("ITECHAPP",1,"unused",&fsidyn);
  IntParameter("ICHMAX",1,"unused",&fsidyn);
  IntParameter("ISDMAX",1,"not used up to now",&fsidyn);
  IntParameter("NUMSTEP",200,"Total number of Timesteps",&fsidyn);
  IntParameter("ITEMAX",100,"Maximum number of iterations over fields",&fsidyn);
  IntParameter("UPPSS",1,"Increment for visualization (unused)",&fsidyn);
  IntParameter("UPRES",1,"Increment for writing solution",&fsidyn);
  IntParameter("RESTARTEVRY",1,"Increment for writing restart",&fsidyn);

  DoubleParameter("TIMESTEP",0.1,"Time increment dt",&fsidyn);
  DoubleParameter("MAXTIME",1000.0,"Total simulation time",&fsidyn);
  DoubleParameter("TOLENCHECK",1e-6,"Tolerance for energy check",&fsidyn);
  DoubleParameter("RELAX",1.0,"fixed relaxation parameter",&fsidyn);
  DoubleParameter("CONVTOL",1e-6,"Tolerance for iteration over fields",&fsidyn);
  DoubleParameter("MAXOMEGA",0.0,"largest omega allowed for Aitken relaxation (0.0 means no constraint)",&fsidyn);

  DoubleParameter("BASETOL",1e-3,
                  "Basic tolerance for adaptive convergence check in monolithic FSI.\n"
                  "This tolerance will be used for the linear solve of the FSI block system.\n"
                  "The linear convergence test will always use the relative residual norm (AZ_r0).\n"
                  "Not to be confused with the Newton tolerance (CONVTOL) that applies\n"
                  "to the nonlinear convergence test.",
                  &fsidyn);

  DoubleParameter("ADAPTIVEDIST",0.0,
                  "Required distance for adaptive convergence check in Newton-type FSI.\n"
                  "This is the improvement we want to achieve in the linear extrapolation of the\n"
                  "adaptive convergence check. Set to zero to avoid the adaptive check altogether.",
                  &fsidyn);

  // monolithic preconditioner parameter

  setNumericStringParameter("STRUCTPCOMEGA","1.0 1.0 1.0 1.0",
                  "Relaxation factor for Richardson iteration on structural block in MFSI block preconditioner",
                  &fsidyn);
  setNumericStringParameter("STRUCTPCITER","1 1 1 1",
               "Number of Richardson iterations on structural block in MFSI block preconditioner",
               &fsidyn);
  setNumericStringParameter("FLUIDPCOMEGA","1.0 1.0 1.0 1.0",
                  "Relaxation factor for Richardson iteration on fluid block in MFSI block preconditioner",
                  &fsidyn);
  setNumericStringParameter("FLUIDPCITER","1 1 1 1",
               "Number of Richardson iterations on fluid block in MFSI block preconditioner",
               &fsidyn);
  setNumericStringParameter("ALEPCOMEGA","1.0 1.0 1.0 1.0",
                  "Relaxation factor for Richardson iteration on ale block in MFSI block preconditioner",
                  &fsidyn);
  setNumericStringParameter("ALEPCITER","1 1 1 1",
               "Number of Richardson iterations on ale block in MFSI block preconditioner",
               &fsidyn);

  setNumericStringParameter("PCOMEGA","1.0 1.0 1.0",
                            "Relaxation factor for Richardson iteration on whole MFSI block preconditioner",
                            &fsidyn);
  setNumericStringParameter("PCITER","1 1 1",
                            "Number of Richardson iterations on whole MFSI block preconditioner",
                            &fsidyn);

  setNumericStringParameter("BLOCKSMOOTHER","BGS BGS BGS",
                            "Type of block smoother, can be BGS or Schur",
                            &fsidyn);

  setNumericStringParameter("SCHUROMEGA","0.001 0.01 0.1",
                            "Damping factor for Schur complement construction",
                            &fsidyn);

  //DoubleParameter("PCOMEGA",1.,
  //                "Relaxation factor for Richardson iteration on whole MFSI block preconditioner",
  //                &fsidyn);
  //IntParameter("PCITER",1,
  //             "Number of Richardson iterations on whole MFSI block preconditioner",
  //             &fsidyn);

  setStringToIntegralParameter<int>("INFNORMSCALING","Yes","Scale Blocks in Mono-FSI with row infnorm?",
                                     yesnotuple,yesnovalue,&fsidyn);
  setStringToIntegralParameter<int>("SYMMETRICPRECOND","No","Symmetric block GS preconditioner in monolithic FSI or ordinary GS",
                                     yesnotuple,yesnovalue,&fsidyn);

  setStringToIntegralParameter<int>("SLIDEALEPROJ","None",
                                 "Projection method to use for sliding FSI.",
                                 tuple<std::string>("None","Curr","Ref","RotZ","RotZSphere"),
                                 tuple<int>(
                                     INPAR::FSI::ALEprojection_none,
                                     INPAR::FSI::ALEprojection_curr,
                                     INPAR::FSI::ALEprojection_ref,
                                     INPAR::FSI::ALEprojection_rot_z,
                                     INPAR::FSI::ALEprojection_rot_zsphere),
                                 &fsidyn);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& constrfsi = fsidyn.sublist("CONSTRAINT",false,"");

  setStringToIntegralParameter<int> ("PRECONDITIONER","Simple","preconditioner to use",
      tuple<std::string>("Simple","Simplec"),tuple<int>(INPAR::FSI::Simple,INPAR::FSI::Simplec),&constrfsi);
  IntParameter("SIMPLEITER",2,"Number of iterations for simple pc",&constrfsi);
  DoubleParameter("ALPHA",0.8,"alpha parameter for simple pc",&constrfsi);


  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& search_tree = list->sublist("SEARCH TREE",false,"");

  setStringToIntegralParameter<int>("TREE_TYPE","notree","set tree type",
                                   tuple<std::string>("notree","octree3d","quadtree3d","quadtree2d"),
                                   tuple<int>(
                                     INPAR::GEO::Notree,
                                     INPAR::GEO::Octree3D,
                                     INPAR::GEO::Quadtree3D,
                                     INPAR::GEO::Quadtree2D),
                                     &search_tree);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& xfem_general = list->sublist("XFEM GENERAL",false,"");

  setStringToIntegralParameter<int>("GMSH_DEBUG_OUT","Yes","Do you want to write extended Gmsh output for each timestep?",
                               yesnotuple,yesnovalue,&xfem_general);
  setStringToIntegralParameter<int>("GMSH_DEBUG_OUT_SCREEN","No","Do you want to be informed, if Gmsh output is written?",
                                 yesnotuple,yesnovalue,&xfem_general);
  setStringToIntegralParameter<int>("DLM_CONDENSATION","Yes","Do you want to condense the distributed Lagrange multiplier?",
                                 yesnotuple,yesnovalue,&xfem_general);
  setStringToIntegralParameter<int>("INCOMP_PROJECTION","No","Do you want to project the old velocity to an incompressible velocity field?",
                               yesnotuple,yesnovalue,&xfem_general);
  setStringToIntegralParameter<int>("CONDEST","No","Do you want to estimate the condition number? It is somewhat costly.",
                                   yesnotuple,yesnovalue,&xfem_general);
  DoubleParameter("volumeRatioLimit",1.0e-2,"don't enrich nodes of elements, when less than this fraction of the element is on one side of the interface",&xfem_general);
  DoubleParameter("boundaryRatioLimit",1.0e-4,"don't enrich element, when less than this area fraction is within this element",&xfem_general);

  setStringToIntegralParameter<int>("EMBEDDED_BOUNDARY","BoundaryTypeSigma","how to treat the interface",
                               tuple<std::string>("BoundaryTypeSigma","BoundaryTypeTauPressure"),
                               tuple<int>(
                                   INPAR::XFEM::BoundaryTypeSigma,
                                   INPAR::XFEM::BoundaryTypeTauPressure),
                               &xfem_general);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fluidsolver = list->sublist("FLUID SOLVER",false,"");
  SetValidSolverParameters(fluidsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& fluidpsolver = list->sublist("FLUID PRESSURE SOLVER",false,"pressure solver parameters for SIMPLE preconditioning");
  SetValidSolverParameters(fluidpsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& xfluidprojsolver = list->sublist("XFLUID PROJECTION SOLVER",false,"");
  SetValidSolverParameters(xfluidprojsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& structsolver = list->sublist("STRUCT SOLVER",false,"");
  SetValidSolverParameters(structsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& alesolver = list->sublist("ALE SOLVER",false,"");
  SetValidSolverParameters(alesolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& thermalsolver = list->sublist("THERMAL SOLVER",false,"linear solver for thermal problems");
  SetValidSolverParameters(thermalsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& scatrasolver = list->sublist("SCALAR TRANSPORT SOLVER",false,"solver parameters for scalar transport problems");
  SetValidSolverParameters(scatrasolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& scatrapotsolver = list->sublist("SCALAR TRANSPORT ELECTRIC POTENTIAL SOLVER",false,"solver parameters for block-preconditioning");
  SetValidSolverParameters(scatrapotsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& coupscatrasolver = list->sublist("COUPLED SCALAR TRANSPORT SOLVER",false,"solver parameters for block-preconditioning");
  SetValidSolverParameters(coupscatrasolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& artnetsolver = list->sublist("ARTERY NETWORK SOLVER",false,"");
  SetValidSolverParameters(artnetsolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& redairwaysolver = list->sublist("REDUCED DIMENSIONAL AIRWAYS SOLVER",false,"");
  SetValidSolverParameters(redairwaysolver);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& precond1 = list->sublist("BGS PRECONDITIONER BLOCK 1",false,"");
  SetValidSolverParameters(precond1);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& precond2 = list->sublist("BGS PRECONDITIONER BLOCK 2",false,"");
  SetValidSolverParameters(precond2);

  // TSI monolithic solver section
  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& tsimonsolver = list->sublist("TSI MONOLITHIC SOLVER",false,"solver parameters for monoltihic tsi");
  SetValidSolverParameters(tsimonsolver);


  return list;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::SetValidSolverParameters(Teuchos::ParameterList& list)
{
  using Teuchos::tuple;
  using Teuchos::setStringToIntegralParameter;

  setStringToIntegralParameter<int>(
    "SOLVER", "UMFPACK",
    "The solver to attack the system of linear equations arising of FE approach with.",
    tuple<std::string>("Amesos_KLU_sym",
                       "Amesos_KLU_nonsym",
                       "Superlu",
                       "vm3",
                       "Aztec_MSR",
                       "LAPACK_sym",
                       "LAPACK_nonsym",
                       "UMFPACK",
                       "Belos"),
    tuple<int>(INPAR::SOLVER::amesos_klu_sym,
                                     INPAR::SOLVER::amesos_klu_nonsym,
                                     INPAR::SOLVER::superlu,
                                     INPAR::SOLVER::vm3,
                                     INPAR::SOLVER::aztec_msr,
                                     INPAR::SOLVER::lapack_sym,
                                     INPAR::SOLVER::lapack_nonsym,
                                     INPAR::SOLVER::umfpack,
                                     INPAR::SOLVER::belos),
    &list
    );

  setStringToIntegralParameter<int>(
    "AZSOLVE", "GMRES",
    "Type of linear solver algorithm to use.",
    tuple<std::string>("CG",
                       "GMRES",
                       "GMRESR",
                       "CGS",
                       "TFQMR",
                       "BiCGSTAB",
                       "LU"),
    tuple<int>(INPAR::SOLVER::azsolv_CG,
               INPAR::SOLVER::azsolv_GMRES,
               INPAR::SOLVER::azsolv_GMRESR,
               INPAR::SOLVER::azsolv_CGS,
               INPAR::SOLVER::azsolv_TFQMR,
               INPAR::SOLVER::azsolv_BiCGSTAB,
               INPAR::SOLVER::azsolv_LU),
    &list
    );

  {
    // this one is longer than 15 and the tuple<> function does not support this,
    // so build the Tuple class directly (which can be any size)
    Teuchos::Tuple<std::string,21> name;
    Teuchos::Tuple<int,21>  number;

    name[0] = "none";                         number[0] = INPAR::SOLVER::azprec_none;
    name[1] = "ILU";                          number[1] = INPAR::SOLVER::azprec_ILU;
    name[2] = "ILUT";                         number[2] = INPAR::SOLVER::azprec_ILUT;
    name[3] = "Jacobi";                       number[3] = INPAR::SOLVER::azprec_Jacobi;
    name[4] = "SymmGaussSeidel";              number[4] = INPAR::SOLVER::azprec_SymmGaussSeidel;
    name[5] = "Least_Squares";                number[5] = INPAR::SOLVER::azprec_Least_Squares;
    name[6] = "Neumann";                      number[6] = INPAR::SOLVER::azprec_Neumann;
    name[7] = "ICC";                          number[7] = INPAR::SOLVER::azprec_ICC;
    name[8] = "LU";                           number[8] = INPAR::SOLVER::azprec_LU;
    name[9] = "RILU";                         number[9] = INPAR::SOLVER::azprec_RILU;
    name[10] = "BILU";                        number[10] = INPAR::SOLVER::azprec_BILU;
    name[11] = "ML";                          number[11] = INPAR::SOLVER::azprec_ML;
    name[12] = "MLFLUID";                     number[12] = INPAR::SOLVER::azprec_MLfluid;
    name[13] = "MLFLUID2";                    number[13] = INPAR::SOLVER::azprec_MLfluid2;
    name[14] = "MLAPI";                       number[14] = INPAR::SOLVER::azprec_MLAPI;
    name[15] = "GaussSeidel";                 number[15] = INPAR::SOLVER::azprec_GaussSeidel;
    name[16] = "DownwindGaussSeidel";         number[16] = INPAR::SOLVER::azprec_DownwindGaussSeidel;
    name[17] = "AMG(Braess-Sarazin)";         number[17] = INPAR::SOLVER::azprec_AMGBS;
    name[18] = "AMG";                         number[18] = INPAR::SOLVER::azprec_AMG;
    name[19] = "BGS2x2";                      number[19] = INPAR::SOLVER::azprec_BGS2x2;
    name[20] = "Teko";                        number[20] = INPAR::SOLVER::azprec_Teko;

    setStringToIntegralParameter<int>(
      "AZPREC", "ILU",
      "Type of internal preconditioner to use.\n"
      "Note! this preconditioner will only be used if the input operator\n"
      "supports the Epetra_RowMatrix interface and the client does not pass\n"
      "in an external preconditioner!",
      name,
      number,
      &list
      );
  }

  IntParameter(
    "IFPACKOVERLAP", 0,
    "The amount of overlap used for the ifpack \"ilu\" and \"ilut\" preconditioners.",
    &list);

  IntParameter(
    "IFPACKGFILL", 0,
    "The amount of fill allowed for the internal \"ilu\" preconditioner.",
    &list);

  DoubleParameter(
    "IFPACKFILL", 1.0,
    "The amount of fill allowed for an internal \"ilut\" preconditioner.",
    &list);

  setStringToIntegralParameter<int>(
    "IFPACKCOMBINE","Add","Combine mode for Ifpack Additive Schwarz",
    tuple<std::string>("Add","Insert","Zero"),
    tuple<int>(0,1,2),
    &list);

  DoubleParameter(
    "AZDROP", 0.0,
    "The tolerance below which an entry from the factors of an internal \"ilut\"\n"
    "preconditioner will be dropped.",
    &list
    );
//   IntParameter(
//     Steps_name, 3,
//     "Number of steps taken for the \"Jacobi\" or the \"Symmetric Gauss-Seidel\"\n"
//     "internal preconditioners for each preconditioner application.",
//     &list
//     );
  IntParameter(
    "AZPOLY", 3,
    "The order for of the polynomials used for the \"Polynomial\" and\n"
    "\"Least-squares Polynomial\" internal preconditioners.",
    &list
    );
//   setStringToIntegralParameter(
//     RCMReordering_name, "Disabled",
//     "Determines if RCM reordering is used with the internal\n"
//     "\"ilu\" or \"ilut\" preconditioners.",
//     tuple<std::string>("Enabled","Disabled"),
//     tuple<int>(1,0),
//     &list
//     );
//   setStringToIntegralParameter(
//     Orthogonalization_name, "Classical",
//     "The type of orthogonalization to use with the \"GMRES\" solver.",
//     tuple<std::string>("Classical","Modified"),
//     tuple<int>(AZ_classic,AZ_modified),
//     &list
//     );
  IntParameter(
    "AZSUB", 300,
    "The maximum size of the Krylov subspace used with \"GMRES\" before\n"
    "a restart is performed.",
    &list
    );
  setStringToIntegralParameter<int>(
    "AZCONV", "AZ_r0", // Same as "rhs" when x=0
    "The convergence test to use for terminating the iterative solver.",
    tuple<std::string>(
      "AZ_r0",
      "AZ_rhs",
      "AZ_Anorm",
      "AZ_noscaled",
      "AZ_sol",
      "AZ_weighted",
      "AZ_expected_values",
      "AZTECOO_conv_test",
      "AZ_inf_noscaled"
      ),
    tuple<int>(
      AZ_r0,
      AZ_rhs,
      AZ_Anorm,
      AZ_noscaled,
      AZ_sol,
      AZ_weighted,
      AZ_expected_values,
      AZTECOO_conv_test,
      AZ_inf_noscaled
      ),
    &list
    );
//   DoubleParameter(
//     IllConditioningThreshold_name, 1e+11,
//     "The threshold tolerance above which a system is considered\n"
//     "ill conditioned.",
//     &list
//     );
  IntParameter(
    "AZOUTPUT", 0, // By default, no output from Aztec!
    "The number of iterations between each output of the solver's progress.",
    &list
    );

  IntParameter("AZREUSE", 0, "how often to recompute some preconditioners", &list);
  IntParameter("AZITER", 1000, "max iterations", &list);
  IntParameter("AZGRAPH", 0, "unused", &list);
  IntParameter("AZBDIAG", 0, "", &list);

  DoubleParameter("AZTOL", 1e-8, "tolerance in (un)scaled residual", &list);
  DoubleParameter("AZOMEGA", 0.0, "damping for GaussSeidel and jacobi type methods", &list);
  DoubleParameter("DWINDTAU",1.5,"threshold tau for downwinding", &list);

  setStringToIntegralParameter<int>(
    "AZSCAL","none","scaling of the system",
    tuple<std::string>("none","sym","infnorm"),
    tuple<int>(0,1,2),
    &list);

  // parameters of ML preconditioner

  IntParameter("ML_PRINT",0,
               "ML print-out level (0-10)",&list);
  IntParameter("ML_MAXCOARSESIZE",5000,
               "ML stop coarsening when coarse ndof smaller then this",&list);
  IntParameter("ML_MAXLEVEL",5,
               "ML max number of levels",&list);
  IntParameter("ML_AGG_SIZE",27,
               "objective size of an aggregate with METIS/VBMETIS, 2D: 9, 3D: 27",&list);

  DoubleParameter("ML_DAMPFINE",1.,"damping fine grid",&list);
  DoubleParameter("ML_DAMPMED",1.,"damping med grids",&list);
  DoubleParameter("ML_DAMPCOARSE",1.,"damping coarse grid",&list);
  DoubleParameter("ML_PROLONG_SMO",0.,"damping factor for prolongator smoother (usually 1.33 or 0.0)",&list);
  DoubleParameter("ML_PROLONG_THRES",0.,"threshold for prolongator smoother/aggregation",&list);

  setNumericStringParameter("ML_SMOTIMES","1 1 1 1 1",
                            "no. smoothing steps or polynomial order on each level (at least ML_MAXLEVEL numbers)",&list);

  setStringToIntegralParameter<int>(
    "ML_COARSEN","UC","",
    tuple<std::string>("UC","METIS","VBMETIS","MIS"),
    tuple<int>(0,1,2,3),
    &list);

  setStringToIntegralParameter<int>(
    "ML_SMOOTHERFINE","ILU","",
    tuple<std::string>("SGS","Jacobi","Chebychev","MLS","ILU","KLU","Superlu","GS","DGS","Umfpack"),
    tuple<int>(0,1,2,3,4,5,6,7,8,9),
    &list);

  setStringToIntegralParameter<int>(
    "ML_SMOOTHERMED","ILU","",
    tuple<std::string>("SGS","Jacobi","Chebychev","MLS","ILU","KLU","Superlu","GS","DGS","Umfpack"),
    tuple<int>(0,1,2,3,4,5,6,7,8,9),
    &list);

  setStringToIntegralParameter<int>(
    "ML_SMOOTHERCOARSE","Umfpack","",
    tuple<std::string>("SGS","Jacobi","Chebychev","MLS","ILU","KLU","Superlu","GS","DGS","Umfpack"),
    tuple<int>(0,1,2,3,4,5,6,7,8,9),
    &list);

  // parameters for AMG(BS)
  setNumericStringParameter("AMGBS_BS_DAMPING","1.3 1.3 1.3",
                            "Relaxation factor for Braess-Sarazin smoother within AMGBS method",
                            &list);

  setNumericStringParameter("AMGBS_BS_PCSWEEPS","2 2 2",
                            "number of jacobi/sgs sweeps for smoothing/solving pressure correction equation within Braess-Sarazin. only necessary for jacobi/gauss seidel",
                            &list);

  setNumericStringParameter("AMGBS_BS_PCDAMPING","1.0 1.0 1.0",
                              "jacobi damping factors for smoothing/solving pressure correction equation within Braess-Sarazin. only necessary for jacobi/gauss seidel",
                              &list);


  setStringToIntegralParameter<int>(
    "AMGBS_PSMOOTHER_VEL","PA-AMG","Prolongation/Restriction smoothing strategy (velocity part in AMGBS preconditioner)",
    tuple<std::string>("PA-AMG","SA-AMG","PG-AMG","PG2-AMG"),
    tuple<int>(INPAR::SOLVER::PA_AMG,INPAR::SOLVER::SA_AMG,INPAR::SOLVER::PG_AMG,INPAR::SOLVER::PG2_AMG),
    &list);
  setStringToIntegralParameter<int>(
    "AMGBS_PSMOOTHER_PRE","PA-AMG","Prolongation/Restriction smoothing strategy (pressure part in AMGBS preconditioner)",
    tuple<std::string>("PA-AMG","SA-AMG","PG-AMG","PG2-AMG"),
    tuple<int>(INPAR::SOLVER::PA_AMG,INPAR::SOLVER::SA_AMG,INPAR::SOLVER::PG_AMG,INPAR::SOLVER::PG2_AMG),
    &list);

  setStringToIntegralParameter<int>(
    "AMGBS_BS_PCCOARSE","Umfpack","approximation algorithm for solving pressure correction equation (coarsest level)",
    tuple<std::string>("Umfpack","KLU","ILU","Jacobi","Gauss-Seidel","symmetric Gauss-Seidel","Jacobi stand-alone","Gauss-Seidel stand-alone","symmetric Gauss-Seidel stand-alone"),
    tuple<int>(0,1,2,3,4,5,6,7,8),
    &list);

  setStringToIntegralParameter<int>(
    "AMGBS_BS_PCMEDIUM","Umfpack","approximation algorithm for solving pressure correction equation (medium level)",
    tuple<std::string>("Umfpack","KLU","ILU","Jacobi","Gauss-Seidel","symmetric Gauss-Seidel","Jacobi stand-alone","Gauss-Seidel stand-alone","symmetric Gauss-Seidel stand-alone"),
    tuple<int>(0,1,2,3,4,5,6,7,8),
    &list);

  setStringToIntegralParameter<int>(
    "AMGBS_BS_PCFINE","Umfpack","approximation algorithm for solving pressure correction equation (finest level)",
    tuple<std::string>("Umfpack","KLU","ILU","Jacobi","Gauss-Seidel","symmetric Gauss-Seidel","Jacobi stand-alone","Gauss-Seidel stand-alone","symmetric Gauss-Seidel stand-alone"),
    tuple<int>(0,1,2,3,4,5,6,7,8),
    &list);

  // unused
  setStringToIntegralParameter<int>("PARTITION","Cut_Elements","unused",
                               tuple<std::string>("Cut_Elements"),
                               tuple<int>(0),
                               &list);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::SetValidTimeAdaptivityParameters(Teuchos::ParameterList& list)
{
  using Teuchos::tuple;
  using Teuchos::setStringToIntegralParameter;

  setStringToIntegralParameter<int>(
    "KIND","None","Method for time step size adapivity",
    tuple<std::string>(
      "None",
      "ZienkiewiczXie",
      "AdamsBashforth2"),
    tuple<int>(
      INPAR::STR::timada_kind_none,
      INPAR::STR::timada_kind_zienxie,
      INPAR::STR::timada_kind_ab2),
    &list);

  DoubleParameter("OUTSYSPERIOD", 0.0, "Write system vectors (displacements, velocities, etc) every given period of time", &list);
  DoubleParameter("OUTSTRPERIOD", 0.0, "Write stress/strain every given period of time", &list);
  DoubleParameter("OUTENEPERIOD", 0.0, "Write energy every given period of time", &list);
  DoubleParameter("OUTRESTPERIOD", 0.0, "Write restart data every given period of time", &list);
  IntParameter("OUTSIZEEVERY", 0, "Write step size every given time step", &list);

  DoubleParameter("STEPSIZEMAX", 0.0, "Limit maximally permitted time step size (>0)", &list);
  DoubleParameter("STEPSIZEMIN", 0.0, "Limit minimally allowed time step size (>0)", &list);
  DoubleParameter("SIZERATIOMAX", 0.0, "Limit maximally permitted change of time step size compared to previous size, important for multi-step schemes (>0)", &list);
  DoubleParameter("SIZERATIOMIN", 0.0, "Limit minimally permitted change of time step size compared to previous size, important for multi-step schemes (>0)", &list);
  DoubleParameter("SIZERATIOSCALE", 0.9, "This is a safety factor to scale theretical optimal step size, should be lower than 1 and must be larger than 0", &list);

  setStringToIntegralParameter<int>(
    "LOCERRNORM", "Vague", "Vector norm to treat error vector with",
    tuple<std::string>(
      "Vague",
      "L1",
      "L2",
      "Rms",
      "Inf"),
    tuple<int>(
      INPAR::STR::norm_vague,
      INPAR::STR::norm_l1,
      INPAR::STR::norm_l2,
      INPAR::STR::norm_rms,
      INPAR::STR::norm_inf),
    &list);

  DoubleParameter("LOCERRTOL", 0.0, "Target local error tolerance (>0)", &list);
  IntParameter("ADAPTSTEPMAX", 0, "Limit maximally allowed step size reduction attempts (>0)", &list);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::SetValidNoxParameters(Teuchos::ParameterList& list)
{
  SetPrintEqualSign(list,true);

  {
    Teuchos::Array<std::string> st = Teuchos::tuple<std::string>(
      "Line Search Based",
      "Trust Region Based",
      "Inexact Trust Region Based",
      "Tensor Based");
    Teuchos::setStringToIntegralParameter<int>(
      "Nonlinear Solver","Line Search Based","",
      st,Teuchos::tuple<int>( 0, 1, 2, 3 ),
      &list);
  }

  // sub-list direction
  Teuchos::ParameterList& direction = list.sublist("Direction",false,"");
  SetPrintEqualSign(direction,true);

  {
    Teuchos::Array<std::string> st = Teuchos::tuple<std::string>(
      "Newton",
      "Steepest Descent",
      "NonlinearCG",
      "Broyden");
    Teuchos::setStringToIntegralParameter<int>(
      "Method","Newton","",
      st,Teuchos::tuple<int>( 0, 1, 2, 3 ),
      &direction);
  }

  // sub-sub-list "Newton"
  Teuchos::ParameterList& newton = direction.sublist("Newton",false,"");
  SetPrintEqualSign(newton,true);

  {
    Teuchos::Array<std::string> forcingtermmethod = Teuchos::tuple<std::string>(
      "Constant",
      "Type 1",
      "Type 2");
    Teuchos::setStringToIntegralParameter<int>(
      "Forcing Term Method","Constant","",
      forcingtermmethod,Teuchos::tuple<int>( 0, 1, 2 ),
      &newton);
    DoubleParameter("Forcing Term Initial Tolerance",0.1,"initial linear solver tolerance",&newton);
    DoubleParameter("Forcing Term Minimum Tolerance",1.0e-6,"",&newton);
    DoubleParameter("Forcing Term Maximum Tolerance",0.01,"",&newton);
    DoubleParameter("Forcing Term Alpha",1.5,"used only by \"Type 2\"",&newton);
    DoubleParameter("Forcing Term Gamma",0.9,"used only by \"Type 2\"",&newton);
    BoolParameter("Rescue Bad Newton Solver","Yes","If set to true, we will use the computed direction even if the linear solve does not achieve the tolerance specified by the forcing term",&newton);
  }

  // sub-sub-list "Steepest Descent"
  Teuchos::ParameterList& steepestdescent = direction.sublist("Steepest Descent",false,"");
  SetPrintEqualSign(steepestdescent,true);

  {
    Teuchos::Array<std::string> scalingtype = Teuchos::tuple<std::string>(
      "2-Norm",
      "Quadratic Model Min",
      "F 2-Norm",
      "None");
    Teuchos::setStringToIntegralParameter<int>(
      "Scaling Type","None","",
      scalingtype,Teuchos::tuple<int>( 0, 1, 2, 3 ),
      &steepestdescent);
  }

  // sub-list "Line Search"
  Teuchos::ParameterList& linesearch = list.sublist("Line Search",false,"");
  SetPrintEqualSign(linesearch,true);

  {
    Teuchos::Array<std::string> method = Teuchos::tuple<std::string>(
      "Full Step",
      "Backtrack" ,
      "Polynomial",
      "More'-Thuente",
      "User Defined");
    Teuchos::setStringToIntegralParameter<int>(
      "Method","Full Step","",
      method,Teuchos::tuple<int>( 0, 1, 2, 3, 4 ),
      &linesearch);
  }

  // sub-sub-list "Full Step"
  Teuchos::ParameterList& fullstep = linesearch.sublist("Full Step",false,"");
  SetPrintEqualSign(fullstep,true);

  {
    DoubleParameter("Full Step",1.0,"length of a full step",&fullstep);
  }

  // sub-sub-list "Backtrack"
  Teuchos::ParameterList& backtrack = linesearch.sublist("Backtrack",false,"");
  SetPrintEqualSign(backtrack,true);

  {
    DoubleParameter("Default Step",1.0,"starting step length",&backtrack);
    DoubleParameter("Minimum Step",1.0e-12,"minimum acceptable step length",&backtrack);
    DoubleParameter("Recovery Step",1.0,"step to take when the line search fails (defaults to value for \"Default Step\")",&backtrack);
    IntParameter("Max Iters",50,"maximum number of iterations (i.e., RHS computations)",&backtrack);
    DoubleParameter("Reduction Factor",0.5,"A multiplier between zero and one that reduces the step size between line search iterations",&backtrack);
  }

  // sub-sub-list "Polynomial"
  Teuchos::ParameterList& polynomial = linesearch.sublist("Polynomial",false,"");
  SetPrintEqualSign(polynomial,true);

  {
    DoubleParameter("Default Step",1.0,"Starting step length",&polynomial);
    IntParameter("Max Iters",100,"Maximum number of line search iterations. The search fails if the number of iterations exceeds this value",&polynomial);
    DoubleParameter("Minimum Step",1.0e-12,"Minimum acceptable step length. The search fails if the computed $lambda_k$ is less than this value",&polynomial);
    Teuchos::Array<std::string> recoverysteptype = Teuchos::tuple<std::string>(
      "Constant",
      "Last Computed Step");
    Teuchos::setStringToIntegralParameter<int>(
      "Recovery Step Type","Constant","Determines the step size to take when the line search fails",
      recoverysteptype,Teuchos::tuple<int>( 0, 1 ),
      &polynomial);
    DoubleParameter("Recovery Step",1.0,"The value of the step to take when the line search fails. Only used if the \"Recovery Step Type\" is set to \"Constant\"",&polynomial);
    Teuchos::Array<std::string> interpolationtype = Teuchos::tuple<std::string>(
      "Quadratic",
      "Quadratic3",
      "Cubic");
    Teuchos::setStringToIntegralParameter<int>(
      "Interpolation Type","Cubic","Type of interpolation that should be used",
      interpolationtype,Teuchos::tuple<int>( 0, 1, 2 ),
      &polynomial);
    DoubleParameter("Min Bounds Factor",0.1,"Choice for $gamma_{min}$, i.e., the factor that limits the minimum size of the new step based on the previous step",&polynomial);
    DoubleParameter("Max Bounds Factor",0.5,"Choice for $gamma_{max}$, i.e., the factor that limits the maximum size of the new step based on the previous step",&polynomial);
    Teuchos::Array<std::string> sufficientdecreasecondition = Teuchos::tuple<std::string>(
      "Armijo-Goldstein",
      "Ared/Pred",
      "None");
    Teuchos::setStringToIntegralParameter<int>(
      "Sufficient Decrease Condition","Armijo-Goldstein","Choice to use for the sufficient decrease condition",
      sufficientdecreasecondition,Teuchos::tuple<int>( 0, 1, 2 ),
      &polynomial);
    DoubleParameter("Alpha Factor",1.0e-4,"Parameter choice for sufficient decrease condition",&polynomial);
    BoolParameter("Force Interpolation","No","Set to true if at least one interpolation step should be used. The default is false which means that the line search will stop if the default step length satisfies the convergence criteria",&polynomial);
    BoolParameter("Use Counters","Yes","Set to true if we should use counters and then output the result to the paramter list as described in Output Parameters",&polynomial);
    IntParameter("Maximum Iteration for Increase",0,"Maximum index of the nonlinear iteration for which we allow a relative increase",&polynomial);
    DoubleParameter("Allowed Relative Increase",100,"",&polynomial);
  }

  // sub-sub-list "More'-Thuente"
  Teuchos::ParameterList& morethuente = linesearch.sublist("More'-Thuente",false,"");
  SetPrintEqualSign(morethuente,true);

  {
    DoubleParameter("Sufficient Decrease",1.0e-4,"The ftol in the sufficient decrease condition",&morethuente);
    DoubleParameter("Curvature Condition",0.9999,"The gtol in the curvature condition",&morethuente);
    DoubleParameter("Interval Width",1.0e-15,"The maximum width of the interval containing the minimum of the modified function",&morethuente);
    DoubleParameter("Maximum Step",1.0e6,"maximum allowable step length",&morethuente);
    DoubleParameter("Minimum Step",1.0e-12,"minimum allowable step length",&morethuente);
    IntParameter("Max Iters",20,"maximum number of right-hand-side and corresponding Jacobian evaluations",&morethuente);
    DoubleParameter("Default Step",1.0,"starting step length",&morethuente);
    Teuchos::Array<std::string> recoverysteptype = Teuchos::tuple<std::string>(
      "Constant",
      "Last Computed Step");
    Teuchos::setStringToIntegralParameter<int>(
      "Recovery Step Type","Constant","Determines the step size to take when the line search fails",
      recoverysteptype,Teuchos::tuple<int>( 0, 1 ),
      &morethuente);
    DoubleParameter("Recovery Step",1.0,"The value of the step to take when the line search fails. Only used if the \"Recovery Step Type\" is set to \"Constant\"",&morethuente);
    Teuchos::Array<std::string> sufficientdecreasecondition = Teuchos::tuple<std::string>(
      "Armijo-Goldstein",
      "Ared/Pred",
      "None");
    Teuchos::setStringToIntegralParameter<int>(
      "Sufficient Decrease Condition","Armijo-Goldstein","Choice to use for the sufficient decrease condition",
      sufficientdecreasecondition,Teuchos::tuple<int>( 0, 1, 2 ),
      &morethuente);
    BoolParameter("Optimize Slope Calculation","No","Boolean value. If set to true the value of $s^T J^T F$ is estimated using a directional derivative in a call to NOX::LineSearch::Common::computeSlopeWithOutJac. If false the slope computation is computed with the NOX::LineSearch::Common::computeSlope method. Setting this to true eliminates having to compute the Jacobian at each inner iteration of the More'-Thuente line search",&morethuente);
  }

  // sub-list "Trust Region"
  Teuchos::ParameterList& trustregion = list.sublist("Trust Region",false,"");
  SetPrintEqualSign(trustregion,true);

  {
    DoubleParameter("Minimum Trust Region Radius",1.0e-6,"Minimum allowable trust region radius",&trustregion);
    DoubleParameter("Maximum Trust Region Radius",1.0e+9,"Maximum allowable trust region radius",&trustregion);
    DoubleParameter("Minimum Improvement Ratio",1.0e-4,"Minimum improvement ratio to accept the step",&trustregion);
    DoubleParameter("Contraction Trigger Ratio",0.1,"If the improvement ratio is less than this value, then the trust region is contracted by the amount specified by the \"Contraction Factor\". Must be larger than \"Minimum Improvement Ratio\"",&trustregion);
    DoubleParameter("Contraction Factor",0.25,"",&trustregion);
    DoubleParameter("Expansion Trigger Ratio",0.75,"If the improvement ratio is greater than this value, then the trust region is contracted by the amount specified by the \"Expansion Factor\"",&trustregion);
    DoubleParameter("Expansion Factor",4.0,"",&trustregion);
    DoubleParameter("Recovery Step",1.0,"",&trustregion);
  }

  // sub-list "Printing"
  Teuchos::ParameterList& printing = list.sublist("Printing",false,"");
  SetPrintEqualSign(printing,true);

  {
    BoolParameter("Error","No","",&printing);
    BoolParameter("Warning","Yes","",&printing);
    BoolParameter("Outer Iteration","Yes","",&printing);
    BoolParameter("Inner Iteration","Yes","",&printing);
    BoolParameter("Parameters","No","",&printing);
    BoolParameter("Details","No","",&printing);
    BoolParameter("Outer Iteration StatusTest","No","",&printing);
    BoolParameter("Linear Solver Details","No","",&printing);
    BoolParameter("Test Details","No","",&printing);
    /*  // for LOCA
    BoolParameter("Stepper Iteration","No","",&printing);
    BoolParameter("Stepper Details","No","",&printing);
    BoolParameter("Stepper Parameters","Yes","",&printing);
    */
    BoolParameter("Debug","No","",&printing);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
std::string DRT::INPUT::PrintEqualSign()
{
  return "*PrintEqualSign*";
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::SetPrintEqualSign(Teuchos::ParameterList& list, const bool& pes)
{
  std::string printequalsign = PrintEqualSign();
  list.set<bool>(printequalsign,pes);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::INPUT::NeedToPrintEqualSign(const Teuchos::ParameterList& list)
{
  const std::string printequalsign = PrintEqualSign();
  bool pes = false;
//  try
//  {
//    pes = list.get<bool>(printequalsign);
//  }
//  catch (Teuchos::Exceptions::InvalidParameter)
//  {
//    pes = false;
//  }
  return pes;
}

#endif
