/*----------------------------------------------------------------------*/
/*!
\file scatra_ele_utils_elch_diffcond.cpp

\brief utility class supporting element evaluation for concentrated electrolytes

<pre>
Maintainer: Rui Fang
            fang@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/
            089-289-15251
</pre>
 */
/*----------------------------------------------------------------------*/
#include "scatra_ele_utils_elch_diffcond.H"
#include "scatra_ele_calc_elch_diffcond.H"

#include "../drt_mat/elchmat.H"
#include "../drt_mat/elchphase.H"
#include "../drt_mat/newman.H"


/*----------------------------------------------------------------------*
 | singleton access method                                   fang 07/15 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>* DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::Instance(
    const int numdofpernode,
    const int numscal,
    bool create
    )
{
  static ScaTraEleUtilsElchDiffCond<distype>* instance;

  if(create)
  {
    if(instance == NULL)
      instance = new ScaTraEleUtilsElchDiffCond<distype>(numdofpernode,numscal);
  }

  else if(instance != NULL)
  {
    delete instance;
    instance = NULL;
  }

  return instance;
}


/*----------------------------------------------------------------------*
 | singleton destruction                                     fang 07/15 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::Done()
{
  // delete singleton
  Instance(0,0,false);

  return;
}


/*----------------------------------------------------------------------*
 | private constructor for singletons                        fang 07/15 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::ScaTraEleUtilsElchDiffCond(const int numdofpernode,const int numscal) :
  myelectrode::ScaTraEleUtilsElchElectrode(numdofpernode,numscal)
{
  return;
}


/*----------------------------------------------------------------------*
 | evaluate electrolyte material                             fang 07/15 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::MatElchMat(
    const Teuchos::RCP<const MAT::Material>                 material,         //!< electrolyte material
    const std::vector<double>&                              concentrations,   //!< concentrations
    const double                                            ffrt,             //!< factor F²/RT
    const INPAR::ELCH::EquPot                               equpot,           //!< type of closing equation for electric potential
    const Teuchos::RCP<ScaTraEleDiffManagerElchDiffCond>&   diffmanager,      //!< diffusion manager
    INPAR::ELCH::DiffCondMat&                               diffcondmat       //!< ion type
    )
{
  // cast material to electrolyte material
  const Teuchos::RCP<const MAT::ElchMat> elchmat = Teuchos::rcp_static_cast<const MAT::ElchMat>(material);

  // safety check
  if(elchmat->NumPhase() != 1)
    dserror("Can only have a single electrolyte phase at the moment!");

  // extract electrolyte phase
  const Teuchos::RCP<const MAT::Material> elchphase = elchmat->PhaseById(elchmat->PhaseID(0));

  if(elchphase->MaterialType() == INPAR::MAT::m_elchphase)
    // evaluate electrolyte phase
    MatElchPhase(elchphase,concentrations,ffrt,equpot,diffmanager,diffcondmat);
  else
    dserror("Invalid material type!");

  return;
}

/*----------------------------------------------------------------------*
 | evaluate electrolyte phase                                fang 07/15 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::MatElchPhase(
    const Teuchos::RCP<const MAT::Material>                 material,         //!< electrolyte phase
    const std::vector<double>&                              concentrations,   //!< concentrations
    const double                                            ffrt,             //!< factor F²/RT
    const INPAR::ELCH::EquPot                               equpot,           //!< type of closing equation for electric potential
    const Teuchos::RCP<ScaTraEleDiffManagerElchDiffCond>&   diffmanager,      //!< diffusion manager
    INPAR::ELCH::DiffCondMat&                               diffcondmat       //!< ion type
    )
{
  // cast material to electrolyte phase
  const Teuchos::RCP<const MAT::ElchPhase> matelchphase = Teuchos::rcp_static_cast<const MAT::ElchPhase>(material);

  // set porosity
  diffmanager->SetPhasePoro(matelchphase->Epsilon(),0);

  // set tortuosity
  diffmanager->SetPhaseTort(matelchphase->Tortuosity(),0);

  // loop over materials within electrolyte phase
  for(int imat=0; imat<matelchphase->NumMat(); ++imat)
  {
    const Teuchos::RCP<const MAT::Material> material = matelchphase->MatById(matelchphase->MatID(imat));

    switch(material->MaterialType())
    {
      case INPAR::MAT::m_newman:
      {
        // safety check
        if(matelchphase->NumMat() != 1)
          dserror("Newman material must be the only transported species!");

        // set ion type
        diffcondmat = INPAR::ELCH::diffcondmat_newman;

        // evaluate Newman material
        MatNewman(material,concentrations[0],diffmanager);

        break;
      }

      case INPAR::MAT::m_ion:
      {
        // set ion type
        diffcondmat = INPAR::ELCH::diffcondmat_ion;

        myelch::MatIon(material,imat,equpot,diffmanager);

        // calculation of conductivity and transference number based on diffusion coefficient and valence
        if(imat == matelchphase->NumMat()-1)
        {
          diffmanager->CalcConductivity(matelchphase->NumMat(),ffrt,concentrations);
          diffmanager->CalcTransNum(matelchphase->NumMat(),concentrations);
        }

        break;
      }

      default:
      {
        dserror("Invalid material type!");
        break;
      }
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 | evaluate Newman material                                  fang 07/15 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<distype>::MatNewman(
    const Teuchos::RCP<const MAT::Material>                 material,        //!< Newman material
    const double                                            concentration,   //!< concentration
    const Teuchos::RCP<ScaTraEleDiffManagerElchDiffCond>&   diffmanager      //!< diffusion manager
    )
{
  // cast material to Newman material
  const Teuchos::RCP<const MAT::Newman> matnewman = Teuchos::rcp_static_cast<const MAT::Newman>(material);

  // valence of ionic species
  diffmanager->SetValence(matnewman->Valence(),0);

  // concentration depending diffusion coefficient
  diffmanager->SetIsotropicDiff(matnewman->ComputeDiffusionCoefficient(concentration),0);
  // derivation of concentration depending diffusion coefficient wrt all ionic species
  diffmanager->SetDerivIsoDiffCoef(matnewman->ComputeFirstDerivDiffCoeff(concentration),0,0);

  // concentration depending transference number
  diffmanager->SetTransNum(matnewman->ComputeTransferenceNumber(concentration),0);
  // derivation of concentration depending transference number wrt all ionic species
  diffmanager->SetDerivTransNum(matnewman->ComputeFirstDerivTrans(concentration),0,0);

  // thermodynamic factor of electrolyte solution
  diffmanager->SetThermFac(matnewman->ComputeThermFac(concentration));
  // derivative of conductivity with respect to concentrations
  diffmanager->SetDerivThermFac(matnewman->ComputeFirstDerivThermFac(concentration),0);

  // conductivity and first derivative can maximally depend on one concentration
  // since time curve is used as input routine
  // conductivity of electrolyte solution
  diffmanager->SetCond(matnewman->ComputeConductivity(concentration));
  // derivative of conductivity with respect to concentrations
  diffmanager->SetDerivCond(matnewman->ComputeFirstDerivCond(concentration),0);

  return;
}


// template classes
// 1D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::line2>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::line3>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::quad4>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::quad8>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::quad9>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::tri3>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::tri6>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::nurbs3>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::nurbs9>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::hex8>;
//template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::hex20>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::hex27>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::tet4>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::tet10>;
//template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::wedge6>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::pyramid5>;
//template class DRT::ELEMENTS::ScaTraEleUtilsElchDiffCond<DRT::Element::nurbs27>;