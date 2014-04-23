
#include "fsi_nox_aitken.H"
#include "fsi_utils.H"

#include <NOX_Common.H>
#include <NOX_Abstract_Vector.H>
#include <NOX_Abstract_Group.H>
#include <NOX_Solver_Generic.H>
#include <Teuchos_ParameterList.hpp>
#include <NOX_GlobalData.H>

#include <Epetra_Vector.h>
#include <Epetra_Comm.h>
#include <NOX_Epetra_Vector.H>
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_io/io_control.H"

NOX::FSI::AitkenRelaxation::AitkenRelaxation(const Teuchos::RCP<NOX::Utils>& utils,
                                             Teuchos::ParameterList& params)
  : utils_(utils)
{
  Teuchos::ParameterList& p = params.sublist("Aitken");
  nu_ = p.get("Start nu", 0.0);

  double maxstep = p.get("max step size", 0.0);
  if (maxstep > 0)
    nu_ = 1-maxstep;
}


NOX::FSI::AitkenRelaxation::~AitkenRelaxation()
{
}


bool NOX::FSI::AitkenRelaxation::reset(const Teuchos::RCP<NOX::GlobalData>& gd,
                                       Teuchos::ParameterList& params)
{
  Teuchos::ParameterList& p = params.sublist("Aitken");

  // do not reset the aitken factor
  //nu_ = p.get("Start nu", 0.0);

  // We might want to constrain the step size of the first relaxation
  // in a new time step.
  double maxstep = p.get("max step size", 0.0);
  if (maxstep > 0 && maxstep < 1-nu_)
    nu_ = 1-maxstep;

  if (!is_null(del_))
  {
    del_->init(1e20);
  }
  utils_ = gd->getUtils();
  return true;
}


bool NOX::FSI::AitkenRelaxation::compute(Abstract::Group& grp, double& step,
                                         const Abstract::Vector& dir,
                                         const Solver::Generic& s)
{
  if (utils_->isPrintType(NOX::Utils::InnerIteration))
  {
    utils_->out() << "\n" << NOX::Utils::fill(72) << "\n"
                  << "-- Aitken Line Search -- \n";
  }

  const Abstract::Group& oldGrp = s.getPreviousSolutionGroup();
  const NOX::Abstract::Vector& F = oldGrp.getF();


  // This occurs in case of FSI-crack simulations
  // When new elements are added to FSI interface, the EpetraVectos do not have same dimensions
  // In this case, we calculate relaxation parameter as in the beginning of the simulation
  if( (not is_null(del_)) and
       ( not dynamic_cast<const NOX::Epetra::Vector&>(F).getEpetraVector().Map().PointSameAs(
           dynamic_cast<NOX::Epetra::Vector&>(*del_).getEpetraVector().Map() ) ) )
  {
    del_ = Teuchos::null;
  }



  // turn off switch

  if (is_null(del_))
  {
    del_  = F.clone(ShapeCopy);
    del2_ = F.clone(ShapeCopy);
    del_->init(1.0e20);
    del2_->init(0.0);
  }

  del2_->update(1,*del_,1,F);
  del_ ->update(-1,F);

  const double top = del2_->innerProduct(*del_);
  const double den = del2_->innerProduct(*del2_);

  nu_ = nu_ + (nu_ - 1.)*top/den;
  step = 1. - nu_;

  utils_->out() << "          RELAX = " << std::setw(5) << step << "\n";

  grp.computeX(oldGrp, dir, step);

  // Calculate F anew here. This results in another FSI loop. However
  // the group will store the result, so it will be reused until the
  // group's x is changed again. We do not waste anything.
  grp.computeF();

  // is this reasonable at this point?
  double checkOrthogonality = fabs( grp.getF().innerProduct(dir) );

  if (utils_->isPrintType(Utils::InnerIteration))
  {
    utils_->out() << std::setw(3) << "1" << ":";
    utils_->out() << " step = " << utils_->sciformat(step);
    utils_->out() << " orth = " << utils_->sciformat(checkOrthogonality);
    utils_->out() << "\n" << NOX::Utils::fill(72) << "\n" << std::endl;
  }

  // write omega
  double fnorm = grp.getF().norm();
  if (dynamic_cast<const NOX::Epetra::Vector&>(F).getEpetraVector().Comm().MyPID()==0)
  {
    static int count;
    static std::ofstream* out;
    if (out==NULL)
    {
      std::string s = DRT::Problem::Instance()->OutputControlFile()->FileName();
      s.append(".omega");
      out = new std::ofstream(s.c_str());
    }
    (*out) << count << " "
           << step << " "
           << fnorm
           << "\n";
    count += 1;
    out->flush();
  }

  return true;
}
