
Rift Node
=========


Rift is function used to represent a conceptualized rift.



![img](../../images/nodes/Rift_settings.png)


# Category


Primitive/Geological
# Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|dr|VirtualArray|No description|
|envelope|VirtualArray|Heightmap used as a post-process amplitude multiplier for the generated noise.|
|offset|VirtualArray|No description|

# Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Rift heightmap.|
|rift_mask|VirtualArray|No description|

# Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Angle|Float|Angle.|
|Axial Slope|Float|No description|
|Bottom Depth|Float|No description|
|Bottom Extent|Float|No description|
|Bottom Profile|Enumeration|No description|
|Bottom Profile Sharpness|Float|No description|
|Center|Vec2Float|Reference center within the heightmap.|
|Depth|Float|No description|
|Activate|Bool|No description|
|Spatial Frequency|Float|No description|
|Amplitude|Float|No description|
|Type|Enumeration|No description|
|Seed|Random seed number|No description|
|Smoothness|Float|No description|
|Outer Slope|Float|No description|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|Profile|Enumeration|No description|
|Profile Sharpness|Float|No description|
|Radius|Float|No description|
|Scale Radius with Depth|Bool|No description|

# Example


No example available.  
