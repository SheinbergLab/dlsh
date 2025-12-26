# Sprite Sheet Package

Automatic collision polygon generation and sprite sheet processing for Tcl applications.

## Features

- Extract collision polygons from sprite transparency using alpha channel analysis
- Support for multiple formats:
  - **Aseprite JSON** (with animation tags, frame timing, and direction)
  - **Kenney XML** atlases
  - **TexturePacker XML** format
- Bayazit convex decomposition for Box2D compatibility
- Visual bounds computation for precise sizing and canonical dimensions
- Animation data extraction (frame sequences, FPS, direction)
- Frame rectangle and texture metadata extraction
- Configurable parameters for quality/performance tuning
- Load-time generation (no pre-processing required)
- Auto-format detection based on file extension

## Dependencies

Download these headers to `external/`:
- **stb_image.h** from https://github.com/nothings/stb (for PNG loading)
- **json.hpp** from https://github.com/nlohmann/json (for JSON parsing)
- **tinyxml2.h** and **tinyxml2.cpp** from https://github.com/leethomason/tinyxml2 (for XML parsing)

## Building
```bash
mkdir build && cd build
cmake .. -DTCL_INCLUDE_PATH=/path/to/tcl/include -DTCL_STUB_LIBRARY=/path/to/libtclstub9.0.a
make
```

## Usage

### Auto-Detect Format
```tcl
package require spritesheet

# Automatically detect JSON vs XML
set sheet_data [spritesheet::process "player.json" -epsilon 4.0]
set sheet_data [spritesheet::process "enemies.xml" -epsilon 2.5]
```

### Format-Specific Commands
```tcl
# Process Aseprite JSON (includes animations)
set json_data [spritesheet::process_aseprite "character.json" \
    -epsilon 4.0 \
    -min_area 30.0 \
    -max_vertices 6]

# Process XML atlas (Kenney, TexturePacker)
set xml_data [spritesheet::process_xml "tileset.xml" \
    -epsilon 6.0 \
    -min_area 50.0]

# Extract collision from single sprite frame
set frame_data [spritesheet::extract_collision "sprite.png" 0 0 64 64 \
    -threshold 128 \
    -epsilon 2.0]
```

### Integration with Tilemap
```tcl
package require yajl
package require spritesheet

# Load sprite sheet
set sheet_json [spritesheet::process "FierceTooth.json" -epsilon 4.0]
set sheet_dict [yajl::json2dict $sheet_json]

# Add to tilemap (loads texture, collision, animations)
tilemapAddSpriteSheet $tilemap "FierceTooth" $sheet_dict

# Create sprite from sheet
set sprite [tilemapCreateSpriteFromSheet $tilemap "FierceTooth" 100 200]

# Add physics with auto-generated collision
tilemapSpriteAddBody $tilemap $sprite dynamic

# Or use manual hitbox override for predictable physics
tilemapSpriteAddBody $tilemap $sprite dynamic \
    -hitbox_w 0.6 -hitbox_h 0.8

# Play animations (Aseprite JSON only)
tilemapSetSpriteAnimationByName $tilemap $sprite "FierceTooth" "Run"
```

## Parameters

- **`-threshold <0-255>`**: Alpha threshold for collision detection (default: 128)
- **`-epsilon <float>`**: Polygon simplification tolerance - higher = simpler shapes (default: 2.0)
- **`-min_area <float>`**: Minimum polygon area to keep (default: 10.0)
- **`-max_vertices <int>`**: Maximum vertices per convex polygon (default: 8)
- **`-pretty <bool>`**: Pretty-print JSON output for debugging

## Output Format

### Aseprite JSON Output
```json
{
  "frame_name": {
    "frame_rect": {"x": 0, "y": 0, "w": 32, "h": 32},
    "duration": 100,
    "fixtures": [
      {
        "vertices": [{"x": 5, "y": 2}, {"x": 28, "y": 3}, ...],
        "convex": true,
        "vertex_count": 5
      }
    ]
  },
  "_metadata": {
    "image": "sprite.png",
    "texture_width": 512,
    "texture_height": 256,
    "frame_count": 40,
    "canonical_canvas": {"w": 49, "h": 27},
    "animations": {
      "Idle": {
        "frames": [0, 1, 2, 3, 4, 5, 6, 7],
        "fps": 10.0,
        "direction": "forward"
      },
      "Run": {
        "frames": [8, 9, 10, 11, 12, 13],
        "fps": 1.09,
        "direction": "forward"
      }
    }
  }
}
```

### XML Output

Similar structure but without `duration` or `animations` (static atlases).

## Tips

### Collision Quality vs Performance

- **Characters**: `-epsilon 4.0 -min_area 30.0 -max_vertices 6` (detailed but optimized)
- **Tiles/Platforms**: `-epsilon 6.0 -min_area 50.0 -max_vertices 4` (simple boxes)
- **Items/Pickups**: `-epsilon 5.0 -min_area 40.0 -max_vertices 6` (moderate detail)

### Physics Predictability

For player characters and projectiles in physics simulations, use **manual hitbox override** instead of auto-generated collision to ensure symmetric, predictable behavior:
```tcl
# Auto-collision may be asymmetric (character leans left/right)
tilemapSpriteAddBody $tm $player dynamic  # Uses auto-collision

# Manual hitbox ensures balanced physics
tilemapSpriteAddBody $tm $player dynamic -hitbox_w 0.6 -hitbox_h 0.8
```

### Memory Savings

Trimmed sprite sheets (exported with "Trim Cels" in Aseprite) save ~95% texture memory compared to fixed-grid tilesets while maintaining precise collision.

## Example Workflow

1. **Create sprite in Aseprite** with animations
2. **Export as JSON** (File → Export → JSON Data)
   - Enable "Trim Cels" for memory savings
   - Include "Frame Tags" for animations
3. **Process with spritesheet package**:
```tcl
   set data [spritesheet::process "sprite.json" -epsilon 4.0]
```
4. **Load into tilemap** - automatic texture loading, collision, and animations
5. **Use in game/experiment** - physics-enabled sprites with animations

## License

MIT