/*!----------------------------------------------------------------------
\file drt_utils_integration.cpp

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <math.h>
#include <iostream>

#include "drt_utils_integration.H"
#include "../drt_lib/drt_dserror.H"


DRT::UTILS::IntegrationPoints3D::IntegrationPoints3D(const GaussRule3D gaussrule)
{
  const double Q12  = 1.0/2.0;
  const double Q14  = 1.0/4.0;
  const double Q16  = 1.0/6.0;
  const double Q124 = 1.0/6.0/4.0;

  switch(gaussrule)
  {
  case intrule_hex_1point:
  {
    nquad = 1;
    qxg[0][0] = 0.0;
    qxg[0][1] = 0.0;
    qxg[0][2] = 0.0;
    qwgt[0] = 8.0;
    break;
  }
  case intrule_hex_8point:
  {
    nquad = 8;
    const double xi2 = 0.5773502691896;
    qxg[0][0] = -xi2; qxg[0][1] = -xi2; qxg[0][2] = -xi2;
    qxg[1][0] =  xi2; qxg[1][1] = -xi2; qxg[1][2] = -xi2;
    qxg[2][0] =  xi2; qxg[2][1] =  xi2; qxg[2][2] = -xi2;
    qxg[3][0] = -xi2; qxg[3][1] =  xi2; qxg[3][2] = -xi2;
    qxg[4][0] = -xi2; qxg[4][1] = -xi2; qxg[4][2] =  xi2;
    qxg[5][0] =  xi2; qxg[5][1] = -xi2; qxg[5][2] =  xi2;
    qxg[6][0] =  xi2; qxg[6][1] =  xi2; qxg[6][2] =  xi2;
    qxg[7][0] = -xi2; qxg[7][1] =  xi2; qxg[7][2] =  xi2;
    qwgt[0] = 1.0;
    qwgt[1] = 1.0;
    qwgt[2] = 1.0;
    qwgt[3] = 1.0;
    qwgt[4] = 1.0;
    qwgt[5] = 1.0;
    qwgt[6] = 1.0;
    qwgt[7] = 1.0;
    break;
  }
  case intrule_hex_27point:
  {
    nquad = 27;
    const double xi3 = 0.7745966692415;
    qxg[0][0]  = -xi3; qxg[0][1]  = -xi3; qxg[0][2]  = -xi3;
    qxg[1][0]  =  0.0; qxg[1][1]  = -xi3; qxg[1][2]  = -xi3;
    qxg[2][0]  =  xi3; qxg[2][1]  = -xi3; qxg[2][2]  = -xi3;
    qxg[3][0]  = -xi3; qxg[3][1]  =  0.0; qxg[3][2]  = -xi3;
    qxg[4][0]  =  0.0; qxg[4][1]  =  0.0; qxg[4][2]  = -xi3;
    qxg[5][0]  =  xi3; qxg[5][1]  =  0.0; qxg[5][2]  = -xi3;
    qxg[6][0]  = -xi3; qxg[6][1]  =  xi3; qxg[6][2]  = -xi3;
    qxg[7][0]  =  0.0; qxg[7][1]  =  xi3; qxg[7][2]  = -xi3;
    qxg[8][0]  =  xi3; qxg[8][1]  =  xi3; qxg[8][2]  = -xi3;
    qxg[9][0]  = -xi3; qxg[9][1]  = -xi3; qxg[9][2]  =  0.0;
    qxg[10][0] =  0.0; qxg[10][1] = -xi3; qxg[10][2] =  0.0;
    qxg[11][0] =  xi3; qxg[11][1] = -xi3; qxg[11][2] =  0.0;
    qxg[12][0] = -xi3; qxg[12][1] =  0.0; qxg[12][2] =  0.0;
    qxg[13][0] =  0.0; qxg[13][1] =  0.0; qxg[13][2] =  0.0;
    qxg[14][0] =  xi3; qxg[14][1] =  0.0; qxg[14][2] =  0.0;
    qxg[15][0] = -xi3; qxg[15][1] =  xi3; qxg[15][2] =  0.0;
    qxg[16][0] =  0.0; qxg[16][1] =  xi3; qxg[16][2] =  0.0;
    qxg[17][0] =  xi3; qxg[17][1] =  xi3; qxg[17][2] =  0.0;
    qxg[18][0] = -xi3; qxg[18][1] = -xi3; qxg[18][2] =  xi3;
    qxg[19][0] =  0.0; qxg[19][1] = -xi3; qxg[19][2] =  xi3;
    qxg[20][0] =  xi3; qxg[20][1] = -xi3; qxg[20][2] =  xi3;
    qxg[21][0] = -xi3; qxg[21][1] =  0.0; qxg[21][2] =  xi3;
    qxg[22][0] =  0.0; qxg[22][1] =  0.0; qxg[22][2] =  xi3;
    qxg[23][0] =  xi3; qxg[23][1] =  0.0; qxg[23][2] =  xi3;
    qxg[24][0] = -xi3; qxg[24][1] =  xi3; qxg[24][2] =  xi3;
    qxg[25][0] =  0.0; qxg[25][1] =  xi3; qxg[25][2] =  xi3;
    qxg[26][0] =  xi3; qxg[26][1] =  xi3; qxg[26][2] =  xi3;
    const double w1 = 0.5555555555556;
    const double w2 = 0.8888888888889;
    const double w3 = w1;
    qwgt[0]  = w1*w1*w1;
    qwgt[1]  = w2*w1*w1;
    qwgt[2]  = w3*w1*w1;
    qwgt[3]  = w1*w2*w1;
    qwgt[4]  = w2*w2*w1;
    qwgt[5]  = w3*w2*w1;
    qwgt[6]  = w1*w3*w1;
    qwgt[7]  = w2*w3*w1;
    qwgt[8]  = w3*w3*w1;
    qwgt[9]  = w1*w1*w2;
    qwgt[10] = w2*w1*w2;
    qwgt[11] = w3*w1*w2;
    qwgt[12] = w1*w2*w2;
    qwgt[13] = w2*w2*w2;
    qwgt[14] = w3*w2*w2;
    qwgt[15] = w1*w3*w2;
    qwgt[16] = w2*w3*w2;
    qwgt[17] = w3*w3*w2;
    qwgt[18] = w1*w1*w3;
    qwgt[19] = w2*w1*w3;
    qwgt[20] = w3*w1*w3;
    qwgt[21] = w1*w2*w3;
    qwgt[22] = w2*w2*w3;
    qwgt[23] = w3*w2*w3;
    qwgt[24] = w1*w3*w3;
    qwgt[25] = w2*w3*w3;
    qwgt[26] = w3*w3*w3;
    break;
  }
  case intrule_tet_1point:
  {
    nquad = 1;
    qxg[0][0] =  Q14 ;
    qxg[0][1] =  Q14 ;
    qxg[0][2] =  Q14 ;
    qwgt[0]   =  Q16 ;
    break;
  }
  case intrule_tet_4point:
  {
    nquad = 4;
    const double palpha = (5.0 + 3.0*sqrt(5.0))/20.0;
    const double pbeta  = (5.0 - sqrt(5.0))/20.0;
    qxg[0][0] =  pbeta ;  qxg[0][1] =  pbeta ;  qxg[0][2] =  pbeta ;
    qxg[1][0] =  palpha;  qxg[1][1] =  pbeta ;  qxg[1][2] =  pbeta ;
    qxg[2][0] =  pbeta ;  qxg[2][1] =  palpha;  qxg[2][2] =  pbeta ;
    qxg[3][0] =  pbeta ;  qxg[3][1] =  pbeta ;  qxg[3][2] =  palpha;
    qwgt[0]   =  Q124  ;
    qwgt[1]   =  Q124  ;
    qwgt[2]   =  Q124  ;
    qwgt[3]   =  Q124  ;
    break;
  }
  case intrule_tet_4point_gauss_radau:
  {
    nquad = 4;
    qxg[0][0] = 0.0;
    qxg[1][0] = 1.0;
    qxg[2][0] = 0.0;
    qxg[3][0] = 0.0;
    qxg[0][1] = 0.0;
    qxg[1][1] = 0.0;
    qxg[2][1] = 1.0;
    qxg[3][1] = 0.0;
    qxg[0][2] = 0.0;
    qxg[1][2] = 0.0;
    qxg[2][2] = 0.0;
    qxg[3][2] = 1.0;
    qwgt[0]   = Q124;
    qwgt[1]   = Q124;
    qwgt[2]   = Q124;
    qwgt[3]   = Q124;
    break;
  }
  case intrule_tet_5point:
  {
    nquad = 5;
    qxg[0][0] = Q14;   qxg[0][1] = Q14;   qxg[0][2] = Q14;
    qxg[1][0] = Q12;   qxg[1][1] = Q16;   qxg[1][2] = Q16;
    qxg[2][0] = Q16;   qxg[2][1] = Q16;   qxg[2][2] = Q16;
    qxg[3][0] = Q16;   qxg[3][1] = Q16;   qxg[3][2] = Q12;
    qxg[4][0] = Q16;   qxg[4][1] = Q12;   qxg[4][2] = Q16;
    const double Q430 = 4.0/5.0/6.0;
    const double Q9120= 9.0/4.0/5.0/6.0;
    qwgt[0]   =    -Q430 ;
    qwgt[1]   =     Q9120;
    qwgt[2]   =     Q9120;
    qwgt[3]   =     Q9120;
    qwgt[4]   =     Q9120;
    break;
  }
  case intrule_tet_10point:
  {
    nquad = 10;
    qxg[0][0] = 0.5684305841968444;   qxg[0][1] = 0.1438564719343852;   qxg[0][2] = 0.1438564719343852;
    qxg[1][0] = 0.1438564719343852;   qxg[1][1] = 0.1438564719343852;   qxg[1][2] = 0.1438564719343852;
    qxg[2][0] = 0.1438564719343852;   qxg[2][1] = 0.1438564719343852;   qxg[2][2] = 0.5684305841968444;
    qxg[3][0] = 0.1438564719343852;   qxg[3][1] = 0.5684305841968444;   qxg[3][2] = 0.1438564719343852;
    qxg[4][0] = 0.0000000000000000;   qxg[4][1] = 0.5000000000000000;   qxg[4][2] = 0.5000000000000000;
    qxg[5][0] = 0.5000000000000000;   qxg[5][1] = 0.0000000000000000;   qxg[5][2] = 0.5000000000000000;
    qxg[6][0] = 0.5000000000000000;   qxg[6][1] = 0.5000000000000000;   qxg[6][2] = 0.0000000000000000;
    qxg[7][0] = 0.5000000000000000;   qxg[7][1] = 0.0000000000000000;   qxg[7][2] = 0.0000000000000000;
    qxg[8][0] = 0.0000000000000000;   qxg[8][1] = 0.5000000000000000;   qxg[8][2] = 0.0000000000000000;
    qxg[9][0] = 0.0000000000000000;   qxg[9][1] = 0.0000000000000000;   qxg[9][2] = 0.5000000000000000;
    qwgt[0]   =     0.2177650698804054*Q16;
    qwgt[1]   =     0.2177650698804054*Q16;
    qwgt[2]   =     0.2177650698804054*Q16;
    qwgt[3]   =     0.2177650698804054*Q16;
    qwgt[4]   =     0.0214899534130631*Q16;
    qwgt[5]   =     0.0214899534130631*Q16;
    qwgt[6]   =     0.0214899534130631*Q16;
    qwgt[7]   =     0.0214899534130631*Q16;
    qwgt[8]   =     0.0214899534130631*Q16;
    qwgt[9]   =     0.0214899534130631*Q16;
    break;
  }
  case intrule_tet_11point:
  {
    nquad = 11;
    qxg[ 0][0] = 0.2500000000000000;   qxg[ 0][1] = 0.2500000000000000;   qxg[ 0][2] = 0.2500000000000000;
    qxg[ 1][0] = 0.7857142857142857;   qxg[ 1][1] = 0.0714285714285714;   qxg[ 1][2] = 0.0714285714285714;
    qxg[ 2][0] = 0.0714285714285714;   qxg[ 2][1] = 0.0714285714285714;   qxg[ 2][2] = 0.0714285714285714;
    qxg[ 3][0] = 0.0714285714285714;   qxg[ 3][1] = 0.0714285714285714;   qxg[ 3][2] = 0.7857142857142857;
    qxg[ 4][0] = 0.0714285714285714;   qxg[ 4][1] = 0.7857142857142857;   qxg[ 4][2] = 0.0714285714285714;
    qxg[ 5][0] = 0.1005964238332008;   qxg[ 5][1] = 0.3994035761667992;   qxg[ 5][2] = 0.3994035761667992;
    qxg[ 6][0] = 0.3994035761667992;   qxg[ 6][1] = 0.1005964238332008;   qxg[ 6][2] = 0.3994035761667992;
    qxg[ 7][0] = 0.3994035761667992;   qxg[ 7][1] = 0.3994035761667992;   qxg[ 7][2] = 0.1005964238332008;
    qxg[ 8][0] = 0.3994035761667992;   qxg[ 8][1] = 0.1005964238332008;   qxg[ 8][2] = 0.1005964238332008;
    qxg[ 9][0] = 0.1005964238332008;   qxg[ 9][1] = 0.3994035761667992;   qxg[ 9][2] = 0.1005964238332008;
    qxg[10][0] = 0.1005964238332008;   qxg[10][1] = 0.1005964238332008;   qxg[10][2] = 0.3994035761667992;
    qwgt[ 0]   =     0.2177650698804054*Q16;
    qwgt[ 1]   =     0.2177650698804054*Q16;
    qwgt[ 2]   =     0.2177650698804054*Q16;
    qwgt[ 3]   =     0.2177650698804054*Q16;
    qwgt[ 4]   =     0.0214899534130631*Q16;
    qwgt[ 5]   =     0.0214899534130631*Q16;
    qwgt[ 6]   =     0.0214899534130631*Q16;
    qwgt[ 7]   =     0.0214899534130631*Q16;
    qwgt[ 8]   =     0.0214899534130631*Q16;
    qwgt[ 9]   =     0.0214899534130631*Q16;
    qwgt[10]   =     0.0214899534130631*Q16;
    break;
  }
  case intrule_tet_24point:
  {
      nquad = 24;
      qxg[ 0][0] = 0.3561913862225449;  qxg[ 0][1] = 0.2146028712591517;  qxg[ 0][2] = 0.2146028712591517;
      qxg[ 1][0] = 0.2146028712591517;  qxg[ 1][1] = 0.2146028712591517;  qxg[ 1][2] = 0.2146028712591517;
      qxg[ 2][0] = 0.2146028712591517;  qxg[ 2][1] = 0.2146028712591517;  qxg[ 2][2] = 0.3561913862225449;
      qxg[ 3][0] = 0.2146028712591517;  qxg[ 3][1] = 0.3561913862225449;  qxg[ 3][2] = 0.2146028712591517;
      qxg[ 4][0] = 0.8779781243961660;  qxg[ 4][1] = 0.0406739585346113;  qxg[ 4][2] = 0.0406739585346113;
      qxg[ 5][0] = 0.0406739585346113;  qxg[ 5][1] = 0.0406739585346113;  qxg[ 5][2] = 0.0406739585346113;
      qxg[ 6][0] = 0.0406739585346113;  qxg[ 6][1] = 0.0406739585346113;  qxg[ 6][2] = 0.8779781243961660;
      qxg[ 7][0] = 0.0406739585346113;  qxg[ 7][1] = 0.8779781243961660;  qxg[ 7][2] = 0.0406739585346113;
      qxg[ 8][0] = 0.0329863295731731;  qxg[ 8][1] = 0.3223378901422757;  qxg[ 8][2] = 0.3223378901422757;
      qxg[ 9][0] = 0.3223378901422757;  qxg[ 9][1] = 0.3223378901422757;  qxg[ 9][2] = 0.3223378901422757;
      qxg[10][0] = 0.3223378901422757;  qxg[10][1] = 0.3223378901422757;  qxg[10][2] = 0.0329863295731731;
      qxg[11][0] = 0.3223378901422757;  qxg[11][1] = 0.0329863295731731;  qxg[11][2] = 0.3223378901422757;
      qxg[12][0] = 0.2696723314583159;  qxg[12][1] = 0.0636610018750175;  qxg[12][2] = 0.0636610018750175;
      qxg[13][0] = 0.0636610018750175;  qxg[13][1] = 0.2696723314583159;  qxg[13][2] = 0.0636610018750175;
      qxg[14][0] = 0.0636610018750175;  qxg[14][1] = 0.0636610018750175;  qxg[14][2] = 0.2696723314583159;
      qxg[15][0] = 0.6030056647916491;  qxg[15][1] = 0.0636610018750175;  qxg[15][2] = 0.0636610018750175;
      qxg[16][0] = 0.0636610018750175;  qxg[16][1] = 0.6030056647916491;  qxg[16][2] = 0.0636610018750175;
      qxg[17][0] = 0.0636610018750175;  qxg[17][1] = 0.0636610018750175;  qxg[17][2] = 0.6030056647916491;
      qxg[18][0] = 0.0636610018750175;  qxg[18][1] = 0.2696723314583159;  qxg[18][2] = 0.6030056647916491;
      qxg[19][0] = 0.2696723314583159;  qxg[19][1] = 0.6030056647916491;  qxg[19][2] = 0.0636610018750175;
      qxg[20][0] = 0.6030056647916491;  qxg[20][1] = 0.0636610018750175;  qxg[20][2] = 0.2696723314583159;
      qxg[21][0] = 0.0636610018750175;  qxg[21][1] = 0.6030056647916491;  qxg[21][2] = 0.2696723314583159;
      qxg[22][0] = 0.2696723314583159;  qxg[22][1] = 0.0636610018750175;  qxg[22][2] = 0.6030056647916491;
      qxg[23][0] = 0.6030056647916491;  qxg[23][1] = 0.2696723314583159;  qxg[23][2] = 0.0636610018750175;
      
      qwgt[ 0] = 0.0399227502581679*Q16;
      qwgt[ 1] = 0.0399227502581679*Q16;
      qwgt[ 2] = 0.0399227502581679*Q16;
      qwgt[ 3] = 0.0399227502581679*Q16;
      qwgt[ 4] = 0.0100772110553207*Q16;
      qwgt[ 5] = 0.0100772110553207*Q16;
      qwgt[ 6] = 0.0100772110553207*Q16;
      qwgt[ 7] = 0.0100772110553207*Q16;
      qwgt[ 8] = 0.0553571815436544*Q16;
      qwgt[ 9] = 0.0553571815436544*Q16;
      qwgt[10] = 0.0553571815436544*Q16;
      qwgt[11] = 0.0553571815436544*Q16;
      qwgt[12] = 0.0482142857142857*Q16;
      qwgt[13] = 0.0482142857142857*Q16;
      qwgt[14] = 0.0482142857142857*Q16;
      qwgt[15] = 0.0482142857142857*Q16;
      qwgt[16] = 0.0482142857142857*Q16;
      qwgt[17] = 0.0482142857142857*Q16;
      qwgt[18] = 0.0482142857142857*Q16;
      qwgt[19] = 0.0482142857142857*Q16;
      qwgt[20] = 0.0482142857142857*Q16;
      qwgt[21] = 0.0482142857142857*Q16;
      qwgt[22] = 0.0482142857142857*Q16;
      qwgt[23] = 0.0482142857142857*Q16;
      break;
  }
  case intrule_tet_45point:
  {
    nquad = 45;

    // weights
    qwgt[0] = -0.235962039848*Q16;
    qwgt[1] = 0.0244878963561*Q16;
    qwgt[2] = 0.0244878963561*Q16;
    qwgt[3] = 0.0244878963561*Q16;
    qwgt[4] = 0.0244878963561*Q16;
    qwgt[5] = 0.00394852063983*Q16;
    qwgt[6] = 0.00394852063983*Q16;
    qwgt[7] = 0.00394852063983*Q16;
    qwgt[8] = 0.00394852063983*Q16;
    qwgt[9] = 0.0263055529507*Q16;
    qwgt[10] = 0.0263055529507*Q16;
    qwgt[11] = 0.0263055529507*Q16;
    qwgt[12] = 0.0263055529507*Q16;
    qwgt[13] = 0.0263055529507*Q16;
    qwgt[14] = 0.0263055529507*Q16;
    qwgt[15] = 0.0829803830551*Q16;
    qwgt[16] = 0.0829803830551*Q16;
    qwgt[17] = 0.0829803830551*Q16;
    qwgt[18] = 0.0829803830551*Q16;
    qwgt[19] = 0.0829803830551*Q16;
    qwgt[20] = 0.0829803830551*Q16;
    qwgt[21] = 0.0254426245481*Q16;
    qwgt[22] = 0.0254426245481*Q16;
    qwgt[23] = 0.0254426245481*Q16;
    qwgt[24] = 0.0254426245481*Q16;
    qwgt[25] = 0.0254426245481*Q16;
    qwgt[26] = 0.0254426245481*Q16;
    qwgt[27] = 0.0254426245481*Q16;
    qwgt[28] = 0.0254426245481*Q16;
    qwgt[29] = 0.0254426245481*Q16;
    qwgt[30] = 0.0254426245481*Q16;
    qwgt[31] = 0.0254426245481*Q16;
    qwgt[32] = 0.0254426245481*Q16;
    qwgt[33] = 0.0134324384377*Q16;
    qwgt[34] = 0.0134324384377*Q16;
    qwgt[35] = 0.0134324384377*Q16;
    qwgt[36] = 0.0134324384377*Q16;
    qwgt[37] = 0.0134324384377*Q16;
    qwgt[38] = 0.0134324384377*Q16;
    qwgt[39] = 0.0134324384377*Q16;
    qwgt[40] = 0.0134324384377*Q16;
    qwgt[41] = 0.0134324384377*Q16;
    qwgt[42] = 0.0134324384377*Q16;
    qwgt[43] = 0.0134324384377*Q16;
    qwgt[44] = 0.0134324384377*Q16;

    // positions
    qxg[0][0] = 0.25;
    qxg[0][1] = 0.25;
    qxg[0][2] = 0.25;

    qxg[1][0] = 0.6175871903;
    qxg[1][1] = 0.127470936567;
    qxg[1][2] = 0.127470936567;

    qxg[2][0] = 0.127470936567;
    qxg[2][1] = 0.127470936567;
    qxg[2][2] = 0.127470936567;

    qxg[3][0] = 0.127470936567;
    qxg[3][1] = 0.127470936567;
    qxg[3][2] = 0.6175871903;

    qxg[4][0] = 0.127470936567;
    qxg[4][1] = 0.6175871903;
    qxg[4][2] = 0.127470936567;

    qxg[5][0] = 0.903763508822;
    qxg[5][1] = 0.0320788303926;
    qxg[5][2] = 0.0320788303926;

    qxg[6][0] = 0.0320788303926;
    qxg[6][1] = 0.0320788303926;
    qxg[6][2] = 0.0320788303926;

    qxg[7][0] = 0.0320788303926;
    qxg[7][1] = 0.0320788303926;
    qxg[7][2] = 0.903763508822;

    qxg[8][0] = 0.0320788303926;
    qxg[8][1] = 0.903763508822;
    qxg[8][2] = 0.0320788303926;

    qxg[9][0] = 0.450222904357;
    qxg[9][1] = 0.0497770956433;
    qxg[9][2] = 0.0497770956433;

    qxg[10][0] = 0.0497770956433;
    qxg[10][1] = 0.450222904357;
    qxg[10][2] = 0.0497770956433;

    qxg[11][0] = 0.0497770956433;
    qxg[11][1] = 0.0497770956433;
    qxg[11][2] = 0.450222904357;

    qxg[12][0] = 0.0497770956433;
    qxg[12][1] = 0.450222904357;
    qxg[12][2] = 0.450222904357;

    qxg[13][0] = 0.450222904357;
    qxg[13][1] = 0.0497770956433;
    qxg[13][2] = 0.450222904357;

    qxg[14][0] = 0.450222904357;
    qxg[14][1] = 0.450222904357;
    qxg[14][2] = 0.0497770956433;

    qxg[15][0] = 0.316269552601;
    qxg[15][1] = 0.183730447399;
    qxg[15][2] = 0.183730447399;

    qxg[16][0] = 0.183730447399;
    qxg[16][1] = 0.316269552601;
    qxg[16][2] = 0.183730447399;

    qxg[17][0] = 0.183730447399;
    qxg[17][1] = 0.183730447399;
    qxg[17][2] = 0.316269552601;

    qxg[18][0] = 0.183730447399;
    qxg[18][1] = 0.316269552601;
    qxg[18][2] = 0.316269552601;

    qxg[19][0] = 0.316269552601;
    qxg[19][1] = 0.183730447399;
    qxg[19][2] = 0.316269552601;

    qxg[20][0] = 0.316269552601;
    qxg[20][1] = 0.316269552601;
    qxg[20][2] = 0.183730447399;

    qxg[21][0] = 0.0229177878448;
    qxg[21][1] = 0.231901089397;
    qxg[21][2] = 0.231901089397;

    qxg[22][0] = 0.231901089397;
    qxg[22][1] = 0.0229177878448;
    qxg[22][2] = 0.231901089397;

    qxg[23][0] = 0.231901089397;
    qxg[23][1] = 0.231901089397;
    qxg[23][2] = 0.0229177878448;

    qxg[24][0] = 0.513280033361;
    qxg[24][1] = 0.231901089397;
    qxg[24][2] = 0.231901089397;

    qxg[25][0] = 0.231901089397;
    qxg[25][1] = 0.513280033361;
    qxg[25][2] = 0.231901089397;

    qxg[26][0] = 0.231901089397;
    qxg[26][1] = 0.231901089397;
    qxg[26][2] = 0.513280033361;

    qxg[27][0] = 0.231901089397;
    qxg[27][1] = 0.0229177878448;
    qxg[27][2] = 0.513280033361;

    qxg[28][0] = 0.0229177878448;
    qxg[28][1] = 0.513280033361;
    qxg[28][2] = 0.231901089397;

    qxg[29][0] = 0.513280033361;
    qxg[29][1] = 0.231901089397;
    qxg[29][2] = 0.0229177878448;

    qxg[30][0] = 0.231901089397;
    qxg[30][1] = 0.513280033361;
    qxg[30][2] = 0.0229177878448;

    qxg[31][0] = 0.0229177878448;
    qxg[31][1] = 0.231901089397;
    qxg[31][2] = 0.513280033361;

    qxg[32][0] = 0.513280033361;
    qxg[32][1] = 0.0229177878448;
    qxg[32][2] = 0.231901089397;

    qxg[33][0] = 0.730313427808;
    qxg[33][1] = 0.0379700484718;
    qxg[33][2] = 0.0379700484718;

    qxg[34][0] = 0.0379700484718;
    qxg[34][1] = 0.730313427808;
    qxg[34][2] = 0.0379700484718;

    qxg[35][0] = 0.0379700484718;
    qxg[35][1] = 0.0379700484718;
    qxg[35][2] = 0.730313427808;

    qxg[36][0] = 0.193746475249;
    qxg[36][1] = 0.0379700484718;
    qxg[36][2] = 0.0379700484718;

    qxg[37][0] = 0.0379700484718;
    qxg[37][1] = 0.193746475249;
    qxg[37][2] = 0.0379700484718;

    qxg[38][0] = 0.0379700484718;
    qxg[38][1] = 0.0379700484718;
    qxg[38][2] = 0.193746475249;

    qxg[39][0] = 0.0379700484718;
    qxg[39][1] = 0.730313427808;
    qxg[39][2] = 0.193746475249;

    qxg[40][0] = 0.730313427808;
    qxg[40][1] = 0.193746475249;
    qxg[40][2] = 0.0379700484718;

    qxg[41][0] = 0.193746475249;
    qxg[41][1] = 0.0379700484718;
    qxg[41][2] = 0.730313427808;

    qxg[42][0] = 0.0379700484718;
    qxg[42][1] = 0.193746475249;
    qxg[42][2] = 0.730313427808;

    qxg[43][0] = 0.730313427808;
    qxg[43][1] = 0.0379700484718;
    qxg[43][2] = 0.193746475249;

    qxg[44][0] = 0.193746475249;
    qxg[44][1] = 0.730313427808;
    qxg[44][2] = 0.0379700484718;

    break;
  }
  case intrule_wedge_1point:
    {
      const double Q13 = 1.0/3.0;
      nquad = 1;
      qxg[0][0] = Q13;
      qxg[0][1] = Q13;
      qxg[0][2] = 0.0;
      qwgt[0] = 1;
      break;
    }

  case intrule_wedge_6point:
  {
    const double xi3 = 1.0/sqrt(3.0);
    const double Q23 = 2.0/3.0;
    const double Q16 = 1.0/6.0;

    nquad = 6;
    qxg[0][0] = Q23;
    qxg[1][0] = Q16;
    qxg[2][0] = Q16;
    qxg[3][0] = Q23;
    qxg[4][0] = Q16;
    qxg[5][0] = Q16;
    qxg[0][1] = Q16;
    qxg[1][1] = Q23;
    qxg[2][1] = Q16;
    qxg[3][1] = Q16;
    qxg[4][1] = Q23;
    qxg[5][1] = Q16;
    qxg[0][2] = xi3;
    qxg[1][2] = xi3;
    qxg[2][2] = xi3;
    qxg[3][2] = -xi3;
    qxg[4][2] = -xi3;
    qxg[5][2] = -xi3;

    qwgt[0] = Q16;
    qwgt[1] = Q16;
    qwgt[2] = Q16;
    qwgt[3] = Q16;
    qwgt[4] = Q16;
    qwgt[5] = Q16;
    break;
  }

  case intrule_wedge_9point:
  {
    std::cout << "Sorry if I interrupt your work, but I think this rule is not sufficient for a wedge15 element. ";
    std::cout << "In my opinion, you need 18 integration points here. If you are sure, go ahead, otherwise ask me or implement the 18 point rule. Axel (06.06.08)\n";
    dserror("stopped code due to believed insufficient integration rule 'intrule_wedge_9point' for wedge15. Axel");
      
    const double xi3 = 0.77459666924;
    const double Q23 = 2.0/3.0;
    const double Q5913 = 5.0/9.0*1.0/3.0;

    nquad = 9;
    qxg[0][0] = Q23;
    qxg[1][0] = Q16;
    qxg[2][0] = Q16;
    qxg[3][0] = Q23;
    qxg[4][0] = Q16;
    qxg[5][0] = Q16;
    qxg[6][0] = Q23;
    qxg[7][0] = Q16;
    qxg[8][0] = Q16;
    qxg[0][1] = Q16;
    qxg[1][1] = Q23;
    qxg[2][1] = Q16;
    qxg[3][1] = Q16;
    qxg[4][1] = Q23;
    qxg[5][1] = Q16;
    qxg[6][1] = Q16;
    qxg[7][1] = Q23;
    qxg[8][1] = Q16;
    qxg[0][2] = xi3;
    qxg[1][2] = xi3;
    qxg[2][2] = xi3;
    qxg[3][2] = 0;
    qxg[4][2] = 0;
    qxg[5][2] = 0;
    qxg[6][2] = -xi3;
    qxg[7][2] = -xi3;
    qxg[8][2] = -xi3;

    qwgt[0] = Q5913;
    qwgt[1] = Q5913;
    qwgt[2] = Q5913;
    qwgt[3] = 0;
    qwgt[4] = 0;
    qwgt[5] = 0;
    qwgt[6] = -Q5913;
    qwgt[7] = -Q5913;
    qwgt[8] = -Q5913;
   break;
  }

  case intrule_pyramid_1point:
  {
    nquad = 1;
    qxg[0][0] = 0;
    qxg[1][0] = 0;
    qxg[2][0] = Q14;
    qwgt[0] = 4.0/3.0;
    break;
  }

  case intrule_pyramid_8point:
  {
    nquad = 8;
    qxg[0][0] = -0.26318405556971;
    qxg[1][0] = -0.50661630334979;
    qxg[2][0] = -0.26318405556971;
    qxg[3][0] = -0.50661630334979;
    qxg[4][0] = 0.26318405556971;
    qxg[5][0] = 0.50661630334979;
    qxg[6][0] = 0.26318405556971;
    qxg[7][0] = 0.50661630334979;
    qxg[0][1] = -0.26318405556971;
    qxg[1][1] = -0.50661630334979;
    qxg[2][1] = 0.26318405556971;
    qxg[3][1] = 0.50661630334979;
    qxg[4][1] = -0.26318405556971;
    qxg[5][1] = -0.50661630334979;
    qxg[6][1] = 0.26318405556971;
    qxg[7][1] = 0.50661630334979;
    qxg[0][2] = 0.54415184401122;
    qxg[1][2] = 0.12251482265544;
    qxg[2][2] = 0.54415184401122;
    qxg[3][2] = 0.12251482265544;
    qxg[4][2] = 0.54415184401122;
    qxg[5][2] = 0.12251482265544;
    qxg[6][2] = 0.54415184401122;
    qxg[7][2] = 0.12251482265544;


    qwgt[0] = 0.10078588207983;
    qwgt[1] = 0.23254745125351;
    qwgt[2] = 0.10078588207983;
    qwgt[3] = 0.23254745125351;
    qwgt[4] = 0.10078588207983;
    qwgt[5] = 0.23254745125351;
    qwgt[6] = 0.10078588207983;
    qwgt[7] = 0.23254745125351;
    break;
  }

  default:
    dserror("unknown 3D integration rule");
  }
}


DRT::UTILS::IntegrationPoints2D::IntegrationPoints2D(const GaussRule2D gaussrule)
{
  switch(gaussrule)
  {
  case intrule_quad_1point :
  {
    nquad = 1;
    qwgt[0]  =  4.0;
    qxg[0][0] =  0.0;
    qxg[0][1] =  0.0;
    break;
  }
  case intrule_quad_4point :
  {
    // numbering is consistent with GiD definition
    //
    //                ^ xi_2
    //     _ _ _ _ _ _|_ _ _ _ _ _
    //    |           |           |
    //    |           |           |
    //    |    4      |     3     |
    //    |           |           |
    //    |           |           |
    //    |           -----------------> xi_1
    //    |                       |
    //    |                       |
    //    |    1            2     |
    //    |                       |
    //    |_ _ _ _ _ _ _ _ _ _ _ _|
    //    
    nquad = 4;
    qwgt[0]  =  1.0;
    qwgt[1]  =  1.0;
    qwgt[2]  =  1.0;
    qwgt[3]  =  1.0;

    qxg[0][0] = -0.5773502691896;
    qxg[0][1] = -0.5773502691896;
    qxg[1][0] =  0.5773502691896;
    qxg[1][1] = -0.5773502691896;
    qxg[2][0] =  0.5773502691896;
    qxg[2][1] =  0.5773502691896;
    qxg[3][0] = -0.5773502691896;
    qxg[3][1] =  0.5773502691896;
    break;
  }
  case intrule_quad_9point:
  {
    // numbering is consistent with GiD definition
    //
    //                ^ xi_2
    //     _ _ _ _ _ _|_ _ _ _ _ _
    //    |           |           |
    //    |           |           |
    //    |    4      7     3     |
    //    |           |           |
    //    |           |           |
    //    |    8      9-----6----------> xi_1
    //    |                       |
    //    |                       |
    //    |    1      5     2     |
    //    |                       |
    //    |_ _ _ _ _ _ _ _ _ _ _ _|
    
    nquad = 9;
    qwgt[0]  =  0.5555555555556*0.5555555555556;
    qwgt[1]  =  0.5555555555556*0.5555555555556;
    qwgt[2]  =  0.5555555555556*0.5555555555556;
    qwgt[3]  =  0.5555555555556*0.5555555555556;
    qwgt[4]  =  0.8888888888889*0.5555555555556;
    qwgt[5]  =  0.5555555555556*0.8888888888889;
    qwgt[6]  =  0.8888888888889*0.5555555555556;
    qwgt[7]  =  0.5555555555556*0.8888888888889;
    qwgt[8]  =  0.8888888888889*0.8888888888889;
   
    qxg[0][0] = -0.7745966692415;
    qxg[0][1] = -0.7745966692415;
    qxg[1][0] =  0.7745966692415;
    qxg[1][1] = -0.7745966692415;
    qxg[2][0] =  0.7745966692415;
    qxg[2][1] =  0.7745966692415;
    qxg[3][0] = -0.7745966692415;
    qxg[3][1] =  0.7745966692415;
    qxg[4][0] =  0.0;
    qxg[4][1] = -0.7745966692415;
    qxg[5][0] =  0.7745966692415;
    qxg[5][1] =  0.0;
    qxg[6][0] =  0.0;
    qxg[6][1] =  0.7745966692415;
    qxg[7][0] = -0.7745966692415;
    qxg[7][1] =  0.0;
    qxg[8][0] =  0.0;
    qxg[8][1] =  0.0;
    
    break;
  }
  case intrule_tri_1point:
  {
    nquad = 1;
    qwgt[0]  = 0.5;

    qxg[0][0] = 1.0/3.0;
    qxg[0][1] = 1.0/3.0;
    break;
  }
  case intrule_tri_3point_gauss_radau:
  {
    nquad = 3;
    qwgt[0]  = 1.0/6.0 ;
    qwgt[1]  = 1.0/6.0 ;
    qwgt[2]  = 1.0/6.0 ;

    qxg[0][0] = 0.5;
    qxg[0][1] = 0.0;
    qxg[1][0] = 0.5;
    qxg[1][1] = 0.5;
    qxg[2][0] = 0.0;
    qxg[2][1] = 0.5;
    break;
  }
  case intrule_tri_3point:
  {
    nquad = 3;
    qwgt[0]  = 1.0/6.0 ;
    qwgt[1]  = 1.0/6.0 ;
    qwgt[2]  = 1.0/6.0 ;

    qxg[0][0] = 1.0/6.0;
    qxg[0][1] = 1.0/6.0;
    qxg[1][0] = 2.0/3.0;
    qxg[1][1] = 1.0/6.0;
    qxg[2][0] = 1.0/6.0;
    qxg[2][1] = 2.0/3.0;
    break;
  }
  case intrule_tri_6point:
  {
    nquad = 6;
    qwgt[0]  = 0.0549758718277;
    qwgt[1]  = 0.0549758718277;
    qwgt[2]  = 0.0549758718277;
    qwgt[3]  = 0.1116907948390;
    qwgt[4]  = 0.1116907948390;
    qwgt[5]  = 0.1116907948390;

    qxg[0][0] = 0.0915762135098;
    qxg[0][1] = 0.0915762135098;
    qxg[1][0] = 0.8168475729805;
    qxg[1][1] = 0.0915762135098;
    qxg[2][0] = 0.0915762135098;
    qxg[2][1] = 0.8168475729805;
    qxg[3][0] = 0.4459484909160;
    qxg[3][1] = 0.1081030181681;
    qxg[4][0] = 0.4459484909160;
    qxg[4][1] = 0.4459484909160;
    qxg[5][0] = 0.1081030181681;
    qxg[5][1] = 0.4459484909160;
    break;
  }
  case intrule_tri_7point:
  {
    nquad = 7;

    // weights
    qwgt[0] = 0.225;
    qwgt[1] = 0.125939180545;
    qwgt[2] = 0.125939180545;
    qwgt[3] = 0.125939180545;
    qwgt[4] = 0.132394152789;
    qwgt[5] = 0.132394152789;
    qwgt[6] = 0.132394152789;

    // positions
    qxg[0][0] = 0.333333333333;
    qxg[0][1] = 0.333333333333;

    qxg[1][0] = 0.797426985353;
    qxg[1][1] = 0.101286507323;

    qxg[2][0] = 0.101286507323;
    qxg[2][1] = 0.797426985353;

    qxg[3][0] = 0.101286507323;
    qxg[3][1] = 0.101286507323;

    qxg[4][0] = 0.0597158717898;
    qxg[4][1] = 0.470142064105;

    qxg[5][0] = 0.470142064105;
    qxg[5][1] = 0.0597158717898;

    qxg[6][0] = 0.470142064105;
    qxg[6][1] = 0.470142064105;
    break;
  }
  case intrule_tri_12point:
  {
    nquad = 12;

    // weights
    qwgt[0] = 0.0508449063702;
    qwgt[1] = 0.0508449063702;
    qwgt[2] = 0.0508449063702;
    qwgt[3] = 0.116786275726;
    qwgt[4] = 0.116786275726;
    qwgt[5] = 0.116786275726;
    qwgt[6] = 0.0828510756184;
    qwgt[7] = 0.0828510756184;
    qwgt[8] = 0.0828510756184;
    qwgt[9] = 0.0828510756184;
    qwgt[10] = 0.0828510756184;
    qwgt[11] = 0.0828510756184;

    // positions
    qxg[0][0] = 0.873821971017;
    qxg[0][1] = 0.0630890144915;

    qxg[1][0] = 0.0630890144915;
    qxg[1][1] = 0.873821971017;

    qxg[2][0] = 0.0630890144915;
    qxg[2][1] = 0.0630890144915;

    qxg[3][0] = 0.501426509658;
    qxg[3][1] = 0.249286745171;

    qxg[4][0] = 0.249286745171;
    qxg[4][1] = 0.501426509658;

    qxg[5][0] = 0.249286745171;
    qxg[5][1] = 0.249286745171;

    qxg[6][0] = 0.636502499121;
    qxg[6][1] = 0.310352451034;

    qxg[7][0] = 0.636502499121;
    qxg[7][1] = 0.0531450498448;

    qxg[8][0] = 0.310352451034;
    qxg[8][1] = 0.636502499121;

    qxg[9][0] = 0.310352451034;
    qxg[9][1] = 0.0531450498448;

    qxg[10][0] = 0.0531450498448;
    qxg[10][1] = 0.636502499121;

    qxg[11][0] = 0.0531450498448;
    qxg[11][1] = 0.310352451034;

    break;
  }
  case intrule_tri_37point:
  {
    const double q12 = 0.5;
    nquad = 37;
    qwgt[0] = q12*0.0517397660657;
    qwgt[1] = q12*0.00800779955556;
    qwgt[2] = q12*0.00800779955556;
    qwgt[3] = q12*0.00800779955556;
    qwgt[4] = q12*0.0468688989818;
    qwgt[5] = q12*0.0468688989818;
    qwgt[6] = q12*0.0468688989818;
    qwgt[7] = q12*0.046590940184;
    qwgt[8] = q12*0.046590940184;
    qwgt[9] = q12*0.046590940184;
    qwgt[10] = q12*0.0310169433138;
    qwgt[11] = q12*0.0310169433138;
    qwgt[12] = q12*0.0310169433138;
    qwgt[13] = q12*0.0107916127366;
    qwgt[14] = q12*0.0107916127366;
    qwgt[15] = q12*0.0107916127366;
    qwgt[16] = q12*0.0321955342424;
    qwgt[17] = q12*0.0321955342424;
    qwgt[18] = q12*0.0321955342424;
    qwgt[19] = q12*0.0154458342107;
    qwgt[20] = q12*0.0154458342107;
    qwgt[21] = q12*0.0154458342107;
    qwgt[22] = q12*0.0154458342107;
    qwgt[23] = q12*0.0154458342107;
    qwgt[24] = q12*0.0154458342107;
    qwgt[25] = q12*0.0178229899232;
    qwgt[26] = q12*0.0178229899232;
    qwgt[27] = q12*0.0178229899232;
    qwgt[28] = q12*0.0178229899232;
    qwgt[29] = q12*0.0178229899232;
    qwgt[30] = q12*0.0178229899232;
    qwgt[31] = q12*0.0370386836814;
    qwgt[32] = q12*0.0370386836814;
    qwgt[33] = q12*0.0370386836814;
    qwgt[34] = q12*0.0370386836814;
    qwgt[35] = q12*0.0370386836814;
    qwgt[36] = q12*0.0370386836814;
    
    
    qxg[0][0] = 0.333333333333;
    qxg[0][1] = 0.333333333333;

    qxg[1][0] = 0.950275662924;
    qxg[1][1] = 0.0248621685379;

    qxg[2][0] = 0.0248621685379;
    qxg[2][1] = 0.950275662924;

    qxg[3][0] = 0.0248621685379;
    qxg[3][1] = 0.0248621685379;

    qxg[4][0] = 0.171614914924;
    qxg[4][1] = 0.414192542538;

    qxg[5][0] = 0.414192542538;
    qxg[5][1] = 0.171614914924;

    qxg[6][0] = 0.414192542538;
    qxg[6][1] = 0.414192542538;

    qxg[7][0] = 0.539412243677;
    qxg[7][1] = 0.230293878161;

    qxg[8][0] = 0.230293878161;
    qxg[8][1] = 0.539412243677;

    qxg[9][0] = 0.230293878161;
    qxg[9][1] = 0.230293878161;

    qxg[10][0] = 0.772160036677;
    qxg[10][1] = 0.113919981662;

    qxg[11][0] = 0.113919981662;
    qxg[11][1] = 0.772160036677;

    qxg[12][0] = 0.113919981662;
    qxg[12][1] = 0.113919981662;

    qxg[13][0] = 0.00908539994984;
    qxg[13][1] = 0.495457300025;

    qxg[14][0] = 0.495457300025;
    qxg[14][1] = 0.00908539994984;

    qxg[15][0] = 0.495457300025;
    qxg[15][1] = 0.495457300025;

    qxg[16][0] = 0.0622772903059;
    qxg[16][1] = 0.468861354847;

    qxg[17][0] = 0.468861354847;
    qxg[17][1] = 0.0622772903059;

    qxg[18][0] = 0.468861354847;
    qxg[18][1] = 0.468861354847;

    qxg[19][0] = 0.0220762896536;
    qxg[19][1] = 0.851306504174;

    qxg[20][0] = 0.0220762896536;
    qxg[20][1] = 0.126617206172;

    qxg[21][0] = 0.851306504174;
    qxg[21][1] = 0.0220762896536;

    qxg[22][0] = 0.851306504174;
    qxg[22][1] = 0.126617206172;

    qxg[23][0] = 0.126617206172;
    qxg[23][1] = 0.0220762896536;

    qxg[24][0] = 0.126617206172;
    qxg[24][1] = 0.851306504174;

    qxg[25][0] = 0.0186205228025;
    qxg[25][1] = 0.689441970729;

    qxg[26][0] = 0.0186205228025;
    qxg[26][1] = 0.291937506469;

    qxg[27][0] = 0.689441970729;
    qxg[27][1] = 0.0186205228025;

    qxg[28][0] = 0.689441970729;
    qxg[28][1] = 0.291937506469;

    qxg[29][0] = 0.291937506469;
    qxg[29][1] = 0.0186205228025;

    qxg[30][0] = 0.291937506469;
    qxg[30][1] = 0.689441970729;

    qxg[31][0] = 0.0965064812922;
    qxg[31][1] = 0.635867859434;

    qxg[32][0] = 0.0965064812922;
    qxg[32][1] = 0.267625659274;

    qxg[33][0] = 0.635867859434;
    qxg[33][1] = 0.0965064812922;

    qxg[34][0] = 0.635867859434;
    qxg[34][1] = 0.267625659274;

    qxg[35][0] = 0.267625659274;
    qxg[35][1] = 0.0965064812922;

    qxg[36][0] = 0.267625659274;
    qxg[36][1] = 0.635867859434;
    break;
  }
  case intrule2D_undefined:
  {
    dserror("trying to use uninitialised 2D gaussrule");
  }
  default:
    dserror("unknown 2D integration rule, GaussRule2D: %d",gaussrule);
  }
}


DRT::UTILS::IntegrationPoints1D::IntegrationPoints1D(const GaussRule1D gaussrule)
{
  switch(gaussrule)
  {
  case intrule_line_1point:
  {
    nquad = 1;
    qwgt[0]  =  2.0;
    qxg[0]   =  0.0;
    break;
  }
  case intrule_line_2point:
  {
    nquad = 2;
    qwgt[0]  =  1.0;
    qwgt[1]  =  1.0;
    qxg[0] = -0.5773502691896;
    qxg[1] =  0.5773502691896;
    break;
  }
  case intrule_line_3point:
  {
    nquad = 3;
    qwgt[0]  =  0.5555555555556;
    qwgt[1]  =  0.8888888888889;
    qwgt[2]  =  0.5555555555556;
    const double xi3 = 0.7745966692415;
    qxg[0] = -xi3;
    qxg[1] =  0.0;
    qxg[2] =  xi3;
    break;
  }
  case intrule_line_4point:
  {
      nquad = 4;
      qwgt[0]  =  0.3478548451375;
      qwgt[1]  =  0.6521451548625;
      qwgt[2]  =  0.6521451548625;
      qwgt[3]  =  0.3478548451375;
      
      qxg[0]   = -0.8611363115941;
      qxg[1]   = -0.3399810435849;
      qxg[2]   =  0.3399810435849;
      qxg[3]   =  0.8611363115941;
      break;
  }
  case intrule_line_5point:
  {
      nquad = 5;
      qwgt[0]  =  0.2369268850562;
      qwgt[1]  =  0.4786286704994;
      qwgt[2]  =  0.5688888888889;
      qwgt[3]  =  0.4786286704994;
      qwgt[4]  =  0.2369268850562;
        
      qxg[0]   = -0.9061798459387;
      qxg[1]   = -0.5384693101057;
      qxg[2]   =  0.0;
      qxg[3]   =  0.5384693101057;
      qxg[4]   =  0.9061798459387;
      break;
  }
  default:
    dserror("unknown 1D integration rule");
  }
}

#endif  // #ifdef CCADISCRET
