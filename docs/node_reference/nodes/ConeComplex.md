
ConeComplex Node
================


Generates a complex conical heightfield with valleys, directional bias, and radial waviness.



![img](../../images/nodes/ConeComplex_settings.png)


## Category


Primitive/Function
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|control|VirtualArray|Optional control map modulating local valley amplitude.|
|dx|VirtualArray|Displacement with respect to the domain size (x-direction).|
|dy|VirtualArray|Displacement with respect to the domain size (y-direction).|
|envelope|VirtualArray|Heightmap used as a post-process amplitude multiplier for the generated noise.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Generated conical heightfield.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|alpha|Float|Exponent controlling the steepness of the cone slope.|
|bias_amp|Float|Amplitude of the directional bias effect.|
|bias_angle|Float|Direction of the slope bias in degrees.|
|bias_exponent|Float|Controls how bias strength varies with radius.|
|center|Vec2Float|Center position of the cone in world coordinate space. Coordinates are in domain units; center.y is measured from the bottom of the map (y increases upward).|
|erosion_delta|Float|Smoothing parameter for the erosion profile function.|
|erosion_profile|Enumeration|Erosion profile type used to shape the valley cross-sections.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|radial_waviness_amp|Float|Amplitude of radial sinusoidal perturbations (coastal waviness).|
|radial_waviness_kw|Float|Frequency multiplier controlling the rate of radial waviness.|
|radius|Float|Effective radius of the cone in coordinate space units. Specified in domain units (same scale as center); on a non-square map the footprint is therefore elliptical, since x and y span different physical extents.|
|smooth_profile|Bool|No description|
|valley_amp|Float|Global amplitude of valley depressions.|
|valley_angle0|Float|Angular offset of the first valley in degrees.|
|valley_decay_ratio|Float|Controls how valley amplitude decays toward the center.|
|valley_nb|Integer|Number of valleys arranged radially around the cone.|

## Example


!!! note "No example yet"
    No example available for this node.  
