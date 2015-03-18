/*----------------------------------------------------------------------*/
/*!
 \file scatra_ele_calc_fwd.hpp

 \brief
 forward declarations for scatra_ele_calc classes

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15251
 </pre>
 *----------------------------------------------------------------------*/


// 1D elements
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line2,1>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line2,2>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line2,3>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line3,1>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line3,2>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::line3,3>;

// 2D elements
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::tri3>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::tri6>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::quad4,2>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::quad4,3>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::quad8>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::quad9,2>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::quad9,3>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::nurbs9,2>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::nurbs9,3>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::hex8,3>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::hex20>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::hex27,3>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::tet4,3>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::tet10,3>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::wedge6>;
template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::pyramid5,3>;
//template class DRT::ELEMENTS::ScaTraEleCalc<DRT::Element::nurbs27>;
