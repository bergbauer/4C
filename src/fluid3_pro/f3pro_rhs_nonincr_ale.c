#ifndef CCADISCRET
for (vi=0; vi<iel; ++vi)
{
    /* Konvektionsterm (u*grad(u),v) */
    eforce_(vi*3)     += timefacfac*funct_(vi)*conv_old_(0) ;
    eforce_(vi*3 + 1) += timefacfac*funct_(vi)*conv_old_(1) ;
    eforce_(vi*3 + 2) += timefacfac*funct_(vi)*conv_old_(2) ;

    /* Stabilisierung der Konvektion (u*grad(u),u*grad(v)) */
    eforce_(vi*3)     += ttimetauM*(2.0*conv_c_(vi)*conv_old_(0) - 2.0*velint_(0)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(0, 0) - velint_(0)*gridvint_(1)*derxyz_(0, vi)*vderxyz_(0, 1) - velint_(0)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(0, 0) - velint_(1)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(0, 1) - velint_(1)*gridvint_(0)*derxyz_(1, vi)*vderxyz_(0, 0) - velint_(0)*gridvint_(2)*derxyz_(0, vi)*vderxyz_(0, 2) - velint_(0)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(0, 0) - velint_(2)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(0, 2) - velint_(2)*gridvint_(0)*derxyz_(2, vi)*vderxyz_(0, 0) - 2.0*velint_(1)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(0, 1) - velint_(1)*gridvint_(2)*derxyz_(1, vi)*vderxyz_(0, 2) - velint_(1)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(0, 1) - velint_(2)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(0, 2) - velint_(2)*gridvint_(1)*derxyz_(2, vi)*vderxyz_(0, 1) - 2.0*velint_(2)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(0, 2)) ;
    eforce_(vi*3 + 1) += ttimetauM*(2.0*conv_c_(vi)*conv_old_(1) - 2.0*velint_(0)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(1, 0) - velint_(0)*gridvint_(1)*derxyz_(0, vi)*vderxyz_(1, 1) - velint_(0)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(1, 0) - velint_(1)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(1, 1) - velint_(1)*gridvint_(0)*derxyz_(1, vi)*vderxyz_(1, 0) - velint_(0)*gridvint_(2)*derxyz_(0, vi)*vderxyz_(1, 2) - velint_(0)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(1, 0) - velint_(2)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(1, 2) - velint_(2)*gridvint_(0)*derxyz_(2, vi)*vderxyz_(1, 0) - 2.0*velint_(1)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(1, 1) - velint_(1)*gridvint_(2)*derxyz_(1, vi)*vderxyz_(1, 2) - velint_(1)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(1, 1) - velint_(2)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(1, 2) - velint_(2)*gridvint_(1)*derxyz_(2, vi)*vderxyz_(1, 1) - 2.0*velint_(2)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(1, 2)) ;
    eforce_(vi*3 + 2) += ttimetauM*(2.0*conv_c_(vi)*conv_old_(2) - 2.0*velint_(0)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(2, 0) - velint_(0)*gridvint_(1)*derxyz_(0, vi)*vderxyz_(2, 1) - velint_(0)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(2, 0) - velint_(1)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(2, 1) - velint_(1)*gridvint_(0)*derxyz_(1, vi)*vderxyz_(2, 0) - velint_(0)*gridvint_(2)*derxyz_(0, vi)*vderxyz_(2, 2) - velint_(0)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(2, 0) - velint_(2)*gridvint_(0)*derxyz_(0, vi)*vderxyz_(2, 2) - velint_(2)*gridvint_(0)*derxyz_(2, vi)*vderxyz_(2, 0) - 2.0*velint_(1)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(2, 1) - velint_(1)*gridvint_(2)*derxyz_(1, vi)*vderxyz_(2, 2) - velint_(1)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(2, 1) - velint_(2)*gridvint_(1)*derxyz_(1, vi)*vderxyz_(2, 2) - velint_(2)*gridvint_(1)*derxyz_(2, vi)*vderxyz_(2, 1) - 2.0*velint_(2)*gridvint_(2)*derxyz_(2, vi)*vderxyz_(2, 2)) ;

    /* Stabilisierung der Konvektion (-2*nu*div(epsilon((u))),u*grad(v)) */
    eforce_(vi*3)     += -2.0*visc_*ttimetauM*conv_c_(vi)*visc_old_(0) ;
    eforce_(vi*3 + 1) += -2.0*visc_*ttimetauM*conv_c_(vi)*visc_old_(1) ;
    eforce_(vi*3 + 2) += -2.0*visc_*ttimetauM*conv_c_(vi)*visc_old_(2) ;

    /* Stabilisierung der Konvektion (grad(p),u*grad(v)) */
    eforce_(vi*3)     += -(ttimetauM*conv_g_(vi)*gradp_(0)) ;
    eforce_(vi*3 + 1) += -(ttimetauM*conv_g_(vi)*gradp_(1)) ;
    eforce_(vi*3 + 2) += -(ttimetauM*conv_g_(vi)*gradp_(2)) ;

    /* Viskositätsterm (2*nu*epsilon(u),epsilon(v)) */

    /* Stabilisierung der Viskosität (u*grad(u),-2*nu*div(epsilon((v)))) */
    eforce_(vi*3)     += 2.0*visc_*ttimetauMp*(conv_old_(0)*viscs2_(0, 0, vi) + conv_old_(1)*viscs2_(0, 1, vi) + conv_old_(2)*viscs2_(0, 2, vi)) ;
    eforce_(vi*3 + 1) += 2.0*visc_*ttimetauMp*(conv_old_(0)*viscs2_(0, 1, vi) + conv_old_(1)*viscs2_(1, 1, vi) + conv_old_(2)*viscs2_(1, 2, vi)) ;
    eforce_(vi*3 + 2) += 2.0*visc_*ttimetauMp*(conv_old_(0)*viscs2_(0, 2, vi) + conv_old_(1)*viscs2_(1, 2, vi) + conv_old_(2)*viscs2_(2, 2, vi)) ;

    /* Stabilisierung der Viskosität (-2*nu*div(epsilon((u))),-2*nu*div(epsilon((v)))) */

    /* Stabilisierung der Viskosität (grad(p),-2*nu*div(epsilon((v)))) */
    eforce_(vi*3)     += -2.0*visc_*ttimetauMp*(gradp_(0)*viscs2_(0, 0, vi) + gradp_(1)*viscs2_(0, 1, vi) + gradp_(2)*viscs2_(0, 2, vi)) ;
    eforce_(vi*3 + 1) += -2.0*visc_*ttimetauMp*(gradp_(0)*viscs2_(0, 1, vi) + gradp_(1)*viscs2_(1, 1, vi) + gradp_(2)*viscs2_(1, 2, vi)) ;
    eforce_(vi*3 + 2) += -2.0*visc_*ttimetauMp*(gradp_(0)*viscs2_(0, 2, vi) + gradp_(1)*viscs2_(1, 2, vi) + gradp_(2)*viscs2_(2, 2, vi)) ;

    /* Kontinuitätsstabilisierung */

    /* Massenterm (u,v) */

    /* Konvektionsstabilisierung (u,u*grad(v)) */
    eforce_(vi*3)     += timetauM*conv_c_(vi)*velint_(0) ;
    eforce_(vi*3 + 1) += timetauM*conv_c_(vi)*velint_(1) ;
    eforce_(vi*3 + 2) += timetauM*conv_c_(vi)*velint_(2) ;

    /* Viskositätsstabilisierung (u,-2*nu*div(epsilon((v)))) */

    /* Quellterm der rechten Seite (b,v) */
    eforce_(vi*3)     += fac*funct_(vi)*rhsint_(0) ;
    eforce_(vi*3 + 1) += fac*funct_(vi)*rhsint_(1) ;
    eforce_(vi*3 + 2) += fac*funct_(vi)*rhsint_(2) ;

    /* Konvektionsstabilisierung (b,u*grad(v)) */
    eforce_(vi*3)     += timetauM*conv_g_(vi)*rhsint_(0) ;
    eforce_(vi*3 + 1) += timetauM*conv_g_(vi)*rhsint_(1) ;
    eforce_(vi*3 + 2) += timetauM*conv_g_(vi)*rhsint_(2) ;

    /* Viskositätsstabilisierung (b,-2*nu*div(epsilon((v))) */
    eforce_(vi*3)     += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 0, vi) + rhsint_(1)*viscs2_(0, 1, vi) + rhsint_(2)*viscs2_(0, 2, vi)) ;
    eforce_(vi*3 + 1) += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 1, vi) + rhsint_(1)*viscs2_(1, 1, vi) + rhsint_(2)*viscs2_(1, 2, vi)) ;
    eforce_(vi*3 + 2) += tau_Mp*time2nue*(rhsint_(0)*viscs2_(0, 2, vi) + rhsint_(1)*viscs2_(1, 2, vi) + rhsint_(2)*viscs2_(2, 2, vi)) ;

}
#endif
