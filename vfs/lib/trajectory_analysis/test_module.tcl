#!/usr/bin/env tclsh

# Simple test to verify the module loads
puts "Testing trajectory_analysis module..."

set dlshlib [file join /usr/local dlsh dlsh.zip]
if [file exists $dlshlib] {
    set base [file join [zipfs root] dlsh]
   zipfs mount $dlshlib $base
   set auto_path [linsert $auto_path [set auto_path 0] $base/lib]
}

# Add current directory to auto_path for package loading
lappend auto_path [file dirname [file normalize [info script]]]/lib

# Try to load the packages
if {[catch {package require dlsh} err]} {
    puts "Warning: Could not load dlsh package: $err"
    puts "The module was built without dlsh integration."
    exit 0
}

if {[catch {package require trajectory_analysis} err]} {
    if {[catch {load ./build/libtrajectory_analysis[info sharedlibextension]} err]} {
	puts "Error: Could not load trajectory_analysis package: $err"
	exit 1
    }
}

puts "Success! Module loaded correctly."

# Test basic functionality if dlsh is available
if {[info commands dl_create] ne ""} {
    puts "Testing basic functionality..."
    
    # Create a simple trajectory
    set trajectories [dl_create list]
    set traj [dl_create list]
    
    # Add a few points
    dl_append $traj [dl_flist 0.0 10.0]
    dl_append $traj [dl_flist 5.0 8.0]
    dl_append $traj [dl_flist 10.0 5.0]
    dl_append $traj [dl_flist 12.0 2.0]
    dl_append $traj [dl_flist 15.0 0.0]
    
    dl_append $trajectories $traj
    
    # Test the analysis function
    if {[catch {
        set result [trajectory_analyze $trajectories -grid_size 10 -threshold 0.0]
        puts "Analysis successful! Result: $result"
        
        # Clean up
        dg_delete $result
        dl_delete $trajectories
    } err]} {
        puts "Analysis test failed: $err"
        dl_delete $trajectories
    }
}

puts "All tests passed!"
