# AUTHORS
#   REHBM
#
# DESCRIPTION
#    the main proc here is ::robust::robust_regression
#    ref: Wilcox (1997). Introduction to robust estimation and hypothesis testing. San Diego: Academic Press.

package provide robust 1.0

namespace eval ::robust {
    proc robust_regression { xs ys } {
	# %HELP%
	# ::robust::regression <xs> <ys>
	#   calculates the biweight midcorrelation coefficient
	#   Reference: Wilcox (1997). Introduction to robust estimation and hypothesis testing. San Diego: Academic Press.
	# %END%

	# check to make sure there is data
	if {![dl_length $xs] || ![dl_length $ys]} {
	    return 0.0
	} else {
	    return [::helprobust::bicor $xs $ys]
	}
    }
}
namespace eval ::helprobust {
    proc bicor { x y } {
	#### Calculate biweight midcorrelation.
	set bi_x [bicov $x $x]
	set bi_y [bicov $y $y]
	if {!$bi_x || !$bi_y} {
	    # then there is no variation across one of the varibles, so there is no correlation
	    set bicor 0.0
	} else {
	    set bicor [expr [bicov $x $y] / (sqrt($bi_x * $bi_y))]; # bicov(x,y)/(sqrt(bicov(x,x)*bicov(y,y)))
	}
	return $bicor
    }
    proc bicov { x y } {
	#### Calculate biweight midcovariance,  
	#### from Wilcox (1997).

	# debugging
	#dl_set x $x
	#dl_set y $y
	
	set mx [dl_median $x]
	set my [dl_median $y]

	set ux [dl_abs [dl_div [dl_sub $x $mx] [dl_mult 9 [qnorm 0.75] [mad $x]]]]; # ux <- abs((x - mx)/(9 * qnorm(0.75) * mad(x)))
	set uy [dl_abs [dl_div [dl_sub $y $my] [dl_mult 9 [qnorm 0.75] [mad $y]]]]; # uy <- abs((y - my)/(9 * qnorm(0.75) * mad(y)))
	
	set aval [dl_lte $ux 1]; # aval <- ifelse(ux <= 1, 1, 0)
	set bval [dl_lte $uy 1]; # bval <- ifelse(uy <= 1, 1, 0)

	set top [dl_sum [dl_mult $aval [dl_sub $x $mx] [dl_pow [dl_sub 1 [dl_pow $ux 2]] 2] $bval [dl_sub $y $my] [dl_pow [dl_sub 1 [dl_pow $uy 2]] 2]]]; # top <- sum(aval * (x - mx) * (1 - ux^2)^2 * bval * (y - my) * (1 - uy^2)^2)
	set top [expr [dl_length $x] * $top]; # top <- length(x) * top
	
	set botx [dl_sum [dl_mult $aval [dl_sub 1 [dl_pow $ux 2]] [dl_sub 1 [dl_mult 5 [dl_pow $ux 2]]]]]; # botx <- sum(aval * (1 - ux^2) * (1 - 5 * ux^2))
	set boty [dl_sum [dl_mult $bval [dl_sub 1 [dl_pow $uy 2]] [dl_sub 1 [dl_mult 5 [dl_pow $uy 2]]]]]; # boty <- sum(bval * (1 - uy^2) * (1 - 5 * uy^2))
	set bi [expr $top / ($botx * $boty)]
	return $bi
    }

    proc mad { x {constant 1.4826} } {
	# median absolute deviation of vector x
	# MADx = median { |x1-Mx|, ..., |XN-Mx|}
	#    where Mx is median of x
	#
	# algorithm also compared to R's mad().  From this I added the constant. Below is from the R documentation for mad():
	#    The default constant = 1.4826 (approximately 1/ Phi^(-1)(3/4) = 1/qnorm(3/4)) ensures consistency, i.e., 
	#         E[mad(X_1,...,X_n)] = s
	#    for X_i distributed as N(µ,s^2) and large n.
	#
	# TODO - convert to dl_mad, add dl_mads (vectorized) functionality, add to dlbasic.tcl
	set mad [expr $constant * [dl_median [dl_abs [dl_sub $x [dl_median $x]]]]]
    }

    proc qnorm { p } {
	# not sure what this does.  normal quantiles from percentage?
	set p [expr 1.0*$p]
	set split 0.42;
	set a0  2.50662823884;
	set a1 -18.61500062529;
	set a2  41.39119773534;
	set a3 -25.44106049637;
	set b1 -8.47351093090;
	set b2  23.08336743743;
	set b3 -21.06224101826;
	set b4  3.13082909833;
	set c0 -2.78718931138;
	set c1 -2.29796479134;
	set c2  4.85014127135;
	set c3  2.32121276858;
	set d1  3.54388924762;
	set d2  1.63706781897;
	set q [expr $p-0.5]

	if { abs($q) <= $split } {
	    set r [expr $q*$q];
	    set ppnd [expr $q * ((($a3*$r+$a2)*$r+$a1)*$r+$a0)/(((($b4*$r+$b3)*$r+$b2)*$r+$b1)*$r+1)];
	} else {
	    set r $p;
	    if {$q > 0} {
		set r [expr 1-$p];
	    }
	    if {$r > 0} {
		set r [expr sqrt(log($r))]; # r=Math.sqrt(-Math.log(r));
		set ppnd [expr ((($c3*$r+$c2)*$r+$c1)*$r+$c0)/(($d2*$r+$d1)*$r+1)];
		if {$q < 0} {
		    set ppnd [expr -1* $ppnd]
		}
	    } else {
		set ppnd 0;
	    }
	}
	return $ppnd
    }

    proc mvrnorm { n u s } {
	#     #### Sample from multivariate normal - this is just the test data

	#     mvrnorm <- function(n, p, u, s, S) {
	# 	Z <- matrix(rnorm(n * p), p, n)
	# 	t(u + s*t(chol(S)) %*% Z)}
	
	#     z<-mvrnorm(n=100, p=2, u=c(100,100), s=c(15,15), 
	# 	       S=matrix(c(1, .4, .4, 1), ncol=2, nrow=2, 
	# 			byrow=T))
	dl_local z [dl_add [dl_mult [dl_zrand $n] $s] $u]
	dl_local noise [dl_mult [dl_add [dl_mult [dl_urand $n] $s] $u] 2]

	dl_local z2 [dl_add [dl_add [dl_mult [dl_div [dl_sub $z $s] $u] 25] 200] $noise]

	dl_return [dl_llist $z $z2]
    }
}

# proc test_robust_regression { } {
#     dl_local z [mvrnorm 100 100 15]
   
#     # Pearson Correlation and Robust Correlation
#     # before outlier added
#     set rp  [dl_first [dl_pearson $z:0 $z:1]]; # cor(z[,1], z[,2])
#     set rbc [bicor $z:0 $z:1];                 # bicor(z[,1], z[,2])
    
#     clearwin
#     dlp_setpanels 2 2
#     dlp_subplot [dlp_scatter $z:0 $z:1] 0
    
#     # Add outlier to Y vector for smallest X value 
#     # z[z[,1]==min(z[,1]),2]<-z[z[,1]==min(z[,1]),2]*2.5
#     dl_append $z:0 [dl_min $z:0]
#     dl_append $z:1 [expr 2.5 * [dl_first $z:1]]
    
#     dlp_subplot [dlp_scatter $z:0 $z:1] 1
    
#     # Pearson Correlation and Robust Correlation 
#     # after outlier added
#     set rp_out  [dl_first [dl_pearson $z:0 $z:1]]; # cor(z[,1], z[,2])
#     set rbc_out [bicor $z:0 $z:1];                 # bicor(z[,1], z[,2])
    
#     puts "Normal Data:        rp = $rp     rbc = $rbc"
#     puts "With Outlier:       rp = $rp_out     rbc = $rbc_out"
# }
