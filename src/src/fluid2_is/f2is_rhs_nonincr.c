/*
<pre>
Maintainer: Ulrich Küttler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/kuettler
            089 - 289-15238
</pre>
 */

#ifndef CCADISCRET
for (vi=0; vi<4; ++vi)
{
#ifdef FLUID2_IS_TERM1
    /* Konvektionsterm */
    eforce_(vi*3)     += timefacfac*funct_(vi)*conv_old_(0) ;
    eforce_(vi*3 + 1) += timefacfac*funct_(vi)*conv_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM2
    /* Stabilisierung der Konvektion ( L_conv_u) */
    eforce_(vi*3)     += 2.0*ttimetauM*conv_c_(vi)*conv_old_(0) ;
    eforce_(vi*3 + 1) += 2.0*ttimetauM*conv_c_(vi)*conv_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM3
    /* Stabilisierung der Konvektion (-L_visc_u) */
    eforce_(vi*3)     += -2.0*nu_*ttimetauM*conv_c_(vi)*visc_old_(0) ;
    eforce_(vi*3 + 1) += -2.0*nu_*ttimetauM*conv_c_(vi)*visc_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM4
    /* Stabilisierung der Konvektion ( L_pres_p) */
    eforce_(vi*3)     += ttimetauM*conv_c_(vi)*gradp_(0) ;
    eforce_(vi*3 + 1) += ttimetauM*conv_c_(vi)*gradp_(1) ;
#endif

#ifdef FLUID2_IS_TERM5
    /* Viskositätsterm */
#endif

#ifdef FLUID2_IS_TERM6
    /* Stabilisierung der Viskosität ( L_conv_u) */
    eforce_(vi*3)     += 2.0*nu_*ttimetauMp*(conv_old_(0)*viscs2_(0, 0, vi) + conv_old_(1)*viscs2_(0, 1, vi)) ;
    eforce_(vi*3 + 1) += 2.0*nu_*ttimetauMp*(conv_old_(0)*viscs2_(0, 1, vi) + conv_old_(1)*viscs2_(1, 1, vi)) ;
#endif

#ifdef FLUID2_IS_TERM7
    /* Stabilisierung der Viskosität (-L_visc_u) */
#endif

#ifdef FLUID2_IS_TERM8
    /* Stabilisierung der Viskosität ( L_pres_p) */
#endif

#ifdef FLUID2_IS_TERM9
    /* Druckterm */
#endif

#ifdef FLUID2_IS_TERM10
    /* Divergenzfreiheit */
#endif

#ifdef FLUID2_IS_TERM11
    /* Kontinuitätsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM12
    /* Massenterm */
#endif

#ifdef FLUID2_IS_TERM13
    /* Konvektionsstabilisierung */
    eforce_(vi*3)     += timetauM*conv_c_(vi)*velint_(0) ;
    eforce_(vi*3 + 1) += timetauM*conv_c_(vi)*velint_(1) ;
#endif

#ifdef FLUID2_IS_TERM14
    /* Viskositätsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM15
    /* Quellterm der rechten Seite */
    eforce_(vi*3)     += fac*funct_(vi)*rhsint_(0) ;
    eforce_(vi*3 + 1) += fac*funct_(vi)*rhsint_(1) ;
#endif

#ifdef FLUID2_IS_TERM16
    /* Konvektionsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM17
    /* Viskositätsstabilisierung */
    eforce_(vi*3)     += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 0, vi) + rhsint_(1)*viscs2_(0, 1, vi)) ;
    eforce_(vi*3 + 1) += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 1, vi) + rhsint_(1)*viscs2_(1, 1, vi)) ;
#endif

}

for (vi=4; vi<iel; ++vi)
{
#ifdef FLUID2_IS_TERM1
    /* Konvektionsterm */
    eforce_(vi*2 + 4) += timefacfac*funct_(vi)*conv_old_(0) ;
    eforce_(vi*2 + 5) += timefacfac*funct_(vi)*conv_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM2
    /* Stabilisierung der Konvektion ( L_conv_u) */
    eforce_(vi*2 + 4) += 2.0*ttimetauM*conv_c_(vi)*conv_old_(0) ;
    eforce_(vi*2 + 5) += 2.0*ttimetauM*conv_c_(vi)*conv_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM3
    /* Stabilisierung der Konvektion (-L_visc_u) */
    eforce_(vi*2 + 4) += -2.0*nu_*ttimetauM*conv_c_(vi)*visc_old_(0) ;
    eforce_(vi*2 + 5) += -2.0*nu_*ttimetauM*conv_c_(vi)*visc_old_(1) ;
#endif

#ifdef FLUID2_IS_TERM4
    /* Stabilisierung der Konvektion ( L_pres_p) */
    eforce_(vi*2 + 4) += ttimetauM*conv_c_(vi)*gradp_(0) ;
    eforce_(vi*2 + 5) += ttimetauM*conv_c_(vi)*gradp_(1) ;
#endif

#ifdef FLUID2_IS_TERM5
    /* Viskositätsterm */
#endif

#ifdef FLUID2_IS_TERM6
    /* Stabilisierung der Viskosität ( L_conv_u) */
    eforce_(vi*2 + 4) += 2.0*nu_*ttimetauMp*(conv_old_(0)*viscs2_(0, 0, vi) + conv_old_(1)*viscs2_(0, 1, vi)) ;
    eforce_(vi*2 + 5) += 2.0*nu_*ttimetauMp*(conv_old_(0)*viscs2_(0, 1, vi) + conv_old_(1)*viscs2_(1, 1, vi)) ;
#endif

#ifdef FLUID2_IS_TERM7
    /* Stabilisierung der Viskosität (-L_visc_u) */
#endif

#ifdef FLUID2_IS_TERM8
    /* Stabilisierung der Viskosität ( L_pres_p) */
#endif

#ifdef FLUID2_IS_TERM9
    /* Druckterm */
#endif

#ifdef FLUID2_IS_TERM10
    /* Divergenzfreiheit */
#endif

#ifdef FLUID2_IS_TERM11
    /* Kontinuitätsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM12
    /* Massenterm */
#endif

#ifdef FLUID2_IS_TERM13
    /* Konvektionsstabilisierung */
    eforce_(vi*2 + 4) += timetauM*conv_c_(vi)*velint_(0) ;
    eforce_(vi*2 + 5) += timetauM*conv_c_(vi)*velint_(1) ;
#endif

#ifdef FLUID2_IS_TERM14
    /* Viskositätsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM15
    /* Quellterm der rechten Seite */
    eforce_(vi*2 + 4) += fac*funct_(vi)*rhsint_(0) ;
    eforce_(vi*2 + 5) += fac*funct_(vi)*rhsint_(1) ;
#endif

#ifdef FLUID2_IS_TERM16
    /* Konvektionsstabilisierung */
#endif

#ifdef FLUID2_IS_TERM17
    /* Viskositätsstabilisierung */
    eforce_(vi*2 + 4) += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 0, vi) + rhsint_(1)*viscs2_(0, 1, vi)) ;
    eforce_(vi*2 + 5) += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 1, vi) + rhsint_(1)*viscs2_(1, 1, vi)) ;
#endif

}
#endif
