
ZeroedEdges Node
================


An operator that enforces values close to zero at the domain edges.



![img](../../images/nodes/ZeroedEdges_settings.png)


## Category


Boundaries
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|dr|VirtualArray|Displacement relative to the domain size (radial direction).|
|input|VirtualArray|The input heightmap.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|The filtered heightmap result.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Amount|Float|No description|
|Center|Vec2Float|No description|
|Distance Axis|Enumeration|No description|
|Distance Function|Enumeration|Determines the method used for distance calculation. Options: Chebyshev, Euclidean, Euclidean/Chebyshev, Manhattan.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|Profile Parameter|Float|No description|
|Radial Profile|Enumeration|No description|
|Radius|Float|No description|

## Example


![img](../../images/nodes/ZeroedEdges_hsd_example.png)

Corresponding Hesiod file: [ZeroedEdges.hsd](../../examples/ZeroedEdges.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


