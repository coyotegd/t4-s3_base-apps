# Maze Pattern Recognition & Wireframe Translation Strategy

## Overview
The maze is a 32x32 grid where 1=wall and 0=corridor. Each cell is uniform size.
Goal: Translate 2D maze patterns into 3D wireframe views that update smoothly as player moves.

## Pattern Recognition Layers

### Layer System (Relative to Player)
```
Layer 0: Current position (row, col)
Layer 1: 1 step ahead in facing direction
Layer 2: 2 steps ahead
Layer 3: 3 steps ahead
Layer 4: 4 steps ahead
Layer 5: 5 steps ahead
```

### Occupancy Grid (5x3 viewing window)
For each layer, check 3 positions: left, center, right

```
         LEFT    CENTER   RIGHT
Layer 5:  L5      C5       R5
Layer 4:  L4      C4       R4
Layer 3:  L3      C3       R3
Layer 2:  L2      C2       R2
Layer 1:  L1      C1       R1
Layer 0:  L0      C0       R0  (player position)
```

## Wireframe Translation Rules

### Screen Coordinate System
- Vanishing point: (160, 85) - center of view
- Inner throat: The nearest visible corridor opening
- Perspective depth: Geometric interpolation toward vanishing point

### Geometry by Pattern

#### Pattern A: Open Corridor (C1=0, L0=0, R0=0)
```
Wireframe elements:
- Left inner vertical (if L0=1)
- Right inner vertical (if R0=1)
- Top/bottom horizontals extending to edges
- Perspective lines to vanishing point (optional)
- Depth markers at layers 2,3,4 if corridors continue
```

#### Pattern B: Wall Ahead (C1=1)
```
Wireframe elements:
- Large rectangle (90% screen size, centered)
- Four perspective connectors from corners to vanishing point
- No inner verticals (view is blocked)
```

#### Pattern C: Left Wall Only (L0=1, R0=0, C1=0)
```
Wireframe elements:
- Left outer vertical (full height)
- Left-to-inner perspective trapezoid
- Inner throat opening (right side visible)
```

#### Pattern D: Right Wall Only (R0=1, L0=0, C1=0)
```
Wireframe elements:
- Right outer vertical (full height)
- Right-to-inner perspective trapezoid
- Inner throat opening (left side visible)
```

#### Pattern E: Tunnel (L0=1, R0=1, C1=0)
```
Wireframe elements:
- Both outer verticals
- Inner throat rectangle
- Perspective connectors creating depth
- Depth markers showing corridor continuation
```

## Movement Translation

### Forward Movement
```
Before: Layer N at depth D
After:  Layer N becomes Layer N-1 at depth D-1

Wireframe transformation:
- Far geometry scales up (moves toward camera)
- Near geometry disappears (passed behind camera)
- New far geometry appears from vanishing point
```

### Turn Left/Right
```
Before: Facing direction F
After:  Facing direction F±1

Wireframe transformation:
- Rotate entire occupancy grid
- Recalculate all relative positions
- Redraw from new perspective
```

## Implementation Recommendations

### 1. Structured Pattern Detection
```c
typedef struct {
    bool L5, C5, R5;  // Layer 5 (5 steps ahead)
    bool L4, C4, R4;  // Layer 4
    bool L3, C3, R3;  // Layer 3
    bool L2, C2, R2;  // Layer 2
    bool L1, C1, R1;  // Layer 1
    bool L0, C0, R0;  // Layer 0 (current)
} occupancy_pattern_t;

occupancy_pattern_t get_occupancy_pattern(void);
```

### 2. Geometry Generation
```c
typedef struct {
    int x, y;
} screen_point_t;

typedef struct {
    screen_point_t inner_tl, inner_tr, inner_bl, inner_br;  // Inner throat
    screen_point_t outer_tl, outer_tr, outer_bl, outer_br;  // Screen edges
    screen_point_t vanish;                                    // Vanishing point
} wireframe_geometry_t;

wireframe_geometry_t calculate_geometry(occupancy_pattern_t pattern);
```

### 3. Pattern-Based Rendering
```c
void draw_wireframe_for_pattern(occupancy_pattern_t pattern) {
    wireframe_geometry_t geom = calculate_geometry(pattern);
    
    // Draw based on pattern
    if (pattern.C1) {
        draw_wall_ahead(geom);
    } else {
        draw_corridor(geom, pattern);
    }
}
```

### 4. Depth Markers
Add vertical line pairs at interpolated depths when corridor continues:
```c
void draw_depth_markers(occupancy_pattern_t pattern) {
    if (!pattern.C1 && !pattern.C2) {  // Open to layer 2
        draw_vertical_pair_at_depth(0.25);  // 25% to vanishing point
    }
    if (!pattern.C1 && !pattern.C2 && !pattern.C3) {  // Open to layer 3
        draw_vertical_pair_at_depth(0.50);  // 50% to vanishing point
    }
    if (!pattern.C1 && !pattern.C2 && !pattern.C3 && !pattern.C4) {
        draw_vertical_pair_at_depth(0.75);  // 75% to vanishing point
    }
}
```

## Current Code Issues Identified

1. **Inconsistent pattern checking**: Some checks use absolute positions, others use relative
2. **Hard-coded geometry**: Magic numbers for throat dimensions
3. **Mode confusion**: "Strict" vs "Connect" modes create visual inconsistencies
4. **Missing smooth transitions**: No interpolation between movement steps

## Recommended Next Steps

1. **Implement structured occupancy detection** (5x3 grid)
2. **Create geometry calculator** based on patterns
3. **Add transition animations** (optional, for smooth movement)
4. **Simplify rendering modes** (remove Strict/Connect confusion)
5. **Add debug visualization** to show detected patterns

## Pattern Testing Checklist

Test each scenario at R9C8 facing North (known reference point):
- [ ] Open corridor (current view)
- [ ] After step forward (R8C8)
- [ ] After turn left (facing West)
- [ ] After turn right (facing East)
- [ ] Wall ahead scenarios
- [ ] T-junction patterns
- [ ] Corner patterns
- [ ] Long corridor depth rendering
