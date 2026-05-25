
Clamp Node
==========


Clamp restrict a value within a specified range. Essentially, it ensures that a value does not exceed a defined upper limit or fall below a defined lower limit.



![img](../../images/nodes/Clamp_settings.png)


# Category


Filter/Range
# Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap.|

# Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Clamped heightmap.|

# Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Clamp|Value range|Clamping range.|
|K Max|Float|Upper bound smoothing intensity.|
|K Min|Float|Lower bound smoothing intensity.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|Remap|Bool|Remap the operator's output to a specified range, defaulting to [0, 1].|
|Smooth Max|Bool|Activate smooth clamping for the upper bound.|
|Smooth Min|Bool|Activate smooth clamping for the lower bound.|

# Example


No example available.  
