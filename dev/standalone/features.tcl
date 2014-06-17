proc CreateFoundPeaksData {} {
	global snack_list sn sn_peakgroup
	set sn_peakcnt   [lindex $snack_list 0]
	set sn(windowcnt) [lindex $snack_list 1]
	set sn(lofrq)     [lindex $snack_list 2]
	set sn(hifrq)     [lindex $snack_list 3]
	set snack_list [lrange $snack_list 4 end]
	set colourstep [expr 4.0 / double($sn_peakcnt)]	;#	THERE ARE ONLY 4 COLOR: USE THESE ECONOMICALLY TO COLOR THE peakcnt PEAKS
	set cnt 0
	set sn_peakgroupindex 0
	catch {unset sn_peakgroup}
	foreach item $snack_list {		;#	ASSUMES THESE ARE LINES OF VALUE TRIPLES ...time /frq/amp
		lappend sn_peakgroup($sn_peakgroupindex) $item
		incr cnt
		if {$cnt == $sn_peakcnt} {		;#	peakcnt OUTPUT BY c PROGRAM	
			set cnt 0
			incr sn_peakgroupindex		;#	EACH sn_peakgroup CONTAINS ALL THE TRIPLES OCCURING AT SAME TIME (THE peakcnt PEAKS) 
		}
	}
	set n 0
	while {$n <= $sn_peakgroupindex} {
		set sn_feature($n) [SortPeakGroup $sn_feature($n) $sn_peakcnt]	;#	SORT PEAKS IN THE GROUP, SO LOUDEST IS FIRST, QUIETIST LAST
		set thiscolor 4.0
		set color 4
		catch {unset nu_peakgroup}
		foreach {time freq amp} $sn_feature($n) {
			set time [expr int(round($time * $srate))]
			set color [expr int(ceil($thiscolor))]
			lappend nu_peakgroup $time $frq $color
			set thiscolor [expr $thiscolor - $colourstep]
		}
		set sn_feature($n) $nu_peakgroup
	}
}

proc DisplayFoundPeaks {} {
	set n 0
	while {$n <= $sn_peakgroupindex} {
		foreach {time freq color} $sn_feature($n) {
			set time [ConvertTimeToDisplayX $time]
			if {$time < 0} {
				continue
			}
			set freq [ConvertFreqToDisplayY $freq]
			.c create point $time $frq -fill magenta$color	;#	USES magenta1 to magenta4 .... possbily better to invert (i.e. m4 to m1)
		}
	}
}

proc SortPeakGroup {peakgroup peakcnt} {

	set peakcnt_less_one $peakcnt
	incr peakcnt_less_one -1
	set n 0
	while {$n < $peakcnt_less_one} {
		set peakn [lindex $peakgroup $n]
		set ampn [lindex $peakn 2]
		set m $n
		incr m
		while {$m < $peakcnt} {
			set peakm [lindex $peakgroup $m]
			set ampm [lindex $peakm 2]
			if {$ampm > $ampn} {
				set peakgroup [lreplace $preakgroup $n $n $peakm]
				set peakgroup [lreplace $preakgroup $m $m $peakn]
				set peakn $peakm
			}
			incr m
		}
		incr n
	}
	return $peakgroup
}

proc ConvertTimeToDisplayX {time} {
	global sn
	if {($time < $sn(dispstart)) || ($time > $sn(dispend)} {
		return -1
	}
	set ratio [expr double($time - $sn(dispstart)) / double($sn(displen))]
	set x [expr int(round($ratio * $sn(width)))]
	return $x
}

proc ConvertFreqToDisplayY {freq} {
	global sn
	set ratio [expr double($freq) / $sn_nyquist]
	set y [expr int(round($ratio * double($sn(height))))]
	set y [expr $sn(height) - $y]
	return $y
}
