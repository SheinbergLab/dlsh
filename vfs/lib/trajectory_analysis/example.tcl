#!/usr/bin/env tclsh
# Example usage of trajectory_analysis module with dlsh

set dlshlib [file join /usr/local dlsh dlsh.zip]
if [file exists $dlshlib] {
    set base [file join [zipfs root] dlsh]
   zipfs mount $dlshlib $base
   set auto_path [linsert $auto_path [set auto_path 0] $base/lib]
}


# Load required packages
package require dlsh
package require trajectory_analysis

# Create sample trajectory data
# Each trajectory is a list of [x y] coordinate pairs
proc create_sample_trajectories {} {
    # Create a list of trajectories (ball falls with bounces)
    dl_local trajectories [dl_llist]
    
    # Generate 100 sample trajectories
    for {set i 0} {$i < 1000} {incr i} {
        dl_local xs [dl_flist]
        dl_local ys [dl_flist]
        
        # Starting position with some randomness
        set start_x [expr {50 + rand() * 100}]
        set start_y [expr {200 + rand() * 50}]
        set start_x {50}
        set start_y {200}
        
        # Simulate a ball falling with gravity and bounces
        set x $start_x
        set y $start_y
        set vx [expr {(rand() - 0.5) * 10}]  ;# Initial horizontal velocity
        set vy 0.0                            ;# Initial vertical velocity
        set gravity 9.8
        set dt 0.1
        set damping 0.8
        
        for {set t 0} {$t < 5.0} {set t [expr {$t + $dt}]} {
            # Add current position
            dl_append $xs $x
	    dl_append $ys $y
            
            # Update physics
            set vy [expr {$vy + $gravity * $dt}]
            set x [expr {$x + $vx * $dt}]
            set y [expr {$y + $vy * $dt}]
            
            # Bounce off ground
            if {$y <= 0} {
                set y 0
                set vy [expr {-$vy * $damping}]
                set vx [expr {$vx * $damping}]
            }
            
            # Bounce off walls
            if {$x <= 0 || $x >= 200} {
                set vx [expr {-$vx * $damping}]
                if {$x <= 0} {set x 0}
                if {$x >= 200} {set x 200}
            }
            
            # Stop if velocity is very low
            if {abs($vy) < 0.1 && abs($vx) < 0.1 && $y < 1} {
                break
            }
        }
        
        dl_append $trajectories [dl_llist $xs $ys]
    }
    
    dl_return $trajectories
}

# Generate sample data
puts "Creating sample trajectory data..."
set trajectories [create_sample_trajectories]

puts "Number of trajectories: [dl_length $trajectories]"

# Analyze trajectories to find collision hotspots
puts "Analyzing trajectories for collision hotspots..."
set result [trajectory_analyze $trajectories \
    -bandwidth_x 5.0 \
    -bandwidth_y 5.0 \
    -grid_size 40 \
    -threshold 0.005 \
    -angle_threshold 0.32]  ;# ~30 degrees

puts "Analysis complete. Result group: $result"

# Display results
puts "\nResults:"
puts "Peak locations (X coordinates): [dl_tcllist $result:peaks_x]"
puts "Peak locations (Y coordinates): [dl_tcllist $result:peaks_y]" 
puts "Peak intensities: [dl_tcllist $result:peak_values]"
puts "Grid info (xmin xmax ymin ymax width height): [dl_tcllist $result:grid_info]"

# Get the number of peaks found
set num_peaks [dl_length $result:peaks_x]
puts "\nFound $num_peaks collision hotspots"

# Show top 5 peaks by intensity
if {$num_peaks > 0} {
    puts "\nTop collision hotspots:"
    
    # Get peak data
    set peaks_x [dl_tcllist $result:peaks_x]
    set peaks_y [dl_tcllist $result:peaks_y]
    set peak_vals [dl_tcllist $result:peak_values]
    
    # Sort by intensity (simplified - in practice you'd want to sort all together)
    for {set i 0} {$i < [expr {min($num_peaks, 5)}]} {incr i} {
        set x [dl_get $result:peaks_x $i]
        set y [dl_get $result:peaks_y $i]
        set intensity [dl_get $result:peak_values $i]
        puts "  Peak [expr {$i+1}]: ($x, $y) intensity = $intensity"
    }
}

# Clean up
puts "\nCleaning up..."
dl_delete $trajectories
dg_delete $result

puts "Example complete!"
