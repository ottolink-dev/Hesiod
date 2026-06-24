
BasaltField Node
================


Generates a synthetic procedural terrain inspired by basaltic landforms. Credit: Ian Fontova.



![img](../../images/nodes/BasaltField_settings.png)


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
|out|VirtualArray|The generated heightmap from brush strokes.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|flatten_activate|Bool|No description|
|flatten_amp|Float|No description|
|flatten_kw_ratio|Float|No description|
|Spatial Frequency|Wavenumber|Base spatial frequencies in the X and Y directions. The frequencies are defined with respect to the entire domain: for example, kw = 2 produces two full oscillations across the domain width (and similarly for the Y direction).|
|large_scale_amp|Float|No description|
|large_scale_gain|Float|No description|
|large_scale_warp_amp|Float|No description|
|medium_scale_amp|Float|No description|
|medium_scale_gain|Float|No description|
|medium_scale_kw_ratio|Float|No description|
|medium_scale_warp_amp|Float|No description|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|rugosity_amp|Float|No description|
|rugosity_kw_ratio|Float|No description|
|Seed|Random seed number|Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.|
|small_scale_amp|Float|No description|
|small_scale_kw_ratio|Float|No description|
|small_scale_overlay_amp|Float|No description|
|warp_kw|Float|No description|

## Example


![img](../../images/nodes/BasaltField_hsd_example.png)

Corresponding Hesiod file: [BasaltField.hsd](../../examples/BasaltField.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
