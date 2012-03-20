/*----------------------------------------------------------------------*/
/*!
\file elast_isoanisoexpo.cpp
\brief


the input line should read
  MAT 1 ELAST_IsoAnisoExpo K1 10.0 K2 1.0 GAMMA 35.0

<pre>
Maintainer: Susanna Tinkl
            tinkl@lnm.mw.tum.de
            089/289 15265
</pre>
*/


/*----------------------------------------------------------------------*/
/* headers */
#include "elast_isoanisoexpo.H"
#include "../drt_mat/material.H"
#include "../drt_lib/drt_linedefinition.H"

/*----------------------------------------------------------------------*
 |                                                                      |
 *----------------------------------------------------------------------*/
MAT::ELASTIC::PAR::IsoAnisoExpo::IsoAnisoExpo(
  Teuchos::RCP<MAT::PAR::Material> matdata
  )
: Parameter(matdata),
  k1_(matdata->GetDouble("K1")),
  k2_(matdata->GetDouble("K2")),
  gamma_(matdata->GetDouble("GAMMA")),
  k1comp_(matdata->GetDouble("K1COMP")),
  k2comp_(matdata->GetDouble("K2COMP"))
{
}


Teuchos::RCP<MAT::Material> MAT::ELASTIC::PAR::IsoAnisoExpo::CreateMaterial()
{
  return Teuchos::null;
  //return Teuchos::rcp( new MAT::ELASTIC::IsoAnisoExpo( this ) );
}


/*----------------------------------------------------------------------*
 |  Constructor                                   (public)  st    03/12 |
 *----------------------------------------------------------------------*/
MAT::ELASTIC::IsoAnisoExpo::IsoAnisoExpo()
  : Summand(),
    params_(NULL)
{
}


/*----------------------------------------------------------------------*
 |  Constructor                             (public)   st         03/12 |
 *----------------------------------------------------------------------*/
MAT::ELASTIC::IsoAnisoExpo::IsoAnisoExpo(MAT::ELASTIC::PAR::IsoAnisoExpo* params)
  : params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::PackSummand(DRT::PackBuffer& data) const
{
  AddtoPack(data,a_);
  AddtoPack(data,A_);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::UnpackSummand(const std::vector<char>& data,
                                                  vector<char>::size_type& position)
{
  ExtractfromPack(position,data,a_);
  ExtractfromPack(position,data,A_);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::Setup(DRT::INPUT::LineDefinition* linedef)
{
  // fibers aligned in local element cosy with gamma around circumferential direction
  // -> check whether element supports local element cosy
  vector<double> rad;
  vector<double> axi;
  vector<double> cir;
  if (linedef->HaveNamed("RAD") and
      linedef->HaveNamed("AXI") and
      linedef->HaveNamed("CIR"))
  {
    // read local (cylindrical) cosy-directions at current element
    // basis is local cosy with third vec e3 = circumferential dir and e2 = axial dir
    LINALG::Matrix<3,3> locsys(true);
    linedef->ExtractDoubleVector("RAD",rad);
    linedef->ExtractDoubleVector("AXI",axi);
    linedef->ExtractDoubleVector("CIR",cir);
    double radnorm=0.; double axinorm=0.; double cirnorm=0.;

    for (int i = 0; i < 3; ++i)
    {
      radnorm += rad[i]*rad[i]; axinorm += axi[i]*axi[i]; cirnorm += cir[i]*cir[i];
    }
    radnorm = sqrt(radnorm); axinorm = sqrt(axinorm); cirnorm = sqrt(cirnorm);

    for (int i=0; i<3; ++i)
    {
      locsys(i,0) = rad[i]/radnorm;
      locsys(i,1) = axi[i]/axinorm;
      locsys(i,2) = cir[i]/cirnorm;
    }

    SetFiberVecs(locsys);
  }
  else
  {
    dserror("Reading of element local cosy for anisotropic materials failed");
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::AddStressAnisoModified(
    const LINALG::Matrix<6,1> rcg,
    LINALG::Matrix<6,1> icg,
    LINALG::Matrix<6,6>& cmat,
    LINALG::Matrix<6,1>& stress,
    double I3
)
{
  double incJ = pow(I3,-1.0/3.0);  // J^{-2/3}

  double J4 = incJ * ( A_(0)*rcg(0) + A_(1)*rcg(1) + A_(2)*rcg(2)
            + A_(3)*rcg(3) + A_(4)*rcg(4) + A_(5)*rcg(5)); //J4 = J^{-2/3} I4

  double k1 = params_->k1_;
  double k2 = params_->k2_;

  if (J4 < 1.0)
  {
    k1 = params_->k1comp_;
    k2 = params_->k2comp_;
  }

  LINALG::Matrix<6,1> Saniso(A_); // first compute Sfbar = 2 dW/dJ4 A_
  double gammabar = 2.*(k1*(J4-1.)*exp(k2*(J4-1.)*(J4-1.)));  // 2 dW/dJ4
  Saniso.Scale(gammabar);  //Sfbar

  double traceCSfbar =  Saniso(0)*rcg(0) + Saniso(1)*rcg(1) + Saniso(2)*rcg(2)
                      + 1.*(Saniso(3)*rcg(3) + Saniso(4)*rcg(4) + Saniso(5)*rcg(5));
  Saniso.Update(-incJ/3.*traceCSfbar,icg,incJ);

  LINALG::Matrix<6,6>  Psl(true);        // Psl = Cinv o Cinv - 1/3 Cinv x Cinv
  AddtoCmatHolzapfelProduct(Psl,icg,1.0);
  Psl.MultiplyNT(-1./3.,icg,icg,1.0);

  LINALG::Matrix<6,1> Aiso(A_);
  Aiso.Update(-J4/3.0,icg,incJ);
  LINALG::Matrix<6,6> cmataniso(true); // isochoric elastic cmat
  double deltabar = 2.*(1. + 2.*k2*(J4-1.)*(J4-1.))*2.*k1* exp(k2*(J4-1.)*(J4-1.)); // 4 d^2Wf/dJ4dJ4
  cmataniso.MultiplyNT(deltabar,Aiso,Aiso);
  cmataniso.Update(2./3.*incJ*traceCSfbar,Psl,1.0);
  cmataniso.MultiplyNT(-2./3.,icg,Saniso,1.0);
  cmataniso.MultiplyNT(-2./3.,Saniso,icg,1.0);

  stress.Update(1.0,Saniso,1.0);
  cmat.Update(1.0,cmataniso,1.0);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::SetFiberVecs(
    LINALG::Matrix<3,3> locsys
)
{
  if ((params_->gamma_<-90) || (params_->gamma_ >90)) dserror("Fiber angle not in [-90,90]");
  //convert
  const double gamma = (params_->gamma_*PI)/180.;

  for (int i = 0; i < 3; ++i)
  {
    // a = cos gamma e3 + sin gamma e2
    a_(i) = cos(gamma)*locsys(i,2) + sin(gamma)*locsys(i,1);
  }

  for (int i = 0; i < 3; ++i)
    A_(i) = a_(i)*a_(i);

  A_(3) = a_(0)*a_(1); A_(4) = a_(1)*a_(2); A_(5) = a_(0)*a_(2);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::IsoAnisoExpo::GetFiberVecs(
    std::vector<LINALG::Matrix<3,1> >& fibervecs ///< vector of all fiber vectors
)
{
  fibervecs.push_back(a_);
}
