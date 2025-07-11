#
# simple example to show how to build a box2d world
#  taken from https://box2d.org/documentation/hello.html
#
package require box2d

# create the world
set world [box2d::createWorld]

# create the ground plane
box2d::createBox $world 0 0 -2 50 1

# create a dynamic box
set box [box2d::createBox $world 2 0 4 1 1]

# simulate
set timestep [expr {1/60.}]
for { set i 0 } { $i < 90 } { incr i } {
    box2d::step $world $timestep
    lassign [box2d::getBodyInfo $world $box] x y a 
    puts "$x $y $a"
}

