/*----------------------------------------------------------------------*/
/*!
 \file scatra_ele_calc_advanced_reaction.cpp

 \brief main file containing routines for calculation of scatra element with advanced reaction terms


 <pre>
 \level 2

   \maintainer Moritz Thon
               thon@mhpc.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-10364
 </pre>
 *----------------------------------------------------------------------*/


#include "scatra_ele_calc_advanced_reaction.H"
#include "scatra_ele_parameter_std.H"
#include "scatra_ele_parameter_timint.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_element.H"

//MATERIALS
#include "../drt_mat/scatra_mat.H"
#include "../drt_mat/scatra_reaction_mat.H"
#include "../drt_mat/scatra_growth_scd.H"
#include "../drt_mat/matlist.H"
#include "../drt_mat/matlist_reactions.H"
#include "../drt_mat/growth_scd.H"
#include "../drt_mat/growth_law.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype,int probdim>
DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim> * DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::Instance(
  const int numdofpernode,
  const int numscal,
  const std::string& disname,
  const ScaTraEleCalcAdvReac *delete_me )
{
  static std::map<std::pair<std::string,int>,ScaTraEleCalcAdvReac<distype,probdim>* > instances;

  std::pair<std::string,int> key(disname,numdofpernode);

  if(delete_me == NULL)
  {
    if(instances.find(key) == instances.end())
      instances[key] = new ScaTraEleCalcAdvReac<distype,probdim>(numdofpernode,numscal,disname);
  }

  else
  {
    // since we keep several instances around in the general case, we need to
    // find which of the instances to delete with this call. This is done by
    // letting the object to be deleted hand over the 'this' pointer, which is
    // located in the map and deleted
    for( typename std::map<std::pair<std::string,int>,ScaTraEleCalcAdvReac<distype,probdim>* >::iterator i=instances.begin(); i!=instances.end(); ++i )
      if ( i->second == delete_me )
      {
        delete i->second;
        instances.erase(i);
        return NULL;
      }
    dserror("Could not locate the desired instance. Internal error.");
  }

  return instances[key];
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
  Instance( 0, 0, "", this );
}


/*----------------------------------------------------------------------*
 *  constructor---------------------------                   thon 02/14 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::ScaTraEleCalcAdvReac(const int numdofpernode,const int numscal,const std::string& disname)
  : DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::ScaTraEleCalc(numdofpernode,numscal,disname)
{
  my::reamanager_ = Teuchos::rcp(new ScaTraEleReaManagerAdvReac(my::numscal_));

  // safety check
  if(not my::scatrapara_->TauGP())
    dserror("For advanced reactions, tau needs to be evaluated by integration-point evaluations!");
}

/*----------------------------------------------------------------------*
 |  get the material constants  (private)                      thon 09/14|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::GetMaterialParams(
  const DRT::Element*  ele,       //!< the element we are dealing with
  std::vector<double>& densn,     //!< density at t_(n)
  std::vector<double>& densnp,    //!< density at t_(n+1) or t_(n+alpha_F)
  std::vector<double>& densam,    //!< density at t_(n+alpha_M)
  double&              visc,      //!< fluid viscosity
  const int            iquad      //!< id of current gauss point
  )
{
  // get the material
  Teuchos::RCP<MAT::Material> material = ele->Material();

  // We may have some reactive and some non-reactive elements in one discretisation.
  // But since the calculation classes are singleton, we have to reset all reactive stuff in case
  // of non-reactive elements:
  ReaManager()->Clear(my::numscal_);

  if (material->MaterialType() == INPAR::MAT::m_matlist)
  {
    const Teuchos::RCP<const MAT::MatList> actmat = Teuchos::rcp_dynamic_cast<const MAT::MatList>(material);
    if (actmat->NumMat() != my::numscal_) dserror("Not enough materials in MatList.");

    for (int k = 0;k<my::numscal_;++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP< MAT::Material> singlemat = actmat->MaterialById(matid);

      Materials(singlemat,k,densn[k],densnp[k],densam[k],visc,iquad);
    }
  }

  else if (material->MaterialType() == INPAR::MAT::m_matlist_reactions)
  {
    const Teuchos::RCP<MAT::MatListReactions> actmat = Teuchos::rcp_dynamic_cast<MAT::MatListReactions>(material);
    if (actmat->NumMat() != my::numscal_) dserror("Not enough materials in MatList.");

    for (int k = 0;k<my::numscal_;++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP< MAT::Material> singlemat = actmat->MaterialById(matid);

      //Note: order is important here!!
      Materials(singlemat,k,densn[k],densnp[k],densam[k],visc,iquad);

      SetAdvancedReactionTerms(k,actmat); //every reaction calculation stuff happens in here!!
    }
  }

  else
  {
    Materials(material,0,densn[0],densnp[0],densam[0],visc,iquad);
  }

  return;
} //ScaTraEleCalc::GetMaterialParams

/*----------------------------------------------------------------------*
 |  evaluate single material  (protected)                    thon 02/14 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::Materials(
  const Teuchos::RCP<const MAT::Material> material, //!< pointer to current material
  const int                               k,        //!< id of current scalar
  double&                                 densn,    //!< density at t_(n)
  double&                                 densnp,   //!< density at t_(n+1) or t_(n+alpha_F)
  double&                                 densam,   //!< density at t_(n+alpha_M)
  double&                                 visc,         //!< fluid viscosity
  const int                               iquad         //!< id of current gauss point
  )
{
  switch(material->MaterialType())
  {
  case INPAR::MAT::m_scatra:
    my::MatScaTra(material,k,densn,densnp,densam,visc,iquad);
    break;
  case INPAR::MAT::m_scatra_growth_scd:
    MatGrowthScd(material,k,densn,densnp,densam,visc,iquad);
    break;
  default:
    dserror("Material type %i is not supported",material->MaterialType());
    break;
  }
  return;
}


/*----------------------------------------------------------------------*
 |  Material GrowthScd                                       vuong 01/14 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::MatGrowthScd(
    const Teuchos::RCP<const MAT::Material> material, //!< pointer to current material
    const int                               k,        //!< id of current scalar
    double&                                 densn,    //!< density at t_(n)
    double&                                 densnp,   //!< density at t_(n+1) or t_(n+alpha_F)
    double&                                 densam,   //!< density at t_(n+alpha_M)
    double&                                 visc,         //!< fluid viscosity
    const int                               iquad         //!< id of current gauss point
  )
{
  dsassert(my::numdofpernode_==1,"more than 1 dof per node for ScatraGrowthScd material");

  if(iquad < 0) dserror("ScatraGrowthScd material has to be evaluated at gauss point!");

  const Teuchos::RCP<const MAT::ScatraGrowthScd>& actmat
    = Teuchos::rcp_dynamic_cast<const MAT::ScatraGrowthScd>(material);

  // get and save constant diffusivity
  my::diffmanager_->SetIsotropicDiff(actmat->Diffusivity(),k);

  //strategy to obtain theta from the structure at equivalent gauss-point
  //access structure discretization
  Teuchos::RCP<DRT::Discretization> structdis = Teuchos::null;
  structdis = DRT::Problem::Instance()->GetDis("structure");
  //get corresponding structure element (it has the same global ID as the scatra element)
  DRT::Element* structele = structdis->gElement(my::eid_);
  if (structele == NULL)
    dserror("Structure element %i not on local processor", my::eid_);

  const Teuchos::RCP<const MAT::GrowthScd>& structmat
          = Teuchos::rcp_dynamic_cast<const MAT::GrowthScd>(structele->Material());
  if (structmat == Teuchos::null)
    dserror("dynamic cast of structure material GrowthScd failed.");
  if(structmat->MaterialType() != INPAR::MAT::m_growth_volumetric_scd)
    dserror("invalid structure material for scalar dependent growth");

  if (structmat->Parameter()->growthlaw_->MaterialType() == INPAR::MAT::m_growth_linear or
      structmat->Parameter()->growthlaw_->MaterialType() == INPAR::MAT::m_growth_exponential)
  {
    const double theta    = structmat->Gettheta_atgp(iquad);
    const double dtheta   = structmat->Getdtheta_atgp(iquad);
    const double thetaold = structmat->Getthetaold_atgp(iquad);
    const double detFe    = structmat->GetdetFe_atgp(iquad);

    // get substrate concentration at n+1 or n+alpha_F at integration point
    const double csnp = my::scatravarmanager_->Phinp(k);
    const Teuchos::RCP<ScaTraEleReaManagerAdvReac> remanager = ReaManager();

    const double reaccoeff = actmat->ComputeReactionCoeff(csnp,theta,dtheta,detFe);
    // set reaction coefficient
    remanager->AddToReaBodyForce(-actmat->ComputeReactionCoeff(csnp,theta,dtheta,detFe)*csnp,k);
    // set derivative of reaction coefficient
    remanager->AddToReaBodyForceDerivMatrix(-actmat->ComputeReactionCoeffDeriv(csnp,theta,thetaold,1.0)*csnp-reaccoeff,k,k);

    // set density at various time steps and density gradient factor to 1.0/0.0
    densn      = 1.0;
    densnp     = 1.0;
    densam     = 1.0;
  }
  else if (structmat->Parameter()->growthlaw_->MaterialType() == INPAR::MAT::m_growth_ac or
           structmat->Parameter()->growthlaw_->MaterialType() == INPAR::MAT::m_growth_ac_radial or
           structmat->Parameter()->growthlaw_->MaterialType() == INPAR::MAT::m_growth_ac_radial_refconc )

    {
    dserror("In the case of MAT_GrowthAC or MAT_GrowthACNormal one should not end up in here, "
        "since the growth does only change the scalars field size/volume. And this is already"
        " cared due to the conservative formulation you hopefully use!");
    }
  else
  {
    dserror("Your growth law is not a valid one!");
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Get right hand side including reaction bodyforce term    thon 02/14 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::GetRhsInt(
  double&      rhsint,   //!< rhs containing bodyforce at Gauss point
  const double densnp,  //!< density at t_(n+1)
  const int    k        //!< index of current scalar
  )
{
                                       //... + all advanced reaction terms
  rhsint = my::bodyforce_[k].Dot(my::funct_) + densnp*ReaManager()->GetReaBodyForce(k);

  return;
}

/*--------------------------------------------------------------------------- *
 |  calculation of reactive element matrix for coupled reactions  thon 02/14  |
 *----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::CalcMatReact(
  Epetra_SerialDenseMatrix&          emat,
  const int                          k,
  const double                       timefacfac,
  const double                       timetaufac,
  const double                       taufac,
  const double                       densnp,
  const LINALG::Matrix<my::nen_,1>&      sgconv,
  const LINALG::Matrix<my::nen_,1>&      diff
  )
{
  // -----------------first care for 'easy' reaction terms K*(\partial_c c)=Id*K--------------------------------------
  // NOTE: K_i must not depend on any concentrations!! Otherwise we loose the corresponding linearisations.

  my::CalcMatReact(emat,k,timefacfac,timetaufac,taufac,densnp,sgconv,diff);

  const LINALG::Matrix<my::nen_,1>&   conv  = my::scatravarmanager_->Conv();

  // -----------------second care for advanced reaction terms ( - (\partial_c f(c) )------------
  // NOTE: The shape of f(c) can be arbitrary. So better consider using this term for new implementations

  const Teuchos::RCP<ScaTraEleReaManagerAdvReac> remanager = ReaManager();

  LINALG::Matrix<my::nen_,1> functint = my::funct_;
  if (not my::scatrapara_->MatGP())
    functint = funct_elementcenter_;

  for (int j=0; j<my::numscal_ ;j++)
  {
    const double fac_reac        = timefacfac*densnp*( - remanager->GetReaBodyForceDerivMatrix(k,j) );
    const double timetaufac_reac = timetaufac*densnp*( - remanager->GetReaBodyForceDerivMatrix(k,j) );

    //----------------------------------------------------------------
    // standard Galerkin reactive term
    //----------------------------------------------------------------
    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double v = fac_reac*functint(vi);
      const int fvi = vi*my::numdofpernode_+k;

      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = ui*my::numdofpernode_+j;

        emat(fvi,fui) += v*my::funct_(ui);
      }
    }

    //----------------------------------------------------------------
    // stabilization of reactive term
    //----------------------------------------------------------------
    if(my::scatrapara_->StabType()!=INPAR::SCATRA::stabtype_no_stabilization)
    {
      double densreataufac = timetaufac_reac*densnp;
      // convective stabilization of reactive term (in convective form)
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = densreataufac*(conv(vi)+sgconv(vi)+my::scatrapara_->USFEMGLSFac()*1.0/my::scatraparatimint_->TimeFac()*functint(vi));
        const int fvi = vi*my::numdofpernode_+k;

        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui = ui*my::numdofpernode_+j;

          emat(fvi,fui) += v*my::funct_(ui);
        }
      }

      if (my::use2ndderiv_)
      {
        // diffusive stabilization of reactive term
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const double v = my::scatrapara_->USFEMGLSFac()*timetaufac_reac*diff(vi);
          const int fvi = vi*my::numdofpernode_+k;

          for (int ui=0; ui<my::nen_; ++ui)
          {
            const int fui = ui*my::numdofpernode_+j;

            emat(fvi,fui) -= v*my::funct_(ui);
          }
        }
      }

      //----------------------------------------------------------------
      // reactive stabilization
      //----------------------------------------------------------------
      densreataufac = my::scatrapara_->USFEMGLSFac()*timetaufac_reac*densnp;

      // reactive stabilization of convective (in convective form) and reactive term
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = densreataufac*functint(vi);
        const int fvi = vi*my::numdofpernode_+k;

        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui = ui*my::numdofpernode_+j;

          emat(fvi,fui) += v*(conv(ui)+remanager->GetReaCoeff(k)*my::funct_(ui));
        }
      }

      if (my::use2ndderiv_)
      {
        // reactive stabilization of diffusive term
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const double v = my::scatrapara_->USFEMGLSFac()*timetaufac_reac*my::funct_(vi);
          const int fvi = vi*my::numdofpernode_+k;

          for (int ui=0; ui<my::nen_; ++ui)
          {
            const int fui = ui*my::numdofpernode_+j;

            emat(fvi,fui) -= v*diff(ui);
          }
        }
      }


      if (not my::scatraparatimint_->IsStationary())
      {
        // reactive stabilization of transient term
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const double v = my::scatrapara_->USFEMGLSFac()*taufac*densnp*remanager->GetReaCoeff(k)*densnp*functint(vi);
          const int fvi = vi*my::numdofpernode_+k;

          for (int ui=0; ui<my::nen_; ++ui)
          {
            const int fui = ui*my::numdofpernode_+j;

            emat(fvi,fui) += v*my::funct_(ui);
          }
        }

        if (my::use2ndderiv_ and remanager->GetReaCoeff(k)!=0.0)
          dserror("Second order reactive stabilization is not fully implemented!! ");
      }
    }
  } //end for
  return;
}


/*-------------------------------------------------------------------------------*
 |  Set advanced reaction terms and derivatives                       thon 09/14 |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::SetAdvancedReactionTerms(
    const int                                 k,          //!< index of current scalar
    const Teuchos::RCP<MAT::MatListReactions> matreaclist //!< index of current scalar
    )
{
  const Teuchos::RCP<ScaTraEleReaManagerAdvReac> remanager = ReaManager();

  remanager->AddToReaBodyForce( matreaclist->CalcReaBodyForceTerm(k,my::scatravarmanager_->Phinp()) ,k );

  matreaclist->CalcReaBodyForceDerivMatrix(k,remanager->GetReaBodyForceDerivVector(k),my::scatravarmanager_->Phinp());

}

/*----------------------------------------------------------------------*
 | evaluate shape functions and derivatives at ele. center   jhoer 11/14 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::EvalShapeFuncAndDerivsAtEleCenter()
{
  const double vol = my::EvalShapeFuncAndDerivsAtEleCenter();

  //shape function at element center
  funct_elementcenter_ = my::funct_;

  return vol;

} //ScaTraImpl::EvalShapeFuncAndDerivsAtEleCenter


// template classes

// 1D elements
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::line2,1>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::line2,2>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::line2,3>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::line3,1>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::tri3,2>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::tri3,3>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::tri6,2>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::quad4,2>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::quad4,3>;
//template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::quad8>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::quad9,2>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::nurbs9,2>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::hex8,3>;
//template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::hex20>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::hex27,3>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::tet4,3>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::tet10,3>;
//template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::wedge6>;
template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::pyramid5,3>;
//template class DRT::ELEMENTS::ScaTraEleCalcAdvReac<DRT::Element::nurbs27>;
