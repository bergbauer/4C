/*----------------------------------------------------------------------------*/
/*! \file
\brief shell8

\level 1


*/
/*---------------------------------------------------------------------------*/
#include "s8_shell8.h"
/*----------------------------------------------------------------------*
 |                                                        m.gee 4/03    |
 | transform covariant components of a 2. Tensor from                   |
 | curvilinear to cartesian                                             |
 | storage mode is t[e11 e12 e13 e22 e23 e33]                           |
 | Must be called with contravariant base vectors !                     |
 *----------------------------------------------------------------------*/
void s8_kov_cuca(DOUBLE *t, const DOUBLE **gkon)
{
  INT i, j;
  DOUBLE T[3][3], Tcart[3][3];
  /*
  DOUBLE c[3][3];
  */
  /*----------------------------------------------------------------------*/
  T[0][0] = t[0];
  T[1][0] = T[0][1] = t[1];
  T[2][0] = T[0][2] = t[2];
  T[1][1] = t[3];
  T[2][1] = T[1][2] = t[4];
  T[2][2] = t[5];
  /*----------------------------------------------------------------------*/
  /* theory:
  for (k=0; k<3; k++)
  for (l=0; l<3; l++)
  {
     Tcart[k][l] = 0.0;
     for (i=0; i<3; i++)
     for (j=0; j<3; j++)
        Tcart[k][l] += gkon[k][i]*gkon[l][j]*T[i][j];
  }
  */
  /*----------------------------------------------------------------------*/
  Tcart[0][0] = 0.0;
  Tcart[0][1] = 0.0;
  Tcart[0][2] = 0.0;
  Tcart[1][1] = 0.0;
  Tcart[1][2] = 0.0;
  Tcart[2][2] = 0.0;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    {
      Tcart[0][0] += gkon[0][i] * gkon[0][j] * T[i][j];
      Tcart[0][1] += gkon[0][i] * gkon[1][j] * T[i][j];
      Tcart[0][2] += gkon[0][i] * gkon[2][j] * T[i][j];
      Tcart[1][1] += gkon[1][i] * gkon[1][j] * T[i][j];
      Tcart[1][2] += gkon[1][i] * gkon[2][j] * T[i][j];
      Tcart[2][2] += gkon[2][i] * gkon[2][j] * T[i][j];
    }
  /*----------------------------------------------------------------------*/
  t[0] = Tcart[0][0];
  t[1] = Tcart[0][1];
  t[2] = Tcart[0][2];
  t[3] = Tcart[1][1];
  t[4] = Tcart[1][2];
  t[5] = Tcart[2][2];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_kov_cuca */

/*----------------------------------------------------------------------*
 |                                                        m.gee 4/03    |
 | transform contravariant components of a 2. Tensor from               |
 | cartesian to curvilinear                                             |
 | storage mode is t[e11 e12 e13 e22 e23 e33]                           |
 | Must be called with contravariant base vectors !                     |
 *----------------------------------------------------------------------*/
void s8_kon_cacu(DOUBLE *t, DOUBLE **gkon)
{
  INT i, j;
  DOUBLE T[3][3], Tcart[3][3];
  /*
  DOUBLE c[3][3];
  */
  /*----------------------------------------------------------------------*/
  Tcart[0][0] = t[0];
  Tcart[1][0] = Tcart[0][1] = t[1];
  Tcart[2][0] = Tcart[0][2] = t[2];
  Tcart[1][1] = t[3];
  Tcart[2][1] = Tcart[1][2] = t[4];
  Tcart[2][2] = t[5];
  /*----------------------------------------------------------------------*/
  /* theory:
  for (k=0; k<3; k++)
  for (l=0; l<3; l++)
  {
     T[k][l] = 0.0;
     for (i=0; i<3; i++)
     for (j=0; j<3; j++)
        T[k][l] += gkon[i][k]*gkon[j][l]*Tcart[i][j];
  }
  */
  /*----------------------------------------------------------------------*/
  T[0][0] = 0.0;
  T[0][1] = 0.0;
  T[0][2] = 0.0;
  T[1][1] = 0.0;
  T[1][2] = 0.0;
  T[2][2] = 0.0;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    {
      T[0][0] += gkon[i][0] * gkon[j][0] * Tcart[i][j];
      T[0][1] += gkon[i][0] * gkon[j][1] * Tcart[i][j];
      T[0][2] += gkon[i][0] * gkon[j][2] * Tcart[i][j];
      T[1][1] += gkon[i][1] * gkon[j][1] * Tcart[i][j];
      T[1][2] += gkon[i][1] * gkon[j][2] * Tcart[i][j];
      T[2][2] += gkon[i][2] * gkon[j][2] * Tcart[i][j];
    }
  /*----------------------------------------------------------------------*/
  t[0] = T[0][0];
  t[1] = T[0][1];
  t[2] = T[0][2];
  t[3] = T[1][1];
  t[4] = T[1][2];
  t[5] = T[2][2];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_kon_cacu */


/*----------------------------------------------------------------------*
 |                                                        m.gee 4/03    |
 | transform covariant components of a 2. Tensor from                   |
 | cartesian to curvilinear                                             |
 | storage mode is t[e11 e12 e13 e22 e23 e33]                           |
 | Must be called with covariant base vectors !                         |
 *----------------------------------------------------------------------*/
void s8_kov_cacu(DOUBLE *t, const DOUBLE **gkov)
{
  INT i, j;
  DOUBLE T[3][3], Tcart[3][3];
  /*
  DOUBLE c[3][3];
  */
  /*----------------------------------------------------------------------*/
  Tcart[0][0] = t[0];
  Tcart[1][0] = Tcart[0][1] = t[1];
  Tcart[2][0] = Tcart[0][2] = t[2];
  Tcart[1][1] = t[3];
  Tcart[2][1] = Tcart[1][2] = t[4];
  Tcart[2][2] = t[5];
  /*----------------------------------------------------------------------*/
  /* theory:
  for (k=0; k<3; k++)
  for (l=0; l<3; l++)
  {
     T[k][l] = 0.0;
     for (i=0; i<3; i++)
     for (j=0; j<3; j++)
        T[k][l] += gkov[i][k]*gkov[j][l]*Tcart[i][j];
  }
  */
  /*----------------------------------------------------------------------*/
  T[0][0] = 0.0;
  T[0][1] = 0.0;
  T[0][2] = 0.0;
  T[1][1] = 0.0;
  T[1][2] = 0.0;
  T[2][2] = 0.0;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    {
      T[0][0] += gkov[i][0] * gkov[j][0] * Tcart[i][j];
      T[0][1] += gkov[i][0] * gkov[j][1] * Tcart[i][j];
      T[0][2] += gkov[i][0] * gkov[j][2] * Tcart[i][j];
      T[1][1] += gkov[i][1] * gkov[j][1] * Tcart[i][j];
      T[1][2] += gkov[i][1] * gkov[j][2] * Tcart[i][j];
      T[2][2] += gkov[i][2] * gkov[j][2] * Tcart[i][j];
    }
  /*----------------------------------------------------------------------*/
  t[0] = T[0][0];
  t[1] = T[0][1];
  t[2] = T[0][2];
  t[3] = T[1][1];
  t[4] = T[1][2];
  t[5] = T[2][2];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_kov_cacu */


/*----------------------------------------------------------------------*
 |                                                        m.gee 5/03    |
 | transform contravariant components of a 4. Tensor from               |
 | cartesian to curvilinear                                             |
 | Must be called with contravariant base vectors base vectors !        |
 *----------------------------------------------------------------------*/
void s8_4kon_cacu(DOUBLE Ccart[][3][3][3], DOUBLE **gkon)
{
  INT i, j, k, l;
  INT m, n, p, q;
  DOUBLE C[3][3][3][3];
  /*----------------------------------------------------------------------*/
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      for (k = 0; k < 3; k++)
        for (l = 0; l < 3; l++)
        {
          C[i][j][k][l] = 0.0;
          for (m = 0; m < 3; m++)
            for (n = 0; n < 3; n++)
              for (p = 0; p < 3; p++)
                for (q = 0; q < 3; q++)
                  C[i][j][k][l] +=
                      Ccart[m][n][p][q] * gkon[m][i] * gkon[n][j] * gkon[p][k] * gkon[q][l];
        }
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      for (k = 0; k < 3; k++)
        for (l = 0; l < 3; l++) Ccart[i][j][k][l] = C[i][j][k][l];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_4kon_cacu */



/*----------------------------------------------------------------------*
 |                                                        m.gee 5/03    |
 *----------------------------------------------------------------------*/
void s8_c4_to_C2(DOUBLE C[][3][3][3], DOUBLE **CC)
{
  /*----------------------------------------------------------------------*/
  CC[0][0] = C[0][0][0][0];
  CC[0][1] = C[0][0][1][0];
  CC[0][2] = C[0][0][2][0];
  CC[0][3] = C[0][0][1][1];
  CC[0][4] = C[0][0][2][1];
  CC[0][5] = C[0][0][2][2];

  CC[1][0] = C[1][0][0][0];
  CC[1][1] = C[1][0][1][0];
  CC[1][2] = C[1][0][2][0];
  CC[1][3] = C[1][0][1][1];
  CC[1][4] = C[1][0][2][1];
  CC[1][5] = C[1][0][2][2];

  CC[2][0] = C[2][0][0][0];
  CC[2][1] = C[2][0][1][0];
  CC[2][2] = C[2][0][2][0];
  CC[2][3] = C[2][0][1][1];
  CC[2][4] = C[2][0][2][1];
  CC[2][5] = C[2][0][2][2];

  CC[3][0] = C[1][1][0][0];
  CC[3][1] = C[1][1][1][0];
  CC[3][2] = C[1][1][2][0];
  CC[3][3] = C[1][1][1][1];
  CC[3][4] = C[1][1][2][1];
  CC[3][5] = C[1][1][2][2];

  CC[4][0] = C[2][1][0][0];
  CC[4][1] = C[2][1][1][0];
  CC[4][2] = C[2][1][2][0];
  CC[4][3] = C[2][1][1][1];
  CC[4][4] = C[2][1][2][1];
  CC[4][5] = C[2][1][2][2];

  CC[5][0] = C[2][2][0][0];
  CC[5][1] = C[2][2][1][0];
  CC[5][2] = C[2][2][2][0];
  CC[5][3] = C[2][2][1][1];
  CC[5][4] = C[2][2][2][1];
  CC[5][5] = C[2][2][2][2];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_c4_to_C2 */


/*----------------------------------------------------------------------*
 |                                                        m.gee 5/03    |
 *----------------------------------------------------------------------*/
void s8_mat_linel_cart(STVENANT *mat, DOUBLE C[][3][3][3], DOUBLE **CC, DOUBLE *strain)
{
  DOUBLE emod, nue;
  DOUBLE l1, l2, ll2;
  /*
  DOUBLE e[3][3];
  */
  /*----------------------------------------------------------------------*/
  emod = mat->youngs;
  nue = mat->possionratio;
  l1 = (emod * nue) / ((1.0 + nue) * (1.0 - 2.0 * nue));
  l2 = emod / (2.0 * (1.0 + nue));
  ll2 = 2.0 * l2;
  /*----------------------------------------------------------------------*/
  /*
  e[0][0] = 1.0;
  e[1][0] = 0.0;
  e[2][0] = 0.0;
  e[0][1] = 0.0;
  e[1][1] = 1.0;
  e[2][1] = 0.0;
  e[0][2] = 0.0;
  e[1][2] = 0.0;
  e[2][2] = 1.0;
  */
  /*----------------------------------------------------------------------*/
  /*
  for (i=0; i<3; i++)
  for (j=0; j<3; j++)
  for (k=0; k<3; k++)
  for (l=0; l<3; l++)
  C[i][j][k][l] = l1*e[i][j]*e[k][l] + l2*( e[i][k]*e[j][l]+e[i][l]*e[k][j] );
  */
  /*----------------------------------------------------------------------*/
  C[0][0][0][0] = l1 + ll2;
  C[1][0][0][0] = 0.0;
  C[2][0][0][0] = 0.0;
  C[0][0][0][1] = 0.0;
  C[1][0][0][1] = l2;
  C[2][0][0][1] = 0.0;
  C[0][0][0][2] = 0.0;
  C[1][0][0][2] = 0.0;
  C[2][0][0][2] = l2;
  C[0][0][1][0] = 0.0;
  C[1][0][1][0] = l2;
  C[2][0][1][0] = 0.0;
  C[0][0][1][1] = l1;
  C[1][0][1][1] = 0.0;
  C[2][0][1][1] = 0.0;
  C[0][0][1][2] = 0.0;
  C[1][0][1][2] = 0.0;
  C[2][0][1][2] = 0.0;
  C[0][0][2][0] = 0.0;
  C[1][0][2][0] = 0.0;
  C[2][0][2][0] = l2;
  C[0][0][2][1] = 0.0;
  C[1][0][2][1] = 0.0;
  C[2][0][2][1] = 0.0;
  C[0][0][2][2] = l1;
  C[1][0][2][2] = 0.0;
  C[2][0][2][2] = 0.0;
  C[0][1][0][0] = 0.0;
  C[1][1][0][0] = l1;
  C[2][1][0][0] = 0.0;
  C[0][1][0][1] = l2;
  C[1][1][0][1] = 0.0;
  C[2][1][0][1] = 0.0;
  C[0][1][0][2] = 0.0;
  C[1][1][0][2] = 0.0;
  C[2][1][0][2] = 0.0;
  C[0][1][1][0] = l2;
  C[1][1][1][0] = 0.0;
  C[2][1][1][0] = 0.0;
  C[0][1][1][1] = 0.0;
  C[1][1][1][1] = l1 + ll2;
  C[2][1][1][1] = 0.0;
  C[0][1][1][2] = 0.0;
  C[1][1][1][2] = 0.0;
  C[2][1][1][2] = l2;
  C[0][1][2][0] = 0.0;
  C[1][1][2][0] = 0.0;
  C[2][1][2][0] = 0.0;
  C[0][1][2][1] = 0.0;
  C[1][1][2][1] = 0.0;
  C[2][1][2][1] = l2;
  C[0][1][2][2] = 0.0;
  C[1][1][2][2] = l1;
  C[2][1][2][2] = 0.0;
  C[0][2][0][0] = 0.0;
  C[1][2][0][0] = 0.0;
  C[2][2][0][0] = l1;
  C[0][2][0][1] = 0.0;
  C[1][2][0][1] = 0.0;
  C[2][2][0][1] = 0.0;
  C[0][2][0][2] = l2;
  C[1][2][0][2] = 0.0;
  C[2][2][0][2] = 0.0;
  C[0][2][1][0] = 0.0;
  C[1][2][1][0] = 0.0;
  C[2][2][1][0] = 0.0;
  C[0][2][1][1] = 0.0;
  C[1][2][1][1] = 0.0;
  C[2][2][1][1] = l1;
  C[0][2][1][2] = 0.0;
  C[1][2][1][2] = l2;
  C[2][2][1][2] = 0.0;
  C[0][2][2][0] = l2;
  C[1][2][2][0] = 0.0;
  C[2][2][2][0] = 0.0;
  C[0][2][2][1] = 0.0;
  C[1][2][2][1] = l2;
  C[2][2][2][1] = 0.0;
  C[0][2][2][2] = 0.0;
  C[1][2][2][2] = 0.0;
  C[2][2][2][2] = l1 + ll2;
  /*----------------------------------------------------------------------*/
  CC[0][0] = C[0][0][0][0];
  CC[0][1] = C[0][0][1][0];
  CC[0][2] = C[0][0][2][0];
  CC[0][3] = C[0][0][1][1];
  CC[0][4] = C[0][0][2][1];
  CC[0][5] = C[0][0][2][2];

  CC[1][0] = C[1][0][0][0];
  CC[1][1] = C[1][0][1][0];
  CC[1][2] = C[1][0][2][0];
  CC[1][3] = C[1][0][1][1];
  CC[1][4] = C[1][0][2][1];
  CC[1][5] = C[1][0][2][2];

  CC[2][0] = C[2][0][0][0];
  CC[2][1] = C[2][0][1][0];
  CC[2][2] = C[2][0][2][0];
  CC[2][3] = C[2][0][1][1];
  CC[2][4] = C[2][0][2][1];
  CC[2][5] = C[2][0][2][2];

  CC[3][0] = C[1][1][0][0];
  CC[3][1] = C[1][1][1][0];
  CC[3][2] = C[1][1][2][0];
  CC[3][3] = C[1][1][1][1];
  CC[3][4] = C[1][1][2][1];
  CC[3][5] = C[1][1][2][2];

  CC[4][0] = C[2][1][0][0];
  CC[4][1] = C[2][1][1][0];
  CC[4][2] = C[2][1][2][0];
  CC[4][3] = C[2][1][1][1];
  CC[4][4] = C[2][1][2][1];
  CC[4][5] = C[2][1][2][2];

  CC[5][0] = C[2][2][0][0];
  CC[5][1] = C[2][2][1][0];
  CC[5][2] = C[2][2][2][0];
  CC[5][3] = C[2][2][1][1];
  CC[5][4] = C[2][2][2][1];
  CC[5][5] = C[2][2][2][2];
  /*----------------------------------------------------------------------*/
  return;
} /* end of s8_mat_linel_cart */
