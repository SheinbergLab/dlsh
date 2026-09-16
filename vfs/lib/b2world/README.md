# b2world

A headless Box2D world of **tagged bodies given as data**: the generic
engine under paradigm physics libraries (`sling_sim` in `~/systems/ess/lib`
is the first). It knows nothing about slingshots or buckets. A world is a
dict of bodies, each with a name, shape, type and a list of **role** tags; a
run launches one body and steps until a **stop rule** fires. Paradigm content
(what the bodies are, which contact means what) stays with the paradigm, so
a new obstacle kind, target shape or moving body is data, not a module edit.

```tcl
package require b2world
set w [b2world::default_spec]                       ;# gravity dt max_t bounds bodies
b2world::add_body w [b2world::body ground box static 0 -8.5 w 40 h 1 roles {ground}]
b2world::add_body w [b2world::body pad box static 3 -5 w 2 h 0.3 roles {target hit}]
b2world::add_body w [b2world::body ball circle dynamic -9 -3 r 0.4 roles {projectile}]
set r [b2world::simulate $w -launch {ball 12 8} \
           -stop {{contact projectile hit hit} {contact projectile ground miss}} \
           -record ball]
dict get $r outcome            ;# hit | miss | out | timeout | <stop_proc's word>
dict get $r paths ball         ;# {t {...} x {...} y {...}}
b2world::contacts_of $r ball   ;# {t other} ...
```

## Spec

| key | meaning |
|---|---|
| `gravity` | negative down; the box2d world's own is −10, any other value is honoured by a per-step force on every dynamic body |
| `dt`, `max_t` | step and cap (s) |
| `bounds` | `{x_lo x_hi y_lo y_hi}`; a tracked body outside → `out` |
| `bodies` | list of body dicts from `b2world::body name shape type x y ?key val…?` |

Body keys: `w h` (box) or `r` (circle), `angle` (deg), `restitution` (unset
means Box2D's default of 0; Box2D mixes restitution by taking the max of the
two bodies in a contact), `sensor`, `roles`, `force {fx fy}` on a sensor box
(a force zone acting on tracked dynamic bodies inside it), and for kinematic
bodies `path`:
`{kind linear vx vy}` or `{kind oscillate ax ay period phase_deg}`. Paths are
velocity-driven, so contacts see the body's motion; `path_position` gives the
same motion analytically for a renderer.

## simulate options

`-launch {name vx vy}`, `-stop rules` (`{contact ROLE_A ROLE_B OUTCOME}` or
`{cross ROLE x|y VALUE up|down OUTCOME}`, first match wins; contacts are
tested before bounds, then crossings), `-stop_proc name` (called
`name world t positions`, a non-empty return is the outcome), `-record names`,
`-track names` (bounds, crossings and force zones; default = record), `-dt`,
`-max_t`, `-stride n`.

`b2world::sweep spec cases launch_proc ?options?` runs one simulate per case.

Units are the caller's (degrees of visual angle in ESS); Box2D treats them as
meters, fine at these scales. Determinism: the same spec and launch give the
same path, which is what lets a preview *be* the flight.

Test: `dlsh test_b2world.tcl` (sources the on-disk copy).
