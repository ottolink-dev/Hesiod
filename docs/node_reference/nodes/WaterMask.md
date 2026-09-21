
WaterMask Node
==============


Computes a gradient-based water mask using an extended water-depth model. This produces a smooth mask indicating regions that become flooded when water depth is artificially increased by a specified amount. It can also be used to generate a transition mask between water and ground, useful for effects such as texturing a sand beach.



![img](../../images/nodes/WaterMask_settings.png)


## Category


Hydrology
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|elevation|VirtualArray|Elevation map corresponding to the water depth grid.|
|water_depth|VirtualArray|Output water depth map representing flooded areas.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|mask|VirtualArray|Output normalized water-extension mask, ranging from 0 to 1.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Additional Water Depth|Float|Amount of additional water depth used to simulate how water would further spread over the terrain.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|

## Example


![img](../../images/nodes/WaterMask_hsd_example.png)

Corresponding Hesiod file: [WaterMask.hsd](../../examples/WaterMask.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


