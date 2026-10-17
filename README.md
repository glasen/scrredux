# STUNT RACER

Stunt Car Racer inspired roller-coaster racer in a stadium. SDL3 + SDL_GPU, flat-shaded polygons, no textures.

    make            # builds ./stuntracer and the SPIR-V shaders (needs glslangValidator)
    ./stuntracer    # run from this directory (shaders are loaded from next to the binary)

Keys: arrows/WASD drive, SPACE boost, R reset to track, P pause, F11 fullscreen, ESC menu/quit.
Races are driven from the cockpit only, like the original.

* Renders at native window resolution with 4x MSAA (when supported).
* The car is a rigid body: 4 spring/damper suspensions with progressive hydraulic bump stops, tyre friction (traction circle),
  weight transfer, full 3D rotation, solid deck collisions, car-to-car contact. The opponent keeps clear of the player.
* Tracks are smooth closed splines with left and right bends through the infield, elevated on red/white girders,
  with a chequered start line. No barriers on the track edges.
* Cranes lower both cars onto the track before the start and put a car back after it falls off. A car that lands beside
  the track crashes in a cloud of dust and smoke (seen from outside) before the crane picks it up. Steel scraping the deck throws sparks.
* Stadium (`scenery.h`): roofed grandstands with a VIP tier, open end terraces, a crowd, sponsor boards, a scoreboard, lattice
  floodlights and a service gate; outside, car parks, trees, big-top tents and faceted mountains under drifting clouds.
* Arena floor: grass with mowing stripes, dirt worn under the track and a gravel service road to a compound with a recovery
  truck, water bowser, ambulance, TV truck, cabins and containers; TV towers, marshal posts, tyre stacks and hay bales around.
* The deck is weathered concrete in precast slabs, with rubber laid down in the bends.
* Sun shadows: two shadow-map cascades (near/far) cover track, towers, stands, trucks, cranes and cars.
* Shaders are SPIR-V only, so a Vulkan-capable driver is required.

Debug: `./stuntracer --sim SECS [TRACK]` (autopilot, prints lap/damage/contact stats),
`./stuntracer --shot out.png SECS [TRACK] [CAM]` (offscreen 1080p render; CAM 0 chase, 1 cockpit, 2 front, 3 side, 4 model viewer, 5 track overview).
`SR_DEBUG=1`, `SR_TRACE=1` add logging; `SR_PLANE=x` sets the autopilot's lane (outside +-6.9 it drives off the track),
`SR_WHEELS=1` reports the gap between the drawn tyres and the road, `SR_CAM=px,py,pz,tx,ty,tz` places the `--shot` camera freely.
`./stuntracer --menu-frames DIR SECS` records the menu scene at 60 fps as PNGs (`SR_NOHUD=1` hides the HUD).

## Car
A single-seat tube-frame buggy in the spirit of the original Stunt Car Racer car: roll cage, exposed V8 with eight chrome zoomie
pipes, independent double-wishbone suspension on all four wheels and a steering rig. It is loaded from `models/stuntcar.scm`
(~32k triangles, shared by both cars; paint is tinted per car), which `../stunt_car_racer/export_game_model.py` writes from the
low-poly Blender model (`models/stuntcar.blend`, also as `models/stuntcar.glb`); `car_model.h` loads it and poses wishbones,
uprights, coilovers and links from the physics' wheel travel and steering. The game does not start without `stuntcar.scm`.
Vehicle physics constants are in `phys_defaults()` (sr_main.h).

Model viewer: `./stuntracer --view`. Orbit with mouse drag / arrows, zoom with wheel or `+`/`-`, `Home` resets, `Space` toggles
the turntable, `Z` stops the animation (wheels straight, at ride height), `N` paint, `R` roll cage, `E` intake, `F` flames.
