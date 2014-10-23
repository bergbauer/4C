/*----------------------------------------------------------------------*/
/*!
\file fluid_ele_parameter_poro.cpp

\brief Evaluation of general fluid parameter for fluid in poroelast problem

FluidEleParameter::SetElementPoroParameter(Teuchos::ParameterList& params)
set all general porofluid parameter once for all elements.

<pre>
Maintainers: Anh-Tu Vuong and Andreas Rauch
             {vuong,rauch}@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289-15251/-15240
</pre>
*/
/*----------------------------------------------------------------------*/
#include "fluid_ele_parameter_poro.H"
#include "../drt_lib/drt_globalproblem.H"

//----------------------------------------------------------------------*/
//    definition of the instance
//----------------------------------------------------------------------*/
DRT::ELEMENTS::FluidEleParameterPoro* DRT::ELEMENTS::FluidEleParameterPoro::Instance( bool create )
{
  static FluidEleParameterPoro* instance;
  if ( create )
  {
    if ( instance==NULL )
    {
      instance = new FluidEleParameterPoro();
    }
  }
  else
  {
    if ( instance!=NULL )
      delete instance;
    instance = NULL;
  }
  return instance;
}

//----------------------------------------------------------------------*/
//    destruction method
//----------------------------------------------------------------------*/
void DRT::ELEMENTS::FluidEleParameterPoro::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
    Instance( false );
}

//----------------------------------------------------------------------*/
//    constructor
//----------------------------------------------------------------------*/
DRT::ELEMENTS::FluidEleParameterPoro::FluidEleParameterPoro()
  : DRT::ELEMENTS::FluidEleParameter::FluidEleParameter(),
    set_fluid_parameter_poro_(false),
    poro_conti_partint_(false),
    time_distype_conti_(INPAR::POROELAST::pressure)
{
}

//----------------------------------------------------------------------*
//  set poro parameters                                      vuong 11/12|
//---------------------------------------------------------------------*/
void DRT::ELEMENTS::FluidEleParameterPoro::SetElementPoroParameter( Teuchos::ParameterList& params, int myrank)
{
  SetElementGeneralFluidParameter(params,myrank);

  set_fluid_parameter_poro_ = true;
  poro_conti_partint_ = params.get<bool>("conti partial integration",false);
  reaction_= true;
  time_distype_conti_ = DRT::INPUT::get<INPAR::POROELAST::TimeDisTypeConti>(params, "Time DisType Conti");
}

//----------------------------------------------------------------------*/
// print fluid parameter to screen                          rauch 11/13 |
//----------------------------------------------------------------------*/
void DRT::ELEMENTS::FluidEleParameterPoro::PrintFluidParameterPoro()
{
  std::cout << std::endl << "|-----------------------------------------------------------------------------" << std::endl;
  std::cout << "|  Poro Fluid parameter: " << std::endl;
  std::cout << "|-----------------------------------------------------------------------------" << std::endl;
  // flag SetGeneralParameter was called
  std::cout << "|    method SetElementParameterPoro was called:    " << set_fluid_parameter_poro_ << std::endl;
  // flag to (de)activate stationary formulation
  std::cout << "|    Partial integration of conti equation:    " << poro_conti_partint_ << std::endl;
  // type of time discretization for continuity equation
  std::cout << "|   type of time discretization for continuity equation:  " << time_distype_conti_ << std::endl;
  // flag to (de)activate Newton linearization
  std::cout << "|    Type of stabilization:    " << stabtype_ << std::endl;

  std::cout << "|---------------------------------------------------------------------------" << std::endl;

}
