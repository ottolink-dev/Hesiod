
MountainStump Node
==================


Generates a mountain-like heightmap with a flattened (stump-shaped) peak. Credit: Ian Fontova.



![img](../../images/nodes/MountainStump_settings.png)


## Category


Primitive/Geological
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|dx|VirtualArray|Displacement with respect to the domain size (x-direction).|
|dy|VirtualArray|Displacement with respect to the domain size (y-direction).|
|envelope|VirtualArray|Heightmap used as a post-process amplitude multiplier for the generated noise.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|out|VirtualArray|No description|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|add_deposition|Bool|No description|
|angle|Float|Expressed in degrees.|
|base_noise_amp|Float|No description|
|center|Vec2Float|No description Coordinates are in domain units; center.y is measured from the bottom of the map (y increases upward).|
|elevation|Float|No description|
|gamma|Float|No description|
|k_smoothing|Float|No description|
|Octaves|Integer|The number of octaves for fractal noise generation. More octaves add finer details to the terrain.|
|peak_kw|Float|No description|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|ridge_amp|Float|No description|
|rugosity|Float|No description|
|scale|Float|No description|
|Seed|Random seed number|Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.|

## Example


!!! note "No example yet"
    No example available for this node.  
