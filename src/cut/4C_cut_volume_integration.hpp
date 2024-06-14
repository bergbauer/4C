/*---------------------------------------------------------------------*/
/*! \file

\brief Integrates base functions over volume, distribute Gauss points and solve moment fitting
equations

\level 3


*----------------------------------------------------------------------*/


#ifndef FOUR_C_CUT_VOLUME_INTEGRATION_HPP
#define FOUR_C_CUT_VOLUME_INTEGRATION_HPP

#include "4C_config.hpp"

#include "4C_cut_enum.hpp"
#include "4C_cut_facet_integration.hpp"
#include "4C_cut_volumecell.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_linalg_utils_sparse_algebra_math.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN

namespace Core::Geo
{
  namespace Cut
  {
    class Element;
    class Facet;
    class VolumeCell;
    // While performing the volume integration, the points of the facet should be arranged in
    // anti-clockwise manner when looking the surface away from the body this ensures outward normal
    // vector
    /*!
    \brief Class to construct integration rule over the volumecell in element local coordinates.
    It performs integration of base functions over the volume, distribution of Gauss points and
    solution of moment fitting matrix to arrive at the Gauss weights
     */
    class VolumeIntegration
    {
     public:
      VolumeIntegration(VolumeCell* volcell, Element* elem,
          const Core::Geo::Cut::Point::PointPosition posi, int num_func)
          : volcell_(volcell), elem1_(elem), position_(posi), num_func_(num_func)
      {
      }

      /*!
      \brief Compute Gauss point weights by solving the moment fitting equations and returns the
      coordinates of Gauss points and their corresponding weights
      */
      Core::LinAlg::SerialDenseVector compute_weights();

      /*!
      \brief Computes the RHS of the moment fitting matrix (performs integration of base functions
      over the volumecell)
      */
      Core::LinAlg::SerialDenseVector compute_rhs_moment();

      /*!
      \brief Returns the location of Gauss points distributed over the volumecell
      */
      std::vector<std::vector<double>> get_gauss_point_location() { return gaus_pts_; }

      /*!
      \brief Check whether the point with this element Local coordinates is inside, outisde or
      boundary of this volumecell. The output std::string will be either "outside", "inside" or
      "onBoundary"
      */
      std::string IsPointInside(Core::LinAlg::Matrix<3, 1>& rst);

     private:
      /*!
      \brief Distribute Gaussian points over the volumecell with "numeach" points in each direction
      */
      bool compute_gaussian_points(int numeach);

      /*!
      \brief Computes the moment fitting matrix
      */
      void moment_fitting_matrix(
          std::vector<std::vector<double>>& mom, std::vector<std::vector<double>> gauspts);

      /*!
      \brief Check whether the generated ray intersect the facets of the volumecell, if so
      distribute Gauss points along this ray
      */
      bool is_intersect(double* pt, double* mini, double* maxi,
          std::vector<std::vector<double>>& linePts, std::vector<std::vector<double>> zcoord,
          std::vector<std::vector<double>> ycoord, double toler, int numeach);

      /*!
      \brief Check whether the particular z-plane of the volumecell contains significant area so as
      to distribute the Gauss points in that plane
      */
      bool is_contain_area(double minn[3], double maxx[3], double& zmin,
          std::vector<std::vector<double>>& pts, std::vector<std::vector<double>> zcoord,
          std::vector<std::vector<double>> ycoord, double toler, int numeach);

      /*!
      \brief Writes the Geometry of volumecell and location of Gauss points in GMSH format output
      file
      */
      void gauss_point_gmsh();

      /*!
      \brief Generates equally spaced "num" number of points on the line whose end points are
      specified by inter1 and inter2
      */
      void on_line(std::vector<double> inter1, std::vector<double> inter2,
          std::vector<std::vector<double>>& linePts, int num);

      /*!
      \brief Store the z- and y-coordinates of the all corner points which will be used to find
      whether the intersection point lies inside the volume or not
      */
      void get_zcoordinates(
          std::vector<std::vector<double>>& zcoord, std::vector<std::vector<double>>& ycoord);

      /*
      \brief Check whether the intersection point, which is in the plane containing the facet,
      actually lies with in the facet area
      */
      int pnpoly(int npol, std::vector<double> xp, std::vector<double> yp, double x, double y);

      /*
      \brief Check whether the intersection point, which is in the plane containing the facet,
      actually lies with in the facet area
      */
      int pnpoly(const std::vector<std::vector<double>>& xp, const Core::LinAlg::Matrix<3, 1>& pt,
          Core::Geo::Cut::ProjectionDirection projType);

      /*!
      \brief Adds linear combination of first order base function to the moment fitting system of
      equations
      */
      void first_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Adds linear combination of second order base function to the moment fitting system of
      equations
      */
      void second_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Adds linear combination of third order base function to the moment fitting system of
      equations
      */
      void third_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Adds linear combination of fourth order base function to the moment fitting system of
      equations
      */
      void fourth_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Adds linear combination of fifth order base function to the moment fitting system of
      equations
      */
      void fifth_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Adds linear combination of sixth order base function to the moment fitting system of
      equations
      */
      void sixth_order_additional_terms(
          std::vector<std::vector<double>>& mat, Core::LinAlg::SerialDenseVector& rhs);

      /*!
      \brief Computes the error introduced by the generated integration rule for integrating some
      specific functions
      */
      void error_for_specific_function(Core::LinAlg::SerialDenseVector rhs_moment,
          Core::LinAlg::SerialDenseVector weights, int numeach);

      //! considered volumecell
      VolumeCell* volcell_;

      //! background element that contains this volumecell
      Element* elem1_;

      //! position (inside or outside) of volumecell
      const Core::Geo::Cut::Point::PointPosition position_;

      //! defines the base function to be integrated
      int num_func_;

      //! position of Gauss points
      std::vector<std::vector<double>> gaus_pts_;

      //! equations of plane in which facets of volumecell are contained
      std::vector<std::vector<double>> eqn_facets_;
    };
  }  // namespace Cut
}  // namespace Core::Geo

FOUR_C_NAMESPACE_CLOSE

#endif